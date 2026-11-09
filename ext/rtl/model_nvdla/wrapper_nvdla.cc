/* Some code is based on nvdla.cpp which has:
* nvdla.cpp
* Driver for Verilator testbench
* NVDLA Open Source Project
*
* Copyright (c) 2017 NVIDIA Corporation.  Licensed under the NVDLA Open
* Hardware License.  For more information, see the "LICENSE" file that came
* with this distribution.
*
* Copyright (c) 2022 Barcelona Supercomputing Center
* All rights reserved.
*
* The license below extends only to copyright in the software and shall
* not be construed as granting a license to any other intellectual
* property including but not limited to intellectual property relating
* to a hardware implementation of the functionality of the software
* licensed hereunder.  You may use the software subject to the license
* terms below provided that you ensure that this notice is replicated
* unmodified and in its entirety in all distributions of the software,
* modified or unmodified, in source code or in binary form.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met: redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer;
* redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution;
* neither the name of the copyright holders nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* Authors: Guillem Lopez Paradis
*/

#include "wrapper_nvdla.hh"
#include <iostream>
#ifdef NVDLA_RTL_SAVABLE
#include "verilated_save.h"
#endif
uint64_t _tickcount = 0;

double sc_time_stamp() {
  return double_t(_tickcount);
}

embeddedBuffer* Wrapper_nvdla::shared_spm = nullptr;
uint64_t* Wrapper_nvdla::print_buffer = nullptr;
uint32_t Wrapper_nvdla::buf_ptr = 0;

Wrapper_nvdla::Wrapper_nvdla(int id_nvdla, const unsigned int maxReq,
                             bool _dma_enable, int _spm_latency, int _spm_line_size, int _spm_line_num,
                             bool pft_enable, bool use_shared_spm, BufferMode mode, uint32_t _assoc) :
        id_nvdla(id_nvdla),
        tickcount(0),
        prefetch_enable(pft_enable),
        use_shared_spm(use_shared_spm),
        buf_mode(mode),
        assoc(_assoc) {
    if (use_shared_spm && shared_spm) {
        spm = shared_spm;
    } else {
        switch (mode) {
            case BUF_MODE_ALL:
                spm = new allBuffer(this, _spm_latency, _spm_line_size, _spm_line_num, _assoc);
                break;
            case BUF_MODE_PFT:
            case BUF_MODE_PFT_CUTOFF:
                spm = new prefetchBuffer(this, _spm_latency, _spm_line_size, _spm_line_num, _assoc);
                break;
            default:
                assert(false);
        }

        if (use_shared_spm) shared_spm = spm;
    }

    if (!print_buffer) {
        print_buffer = new uint64_t[PB_SIZE * 2];
    }


    int argcc = 1;
    char* buf[] = {(char*)"aaa",(char*)"bbb"};
    Verilated::commandArgs(argcc, buf);

    dla = new VNV_nvdla();

    // we always enable the trace
    // but we will use it depending on traceOn
    // otherwise this function launch an error
    Verilated::traceEverOn(false);

    // CSB Wrapper
    csb = new CSBMaster(dla, this);

    // AXI DBBIF
    AXIResponder::connections dbbconn = {
        .aw_awvalid = &dla->nvdla_core2dbb_aw_awvalid,
        .aw_awready = &dla->nvdla_core2dbb_aw_awready,
        .aw_awid = &dla->nvdla_core2dbb_aw_awid,
        .aw_awlen = &dla->nvdla_core2dbb_aw_awlen,
        .aw_awaddr = &dla->nvdla_core2dbb_aw_awaddr,

        .w_wvalid = &dla->nvdla_core2dbb_w_wvalid,
        .w_wready = &dla->nvdla_core2dbb_w_wready,
#ifndef NV_SMALL_EN
        .w_wdata = dla->nvdla_core2dbb_w_wdata,
#else
        .w_wdata = &dla->nvdla_core2dbb_w_wdata,
#endif
        .w_wstrb = &dla->nvdla_core2dbb_w_wstrb,
        .w_wlast = &dla->nvdla_core2dbb_w_wlast,

        .b_bvalid = &dla->nvdla_core2dbb_b_bvalid,
        .b_bready = &dla->nvdla_core2dbb_b_bready,
        .b_bid = &dla->nvdla_core2dbb_b_bid,

        .ar_arvalid = &dla->nvdla_core2dbb_ar_arvalid,
        .ar_arready = &dla->nvdla_core2dbb_ar_arready,
        .ar_arid = &dla->nvdla_core2dbb_ar_arid,
        .ar_arlen = &dla->nvdla_core2dbb_ar_arlen,
        .ar_araddr = &dla->nvdla_core2dbb_ar_araddr,

        .r_rvalid = &dla->nvdla_core2dbb_r_rvalid,
        .r_rready = &dla->nvdla_core2dbb_r_rready,
        .r_rid = &dla->nvdla_core2dbb_r_rid,
        .r_rlast = &dla->nvdla_core2dbb_r_rlast,
#ifndef NV_SMALL_EN
        .r_rdata = dla->nvdla_core2dbb_r_rdata,
#else
        .r_rdata = &dla->nvdla_core2dbb_r_rdata,
#endif
    };
    axi_dbb = new AXIResponder(dbbconn, this, "DBB",
              false, maxReq, _dma_enable);

    // AXI CVSRAM
#ifndef NV_SMALL_EN
    AXIResponder::connections cvsramconn = {
        .aw_awvalid = &dla->nvdla_core2cvsram_aw_awvalid,
        .aw_awready = &dla->nvdla_core2cvsram_aw_awready,
        .aw_awid = &dla->nvdla_core2cvsram_aw_awid,
        .aw_awlen = &dla->nvdla_core2cvsram_aw_awlen,
        .aw_awaddr = &dla->nvdla_core2cvsram_aw_awaddr,

        .w_wvalid = &dla->nvdla_core2cvsram_w_wvalid,
        .w_wready = &dla->nvdla_core2cvsram_w_wready,
        .w_wdata = dla->nvdla_core2cvsram_w_wdata,
        .w_wstrb = &dla->nvdla_core2cvsram_w_wstrb,
        .w_wlast = &dla->nvdla_core2cvsram_w_wlast,

        .b_bvalid = &dla->nvdla_core2cvsram_b_bvalid,
        .b_bready = &dla->nvdla_core2cvsram_b_bready,
        .b_bid = &dla->nvdla_core2cvsram_b_bid,

        .ar_arvalid = &dla->nvdla_core2cvsram_ar_arvalid,
        .ar_arready = &dla->nvdla_core2cvsram_ar_arready,
        .ar_arid = &dla->nvdla_core2cvsram_ar_arid,
        .ar_arlen = &dla->nvdla_core2cvsram_ar_arlen,
        .ar_araddr = &dla->nvdla_core2cvsram_ar_araddr,

        .r_rvalid = &dla->nvdla_core2cvsram_r_rvalid,
        .r_rready = &dla->nvdla_core2cvsram_r_rready,
        .r_rid = &dla->nvdla_core2cvsram_r_rid,
        .r_rlast = &dla->nvdla_core2cvsram_r_rlast,
        .r_rdata = dla->nvdla_core2cvsram_r_rdata,
    };
    axi_cvsram = new AXIResponder(cvsramconn, this, "CVSRAM",
                                      true, maxReq, false);
#else
    axi_cvsram = nullptr;
#endif
}


Wrapper_nvdla::~Wrapper_nvdla() {
    delete dla;
#ifdef AXI_RESP_FAST_IO
    delete print_buffer;
#endif
    if (use_shared_spm) {
        if (shared_spm) {
            delete shared_spm;
            shared_spm = nullptr;
        }
    } else {
        delete spm;
    }
    exit(EXIT_SUCCESS);
}

void Wrapper_nvdla::reset() {
    //dla->rst = 1;
    dla->dla_core_clk = 1;
    dla->dla_csb_clk = 1;
    dla->eval();

    dla->dla_core_clk = 0;
    dla->dla_csb_clk = 0;
    dla->eval();
}

void Wrapper_nvdla::init() {
    dla->global_clk_ovr_on = 0;
    dla->tmc2slcg_disable_clock_gating = 0;
    dla->test_mode = 0;
    dla->nvdla_pwrbus_ram_c_pd = 0;
    dla->nvdla_pwrbus_ram_ma_pd = 0;
    dla->nvdla_pwrbus_ram_mb_pd = 0;
    dla->nvdla_pwrbus_ram_p_pd = 0;
    dla->nvdla_pwrbus_ram_o_pd = 0;
    dla->nvdla_pwrbus_ram_a_pd = 0;
    
    printf("reset...\n");
    dla->dla_reset_rstn = 1;
    dla->direct_reset_ = 1;
    dla->eval();
    for (int i = 0; i < 20; i++) {
        dla->dla_core_clk = 1;
        dla->dla_csb_clk = 1;
        dla->eval();
        
        dla->dla_core_clk = 0;
        dla->dla_csb_clk = 0;
        dla->eval();
    }

    dla->dla_reset_rstn = 0;
    dla->direct_reset_ = 0;
    dla->eval();
    
    for (int i = 0; i < 20; i++) {
        dla->dla_core_clk = 1;
        dla->dla_csb_clk = 1;
        dla->eval();
        
        dla->dla_core_clk = 0;
        dla->dla_csb_clk = 0;
        dla->eval();
    }
    
    dla->dla_reset_rstn = 1;
    dla->direct_reset_ = 1;
    
    printf("letting buffers clear after reset...\n");
    for (int i = 0; i < 4096; i++) {
        dla->dla_core_clk = 1;
        dla->dla_csb_clk = 1;
        dla->eval();
        
        dla->dla_core_clk = 0;
        dla->dla_csb_clk = 0;
        dla->eval();
    }
}

void Wrapper_nvdla::addReadReq(bool read_sram, bool read_timing, bool cacheable,
                uint64_t read_addr, uint32_t read_bytes) {

    output.read_valid = true;
    read_req_entry_t rd;
    rd.read_sram      = read_sram;
    rd.read_timing    = read_timing;
    rd.cacheable      = cacheable;
    rd.read_addr      = read_addr;
    rd.read_bytes     = read_bytes;
    output.read_buffer.push(rd);
}

void Wrapper_nvdla::addWriteReq(bool write_sram, bool write_timing,
                 uint64_t write_addr, uint8_t write_data) {
    output.write_valid = true;
    write_req_entry_t wr;
    wr.write_sram   = write_sram;
    wr.write_timing = write_timing;
    wr.write_data   = write_data;
    wr.write_addr   = write_addr;
    output.write_buffer.push(wr);
}

void Wrapper_nvdla::addLongWriteReq(bool write_sram, bool write_timing, bool cacheable,
                uint64_t write_addr, uint32_t length, const uint8_t* const write_data, uint64_t mask) {
    output.write_valid = true;
    long_write_req_entry_t wr;
    wr.write_sram = write_sram;
    wr.write_timing = write_timing;
    wr.cacheable = cacheable;
    wr.write_addr = write_addr;
    wr.length = length;
    wr.write_mask = mask;
    wr.write_data = new uint8_t[length];
#ifndef NO_DATA
    for (int i = 0; i < length; i++) {
        wr.write_data[i] = write_data[i];
    }
#endif
    output.long_write_buffer.push(std::move(wr));
}

void Wrapper_nvdla::clearOutput() {
    output.read_valid  = false;
    output.write_valid = false;
    while (!output.read_buffer.empty()) {
        output.read_buffer.pop();
    }
    while (!output.write_buffer.empty()) {
        output.write_buffer.pop();
    }
    // dma rd and wr buffer should be kept because
    // dma_engine cannot be issued with multiple tasks at once
}

outputNVDLA& Wrapper_nvdla::tick() {
    dla->dla_core_clk = 1;
    dla->dla_csb_clk = 1;
    dla->eval();

    dla->dla_core_clk = 0;
    dla->dla_csb_clk = 0;
    dla->eval();

    tickcount++;    // align this tick advancement with stats.txt

    _tickcount++;
    return output;
}

void Wrapper_nvdla::addDMAWriteReq(uint64_t addr, const std::vector<uint8_t>& write_data) {
    output.dma_write_buffer.emplace_back(addr, write_data);
}

void Wrapper_nvdla::tryMergeDMAWriteReq(uint64_t addr, uint8_t* write_data, uint32_t len) {
    if (!output.dma_write_buffer.empty()) {
        auto &q_end = output.dma_write_buffer.back();
        if (q_end.first + q_end.second.size() == addr && q_end.second.size() < spm->spm_line_size) {
            q_end.second.insert(q_end.second.end(), write_data, write_data + len);
            return;
        }
    }
    output.dma_write_buffer.emplace_back(addr, std::move(std::vector<uint8_t>(write_data, write_data + len)));
}

void Wrapper_nvdla::addDMAReadReq(uint64_t read_addr, uint32_t read_bytes) {
    output.dma_read_buffer.emplace(read_addr, read_bytes);
}

static const uint32_t WRAP_STATE_MAGIC = 0x4e56444c;
static const uint32_t WRAP_STATE_VERSION = 1;

void Wrapper_nvdla::save(const std::string &prefix) {
#ifdef NVDLA_RTL_SAVABLE
    VerilatedSave vs;
    vs.open((prefix + ".rtl").c_str());
    if (!vs.isOpen()) {
        fprintf(stderr, "Wrapper_nvdla: cannot write %s.rtl\n",
                prefix.c_str());
        abort();
    }
    vs << *dla;
    vs.close();
#else
    fprintf(stderr, "Wrapper_nvdla: the RTL model was not built with "
            "--savable, cannot checkpoint it\n");
    abort();
#endif

    StateOut os(prefix + ".wrap");
    os.pod(WRAP_STATE_MAGIC);
    os.pod(WRAP_STATE_VERSION);
    os.pod(tickcount);
    os.pod(_tickcount);
    os.pod(rtl_pipeline_idle_cycles);

    os.tag(0x0007);
    os.pod(output.read_valid);
    os.pod(output.write_valid);
    std::queue<read_req_entry_t> rq = output.read_buffer;
    os.pod<uint64_t>(rq.size());
    for (; !rq.empty(); rq.pop())
        os.pod(rq.front());
    std::queue<write_req_entry_t> wq = output.write_buffer;
    os.pod<uint64_t>(wq.size());
    for (; !wq.empty(); wq.pop())
        os.pod(wq.front());
    std::queue<long_write_req_entry_t> lq = output.long_write_buffer;
    os.pod<uint64_t>(lq.size());
    for (; !lq.empty(); lq.pop()) {
        long_write_req_entry_t e = lq.front();
        uint8_t *data = e.write_data;
        e.write_data = nullptr;
        os.pod(e);
        os.bytes(data, e.length);
    }
    std::queue<std::pair<uint64_t, uint32_t>> dq = output.dma_read_buffer;
    os.pod<uint64_t>(dq.size());
    for (; !dq.empty(); dq.pop()) {
        os.pod(dq.front().first);
        os.pod(dq.front().second);
    }
    os.pod<uint64_t>(output.dma_write_buffer.size());
    for (auto &e : output.dma_write_buffer) {
        os.pod(e.first);
        os.podVec(e.second);
    }

    csb->save(os);
    axi_dbb->save(os);
    os.pod<uint8_t>(axi_cvsram != nullptr);
    if (axi_cvsram)
        axi_cvsram->save(os);
    // A shared spm is written by every wrapper with the same content,
    // so loading it more than once gives the same result
    spm->save(os);
    os.tag(0xe0d);
}

void Wrapper_nvdla::load(const std::string &prefix) {
#ifdef NVDLA_RTL_SAVABLE
    VerilatedRestore vr;
    vr.open((prefix + ".rtl").c_str());
    if (!vr.isOpen()) {
        fprintf(stderr, "Wrapper_nvdla: cannot read %s.rtl\n",
                prefix.c_str());
        abort();
    }
    vr >> *dla;
    vr.close();
#else
    fprintf(stderr, "Wrapper_nvdla: the RTL model was not built with "
            "--savable, cannot restore it\n");
    abort();
#endif

    StateIn is(prefix + ".wrap");
    if (is.get<uint32_t>() != WRAP_STATE_MAGIC ||
            is.get<uint32_t>() != WRAP_STATE_VERSION) {
        fprintf(stderr, "Wrapper_nvdla: %s.wrap has a wrong magic or "
                "version\n", prefix.c_str());
        abort();
    }
    is.pod(tickcount);
    is.pod(_tickcount);
    is.pod(rtl_pipeline_idle_cycles);

    is.tag(0x0007);
    is.pod(output.read_valid);
    is.pod(output.write_valid);
    output.read_buffer = std::queue<read_req_entry_t>();
    for (uint64_t n = is.get<uint64_t>(); n > 0; n--)
        output.read_buffer.push(is.get<read_req_entry_t>());
    output.write_buffer = std::queue<write_req_entry_t>();
    for (uint64_t n = is.get<uint64_t>(); n > 0; n--)
        output.write_buffer.push(is.get<write_req_entry_t>());
    while (!output.long_write_buffer.empty()) {
        delete[] output.long_write_buffer.front().write_data;
        output.long_write_buffer.pop();
    }
    for (uint64_t n = is.get<uint64_t>(); n > 0; n--) {
        long_write_req_entry_t e = is.get<long_write_req_entry_t>();
        e.write_data = new uint8_t[e.length];
        is.bytes(e.write_data, e.length);
        output.long_write_buffer.push(e);
    }
    output.dma_read_buffer = std::queue<std::pair<uint64_t, uint32_t>>();
    for (uint64_t n = is.get<uint64_t>(); n > 0; n--) {
        uint64_t addr = is.get<uint64_t>();
        output.dma_read_buffer.emplace(addr, is.get<uint32_t>());
    }
    output.dma_write_buffer.clear();
    for (uint64_t n = is.get<uint64_t>(); n > 0; n--) {
        uint64_t addr = is.get<uint64_t>();
        output.dma_write_buffer.emplace_back(addr, std::vector<uint8_t>());
        is.podVec(output.dma_write_buffer.back().second);
    }

    csb->load(is);
    axi_dbb->load(is);
    if (is.get<uint8_t>() != (axi_cvsram != nullptr)) {
        fprintf(stderr, "Wrapper_nvdla: CVSRAM port mismatch in %s.wrap\n",
                prefix.c_str());
        abort();
    }
    if (axi_cvsram)
        axi_cvsram->load(is);
    spm->load(is);
    is.tag(0xe0d);
    is.expectEnd();
}
