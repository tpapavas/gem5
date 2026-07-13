/*
 * Copyright (c) 2022 Guillem Lopez Paradis
 * All rights reserved.
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

#include "dev/arm/nvdla_device_se.hh"

#include "debug/NvDlaDeviceSE.hh"
#include "debug/NvDlaDeviceSEDebug.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"

namespace gem5
{


NvDlaDeviceSE::NvDlaDeviceSE(const NvDlaDeviceSEParams &params) :
    BasicPioDevice(params, params.pio_size),
    interruptRaised(false),
    traceMode(params.trace_mode),
    engineStarted(false),
    onRead(false),
    netFinished(false),
    system(params.system),
    cpu(params.cpu),
    enableObject(params.enableRTLObject),
    enableWaveform(params.enableWaveform),
    to_retry_vaddr(0),
    tickEvent([this]{ tick(); }, params.name + " tick"),
    retryTranslateEvent([this]{ retryTranslate(); },
        params.name + " retryTranslate"),
    cyclesStat(0),
    cpuPort(params.name + ".cpu_side", this),
    cmdCpuPort(params.name + ".cmd_cpu_side", this),
    memPort(params.name + ".mem_side", this),
    sramPort(params.name + ".sram_port", this, true),
    dramPort(params.name + ".dram_port", this, false),
    dmaPort(this, params.system),
    bytesToRead(0),
    bytesReaded(0),
    blocked(false),
    max_req_inflight(params.maxReq),
    freq_ratio(params.freq_ratio),
    id_nvdla(params.id_nvdla),
    baseAddrDRAM(params.base_addr_dram),
    baseAddrSRAM(params.base_addr_sram),
    waiting_for_gem5_mem(0),
    flushing_spm(0),
    prefetch_enable(params.prefetch_enable),
    pft_threshold(params.pft_threshold),
    spm_latency(params.spm_latency),
    spm_line_size(params.spm_line_size),
    spm_line_num(params.spm_size / params.spm_line_size),
    dma_enable(params.dma_enable),
    use_fake_mem(params.use_fake_mem),
    print_path(params.print_path),
    trace(nullptr)
    {
    // fatal_if(!interrupt, "No NvDlaDeviceSE interrupt specified\n");

    cpu->setNvDlaDevice(id_nvdla, this);
    switch (params.buffer_mode) {
        case 0:
            buffer_mode = BUF_MODE_ALL;
            break;
        case 1:
            buffer_mode = BUF_MODE_PFT;
            break;
        case 2:
            buffer_mode = BUF_MODE_PFT_CUTOFF;
            break;
        default:
            assert(false);
    }

    uint32_t temp_assoc;
    temp_assoc = (params.assoc == "full") ?
        0xffffffff : std::stoi(params.assoc);
    assoc = (temp_assoc > spm_line_num) ? spm_line_num : temp_assoc;
    assert(assoc > 0);

    initNVDLA(params.use_shared_spm);
    cyclesNVDLA = 0;
    std::cout << std::hex << "NVDLA " << id_nvdla
              << " Base Addr DRAM: " << baseAddrDRAM
              << " Base Addr SRAM: " << baseAddrSRAM << std::endl;
    // Clear input
    memset(&input, 0, sizeof(inputNVDLA));

    if (dma_enable) {
        dma_rd_engine = new DmaReadFifo(dmaPort, spm_line_size * spm_line_num,
                                    spm_line_size, spm_line_num,
                                    Request::UNCACHEABLE);
        dma_wr_engine = new DmaNvdla(dmaPort, true,
                                    spm_line_size * spm_line_num,
                                    spm_line_size, spm_line_num,
                                    Request::UNCACHEABLE);
    } else {
        dma_rd_engine = nullptr;
        dma_wr_engine = nullptr;
    }
}


NvDlaDeviceSE::~NvDlaDeviceSE() {
    delete wr;
    if (dma_rd_engine != nullptr)
        delete dma_rd_engine;
    if (dma_wr_engine != nullptr)
        delete dma_wr_engine;
}

Port &
NvDlaDeviceSE::getPort(const std::string &if_name, PortID idx) {
    if (if_name == "mem_side") {
        return memPort;
    } else if (if_name == "cpu_side") {
        return cpuPort;
    } else if (if_name == "cmd_cpu_side") {
        return cmdCpuPort;
    } else if (if_name == "sram_port") {
        return sramPort;
    } else if (if_name == "dram_port") {
        return dramPort;
    } else if (if_name == "dma_port") {
        return dmaPort;
    } else {
        warn_if(true, "Asking to NvDlaDeviceSE for a port other "
                       "than cpu or mem");
        return BasicPioDevice::getPort(if_name, idx);
    }
}

bool
NvDlaDeviceSE::handleRequest(PacketPtr pkt) {
    // [DLA IO] this is used only with reg trace
    assert(traceMode);

    // Here we have just received the start rtlNVDLA function
    // we check if there is an outstanding call
    // otherwise start getting the whole trace
    if (blocked) {
        // There is currently an outstanding request. Stall.
        return false;
    }

    blocked = true;

    DPRINTF(NvDlaDeviceSE, "Got request for size: %d, addr: %#x\n",
                        pkt->getSize(),
                        pkt->req->getVaddr());

    bytesToRead = pkt->getSize();   // it works as a counter
    trace->trace_and_rd_log_size = bytesToRead;

    ptrTrace = (char *) malloc(bytesToRead);

    startTranslate(pkt->req->getVaddr(), 0);

    return true;
}

void
NvDlaDeviceSE::initNVDLA(bool use_shared_spm) {
    // Wrapper
    wr = new Wrapper_nvdla(id_nvdla, max_req_inflight,
        dma_enable, spm_latency, spm_line_size, spm_line_num,
        prefetch_enable, use_shared_spm, buffer_mode, assoc);

    // wrapper trace from nvidia
    if (traceMode) {
        std::cout << "trace loaded!!" << std::endl;
        trace = new TraceLoaderGem5(wr->csb, wr->axi_dbb, wr->axi_cvsram);
    } else {
        // reset NVDLA
        std::cout << "reset NVDLA... at else " << std::endl;
        wr->init();
        // init some variable before exec of trace
        quiesc_timer = 200;
        waiting = 0;

        sim_time = time(nullptr);
    }
}

void
NvDlaDeviceSE::loadTraceNVDLA(char *ptr) {
    // load the trace into the queues
    trace->load(ptr);
    trace->load_read_var_log(ptr);  // call load_read_var_log
                                    // no matter prefetch enabled or not.
    // If not, we will directly see 8 bytes of 0xff
    // indicating the end of rd_var_log.

    startBaseTrace = trace->getBaseAddr();

    DPRINTF(NvDlaDeviceSE,
            "Base Addr: %#x \n",
            trace->getBaseAddr());
    // reset NVDLA
    wr->init();
    // init some variable before exec of trace
    quiesc_timer = 200;
    waiting = 0;

    //schedule(tickEvent, nextCycle() + (freq_ratio - 1) * clockPeriod());
    schedule(tickEvent,
        nextCycle() + static_cast<Tick>(abs((freq_ratio-1.0))*clockPeriod()));
    sim_time = time(nullptr);
}

void
NvDlaDeviceSE::processOutput(outputNVDLA& out) {
    if (out.read_valid) {
        while (!out.read_buffer.empty()) {
            read_req_entry_t aux = out.read_buffer.front();
            // printf("read req addr: %08x, size %d\n",
            //         aux.read_addr, aux.read_bytes);
            // std::cout << std::hex << "read req: " \
            // << aux.read_addr << std::endl;
            readAXIVariable(aux.read_addr,
                            aux.read_sram,
                            aux.read_timing,
                            aux.cacheable,
                            aux.read_bytes);
            out.read_buffer.pop();
        }
    }

    if (out.write_valid) {
        while (!out.write_buffer.empty()) {
            // this buffer outputs in 1-byte granularity
            write_req_entry_t aux = out.write_buffer.front();
            writeAXI(aux.write_addr,
                     aux.write_data,
                     aux.write_sram,
                     aux.write_timing);
            out.write_buffer.pop();
        }

        while (!out.long_write_buffer.empty()) {
            // this buffer outputs in 1-64 bytes granularity
            auto& aux = out.long_write_buffer.front();
            writeAXILong(aux.write_addr, aux.length, aux.write_data,
                         aux.write_mask, aux.write_sram,
                         aux.write_timing, aux.cacheable);
            out.long_write_buffer.pop();
        }
    }

    //! use dma_rd_engine to process reading requests
    // memory requests already in spm is dealt with in wrapper_nvdla
    // only one DMA request can be tackled at once
    if (!out.dma_read_buffer.empty()) {
        std::cout << "DMA read req: addr "
                  << std::hex << out.dma_read_buffer.front().first
                  << ", len " << std::dec << out.dma_read_buffer.front().second
                  << std::endl;
        auto& aux = out.dma_read_buffer.front();

        uint64_t real_addr = getRealAddr(aux.first, false);
        // only DRAM has DMA fetch
        if (dma_rd_engine->atEndOfBlock()) {
            dma_rd_engine->startFill(real_addr, aux.second);
#ifndef AXI_RESP_FAST_IO
            printf("(%lu) nvdla#%d DMA read req is issued: "
                   "addr 0x%08lx, len %d\n",
                   wr->tickcount, id_nvdla, aux.first, aux.second);
#endif
            stats.num_dma_rd++;
            // after successfully calling DMA, pop aux
            out.dma_read_buffer.pop();
        }   // if previous DMA request not sent, have to stop here
    }


    //! use dma_wr_engine to process writing requests
    if (!out.dma_write_buffer.empty()) {
        auto& aux = out.dma_write_buffer.front();
        if (dma_wr_engine->atEndOfBlock()) {
            // previous DMA write has been sent
            uint64_t real_addr = getRealAddr(aux.first, false);
            // only DRAM has DMA write
            dma_wr_engine->startFill(real_addr, aux.second.size(),
                                     aux.second.data());
#ifndef AXI_RESP_FAST_IO
            printf("(%lu) nvdla#%d DMA write req is issued: "
                   "addr 0x%08lx, len %ld\n",
                   wr->tickcount, id_nvdla, aux.first, aux.second.size());
#endif
            stats.num_dma_wr++;
            out.dma_write_buffer.pop_front();
        }
    }
}

void
NvDlaDeviceSE::runIterationNVDLA() {
    wr->clearOutput();

    if (interruptRaised && !wr->dla->dla_intr) {
        printf("(%lu) interrupt finished...\n", wr->tickcount);
        interruptRaised = false;
    }

    int extevent;

    if (!waiting_for_gem5_mem) {
        if (!onRead) {
            extevent = wr->csb->eval(waiting);
        } else {
            extevent = wr->csb->eval(waiting, &readData);
        }
    }
    else
        extevent = 0;

    if (extevent == TraceLoaderGem5::TRACE_AXIEVENT || waiting_for_gem5_mem) {
        trace->axievent(&waiting_for_gem5_mem);
    } else if (extevent == TraceLoaderGem5::TRACE_WFI) {
        waiting = 1;
#ifndef AXI_RESP_FAST_IO
        printf("(%lu) waiting for interrupt...\n", wr->tickcount);
#endif
    } else if (extevent == TraceLoaderGem5::TRACE_RESET) {
        wr->init();
#ifndef AXI_RESP_FAST_IO
        printf("nvdla#%d reset\n", id_nvdla);
#endif
    }

    if (traceMode) {
        if (waiting && wr->dla->dla_intr) {
#ifndef AXI_RESP_FAST_IO
            printf("(%lu) nvdla#%d interrupt!\n", wr->tickcount, id_nvdla);
#endif
            waiting = 0;
        }
    } else {
        if (wr->dla->dla_intr) {
            if (!interruptRaised) {
#ifndef AXI_RESP_FAST_IO
                printf("(%lu) nvdla#%d interrupt!\n", wr->tickcount, id_nvdla);
#endif
                // interrupt->raise();
                interruptRaised = true;
            }
        }
    }

    if (!waiting_for_gem5_mem) {
        if (!use_fake_mem) {
            wr->axi_dbb->eval_timing();
            if (wr->axi_cvsram) {
                wr->axi_cvsram->eval_timing();
            }
        } else {
            wr->axi_dbb->eval_ram();
            if (wr->axi_cvsram) {
                wr->axi_cvsram->eval_ram();
            }
        }
    }

    outputNVDLA& output = wr->tick();
    getStats().nvdla_rtl_cycles = getTickfromWrapperNVDLA();
    //getStats().nvdla_rtl_cycles_idle =
        // (getStats().nvdla_rtl_cycles.value() -
        // getStats().nvdla_total_Bdma0.value() -
        // getStats().nvdla_total_Bdma1.value() -
        // getStats().nvdla_total_Cdp0.value() -
        // getStats().nvdla_total_Cdp1.value() -
        // getStats().nvdla_total_Cmnv0.value() -
        // getStats().nvdla_total_Conv1.value() -
        // getStats().nvdla_total_Pdp0.value() -
        // getStats().nvdla_total_Pdp1.value() -
        // getStats().nvdla_total_Rubik0.value() -
        // getStats().nvdla_total_Rubik1.value() -
        // getStats().nvdla_total_Sdp0.value() -
        // getStats().nvdla_total_Sdp1.value() -
        // getStats().nvdla_total_Cacc0.value() -
        // getStats().nvdla_total_Cacc1.value() -
        // getStats().nvdla_total_CdmaDat0.value() -
        // getStats().nvdla_total_CdmaDat1.value() -
        // getStats().nvdla_total_CdmaWt0.value() -
        // getStats().nvdla_total_CdmaWt1.value());

    if (dma_enable) {
        try_get_dma_read_data(spm_line_size);
        if (traceMode) {
            if (wr->csb->done()) {
                // write back dirty data in spm to main memory
                if (!flushing_spm) {
                    wr->spm->clear_and_write_back_dirty();
                    flushing_spm = 1;
                }
                // all items have been flushed to dma write engine
                if (flushing_spm && output.dma_write_buffer.empty()) {
                    flushing_spm = 0;
                    // printf("nvdla#%d spm flush complete!\n", id_nvdla);
                }
            }
        } else if (netFinished) {
            // write back dirty data in spm to main memory
            if (!flushing_spm) {
                wr->spm->clear_and_write_back_dirty();
                flushing_spm = 1;
            }
            // all items have been flushed to dma write engine
            if (flushing_spm && output.dma_write_buffer.empty()) {
                flushing_spm = 0;
                // printf("nvdla#%d spm flush complete!\n", id_nvdla);
            }
        }
    }
    processOutput(output);

#ifdef AXI_RESP_FAST_IO
    if (Wrapper_nvdla::buf_ptr >= PB_SIZE) {
        std::ofstream fout;
        fout.open(print_path,
                  std::ios::out | std::ios::app | std::ios::binary);
        fout.write((char*)Wrapper_nvdla::print_buffer,
                   Wrapper_nvdla::buf_ptr * sizeof(uint64_t));
        fout.close();
        Wrapper_nvdla::buf_ptr = 0;
    }
#endif
}

void
NvDlaDeviceSE::tick() {
    DPRINTF(NvDlaDeviceSEDebug, "Tick NVDLA \n");
    // if we are still running trace
    // runIteration
    // schedule new iteration
    if (!wr->csb->done() || (quiesc_timer-- > 0)
            || waiting_for_gem5_mem || flushing_spm) {

        if (wr->axi_dbb->getRequestsOnFlight() == 0 &&
            //wr->axi_cvsram->getRequestsOnFlight() == 0 &&
            !waiting_for_gem5_mem &&
            !flushing_spm)
        {
            stats.nvdla_idle_cycles++;
        }

        // Update stats
        // stats.nvdla_avgReqCVSRAM.sample(
        //     wr->axi_cvsram->getRequestsOnFlight());
        stats.nvdla_avgReqDBBIF.sample(wr->axi_dbb->getRequestsOnFlight());
        stats.nvdla_cycles++;
        cyclesNVDLA++;
        runIterationNVDLA();
        //schedule(tickEvent, nextCycle() + (freq_ratio - 1) * clockPeriod());
        schedule(tickEvent,
            nextCycle() +
            static_cast<Tick>(abs((freq_ratio-1.0))*clockPeriod()));
    } else {
        if (traceMode) {
            // we have finished running the trace
            printf("done at %lu ticks\n", wr->tickcount);
            printf("simulation time: %lu seconds\n", time(nullptr) - sim_time);

            if (!trace->test_passed()) {
                printf("*** FAIL: test failed due to output mismatch\n");

            } else if (!wr->csb->test_passed()) {
                printf("*** FAIL: test failed due to CSB read mismatch\n");
            } else {
                printf("NVDLA %d *** PASS\n", id_nvdla);
            }

    #ifdef AXI_RESP_FAST_IO
            if (Wrapper_nvdla::buf_ptr != 0) {
                std::ofstream fout;
                fout.open(print_path,
                    std::ios::out | std::ios::app | std::ios::binary);
                fout.write((char*)Wrapper_nvdla::print_buffer,
                    Wrapper_nvdla::buf_ptr * sizeof(uint64_t));
                fout.close();
                Wrapper_nvdla::buf_ptr = 0;
            }
    #endif
            // we send a null packet telling we have finished
            RequestPtr req = std::make_shared<Request>(id_nvdla, 1,
                                                   Request::UNCACHEABLE, 0);
            PacketPtr packet = nullptr;
            // we create the real packet, write request
            packet = Packet::createRead(req);
            packet->allocate();
            packet->makeResponse();
            cpuPort.sendPacket(packet);
        } else {
            runIterationNVDLA();
            //schedule(tickEvent, nextCycle() + (freq_ratio-1)*clockPeriod());
            schedule(tickEvent,
                nextCycle() +
                static_cast<Tick>(abs((freq_ratio-1.0))*clockPeriod()));
        }
    }
    // check DRAM Ports
    dramPort.tick();
    sramPort.tick();
}


bool
NvDlaDeviceSE::handleResponse(PacketPtr pkt) {
    assert(traceMode);

    if (pkt->hasData()) {
        char *data_ptr = pkt->getPtr<char>();

        int maxRead = ((bytesToRead - bytesReaded) > 64) ?
                         64 : (bytesToRead - bytesReaded);

        for (int i = 0;i < maxRead; i++) {
            ptrTrace[bytesReaded + i] = data_ptr[i];
        }

        bytesReaded += 64;

        if (bytesReaded < bytesToRead) {
            startTranslate(pkt->req->getVaddr() + 64, 0);
        } else {
            bytesReaded = 0;
            bytesToRead = 0;

            // Load the trace and reset NVDLA
            loadTraceNVDLA(ptrTrace);
        }
    }
    else {
        // Strange situation, report!
        DPRINTF(NvDlaDeviceSE, "Got response for addr %#x no data\n",
                            pkt->getAddr());
    }


    // The packet is now done. We're about to put it in the port, no need for
    // this object to continue to stall.
    // We need to free the resource before sending the packet in case the CPU
    // tries to send another request immediately (e.g., in the same callchain).
    blocked = false;

    return true;
}

bool
NvDlaDeviceSE::handleResponseNVDLA(PacketPtr pkt, bool sram) {
    if (pkt->hasData()) {
        if (pkt->isRead()) {
            // Get data from gem5 memory system
            // and set it to AXI
            DPRINTF(rtlNVDLADebug,
                    "Handling response for data read Timing\n");
            // Get the data ptr and sent it
            const uint8_t* dataPtr = pkt->getConstPtr<uint8_t>();
            uint64_t addr_nvdla = getAddrNVDLA(pkt->getAddr(), sram);
            if (sram) {
                // SRAM
                wr->axi_cvsram->inflight_resp(addr_nvdla, dataPtr);
            } else {
                // DBBIF
                wr->axi_dbb->inflight_resp(addr_nvdla, dataPtr);
            }
        } else {
            // this is somehow odd, report!
            DPRINTF(NvDlaDeviceSE, "Got response for addr %#x no read\n",
                    pkt->getAddr());
        }
    } else {
         DPRINTF(NvDlaDeviceSE, "Got response for addr %#x no data\n",
         pkt->getAddr());
    }

    return true;
}

void
NvDlaDeviceSE::handleFunctional(PacketPtr pkt) {
    assert(traceMode);

    // Just pass this on to the memory side to handle for now.
    memPort.sendFunctional(pkt);
}

AddrRangeList
NvDlaDeviceSE::getAddrRanges() const {
    DPRINTF(NvDlaDeviceSE, "Sending new ranges\n");
    // Just use the same ranges as whatever is on the memory side.
    if (traceMode) {
        return memPort.getAddrRanges();
    } else {
        return BasicPioDevice::getAddrRanges();
    }
}

void
NvDlaDeviceSE::sendRangeChange() {
    cpuPort.sendRangeChange();
}

void
NvDlaDeviceSE::finishTranslation(WholeTranslationState *state) {
    DPRINTF(NvDlaDeviceSE, "Finishing translation\n");

    RequestPtr req = state->mainReq;

    if (req->hasPaddr()) {
        DPRINTF(NvDlaDeviceSE,
                "Finished translation step: Got request for addr %#x %#x\n",
        state->mainReq->getVaddr(),state->mainReq->getPaddr());

    } else {
        to_retry_vaddr = req->getVaddr();
        schedule(retryTranslateEvent, nextCycle());
        return;
    }

    PacketPtr new_pkt = new Packet(req, MemCmd::ReadReq, 64);

    if (memPort.blockedPacket != nullptr) {
        DPRINTF(NvDlaDeviceSE, "Packet lost\n");
    } else {
        new_pkt->allocate();
        memPort.sendPacket(new_pkt);
    }
}

// DRAM PORT
void
NvDlaDeviceSE::MemNVDLAPort::sendPacket(PacketPtr pkt, bool timing) {
    if (timing) {
        DPRINTF(NvDlaDeviceSE,
            "Add Mem Req pending %#x size: %d timing s: %d\n",
            pkt->getAddr(), pkt->getSize(), pending_req.size());
        // we add as a pending request, we deal later
        pending_req.push(pkt);
    } else {
        DPRINTF(NvDlaDeviceSE,
            "Send Mem Req to DRAM %#x size: %d functional\n",
            pkt->getAddr(), pkt->getSize());
        // send Atomic
        sendAtomic(pkt);
        // Update all the pointers
        recentData32 = *pkt->getConstPtr<uint32_t>();
        recentData = *pkt->getConstPtr<uint8_t>();
        recentDataptr = pkt->getConstPtr<uint8_t>();
    }
}

void
NvDlaDeviceSE::MemNVDLAPort::recvRangeChange() {
    owner->sendRangeChange();
}

bool
NvDlaDeviceSE::MemNVDLAPort::recvTimingResp(PacketPtr pkt) {
    DPRINTF(NvDlaDeviceSE, "Got response SRAM?: %d\n", sram);
    return owner->handleResponseNVDLA(pkt, sram);
}

void
NvDlaDeviceSE::MemNVDLAPort::recvReqRetry() {
    // we check we have pending packets
    assert(blockedRetry);
    // Grab the blocked packet.
    PacketPtr pkt = pending_req.front();
    bool sent = sendTimingReq(pkt);
    // if not sent put it in the queue
    if (sent) {
        pending_req.pop();
        blockedRetry = false;
    }
    // else
    // we do nothing
    // we failed sending the packet we wait
}

// In this function we send the packets
void
NvDlaDeviceSE::MemNVDLAPort::tick() {
    // we check we have pending packets
    if (!blockedRetry and !pending_req.empty()) {
        PacketPtr pkt = pending_req.front();
        bool sent = sendTimingReq(pkt);
        // if not sent, put it in the queue
        if (sent) {
            pending_req.pop();
        } else {
            blockedRetry = true;
        }
    }
}

uint64_t
NvDlaDeviceSE::getRealAddr(uint64_t addr, bool sram) {
    uint64_t real_addr;

    if (sram) {
        // Base addr is 0x5000_0000
        real_addr = (addr - 0x50000000) + baseAddrSRAM;
    } else {
        if (traceMode) {
            // Base addr is 0xc000_0000
            real_addr = (addr - 0xc0000000) + baseAddrDRAM;
        } else {
            real_addr = addr;
        }
    }
    return real_addr;
}

uint64_t
NvDlaDeviceSE::getAddrNVDLA(uint64_t addr, bool sram) {
    uint64_t real_addr;

    if (sram) {
        real_addr = (addr - baseAddrSRAM) + 0x50000000;
    } else {
        if (traceMode) {
            real_addr = (addr - baseAddrDRAM) + 0xc0000000;
        } else {
            real_addr = addr;
        }
    }
    return real_addr;
}

const uint8_t *
NvDlaDeviceSE::readAXIVariable(uint64_t addr, bool sram, bool timing,
        bool cacheable, unsigned int size) {
    // Update stats
    stats.nvdla_reads++;

    uint64_t real_addr = getRealAddr(addr, sram);

    DPRINTF(NvDlaDeviceSE,
            "Read AXI Variable addr: %#x, real_addr %#x, size %d\n",
            addr, real_addr, size);

    RequestPtr req = std::make_shared<Request>(real_addr, size,
        cacheable ? 0: Request::UNCACHEABLE, 0);
    PacketPtr packet = nullptr;
    // we create the real packet, write request
    packet = Packet::createRead(req);
    packet->allocate();
    // send the packet in timing?
    if (sram) {
        sramPort.sendPacket(packet, timing);
        return sramPort.recentDataptr;
    } else {
        dramPort.sendPacket(packet, timing);
        return dramPort.recentDataptr;
    }
    return nullptr;
}

void
NvDlaDeviceSE::writeAXI(uint64_t addr, uint8_t data, bool sram, bool timing) {
    // Update stats
    stats.nvdla_writes++;

    uint64_t real_addr = getRealAddr(addr, sram);

    DPRINTF(NvDlaDeviceSE,
        "Write AXI Variable addr: %#x, real_addr %#x, data_to_write 0x%02x\n",
        addr, real_addr, data);
    //Request(Addr paddr, unsigned size, Flags flags, MasterID mid)
    // addr is the physical addr
    // size is one byte
    // flags is physical (vaddr is also the physical one)
    RequestPtr req = std::make_shared<Request>(real_addr, 1,
                                               0, 0);
    PacketPtr packet = nullptr;
    // we create the real packet, write request
    packet = Packet::createWrite(req);
    // always in Little Endian
    PacketDataPtr dataAux = new uint8_t[1];
#ifndef NO_DATA
    dataAux[0] = data;
#endif
    packet->dataDynamic(dataAux);
    // send the packet in timing?
    if (sram) {
        sramPort.sendPacket(packet, timing);
    } else {
        dramPort.sendPacket(packet, timing);
    }
}

void
NvDlaDeviceSE::writeAXILong(uint64_t addr, uint32_t length, uint8_t* data,
        uint64_t mask, bool sram, bool timing, bool cacheable) {
    stats.nvdla_writes++;

    uint64_t real_addr = getRealAddr(addr, sram);
    RequestPtr req = std::make_shared<Request>(real_addr, length,
        cacheable ? 0: Request::UNCACHEABLE, 0);
#ifndef NO_DATA
    std::vector<bool> byte_enable_vec(length);

    for (int i = 0; i < length; i++)
        byte_enable_vec[i] = ((mask >> i) & 1);

    req->setByteEnable(byte_enable_vec);
#endif
    PacketPtr packet = nullptr;
    packet = Packet::createWrite(req);

    // here we directly give the 'data' ptr to pkt.
    // This is supported by the fact that 'data' is malloced by ourselves
    packet->dataDynamic(data);
    // send the packet in timing?
    if (sram) {
        sramPort.sendPacket(packet, timing);
    } else {
        dramPort.sendPacket(packet, timing);
    }
}

void
NvDlaDeviceSE::try_get_dma_read_data(uint32_t size) {
    uint8_t dma_temp_buffer[size];
    bool get_success = dma_rd_engine->tryGet(dma_temp_buffer, size);
    if (get_success) {
        std::cout << " Success Got DMA "<< std::endl;
        // we assume only DBB involves DMA.
        // SRAM should not be accessed with DMA
        wr->axi_dbb->inflight_dma_resp(dma_temp_buffer, size);
    }
}

void
NvDlaDeviceSE::regStats() {
    // If you don't do this you get errors about uninitialized stats.
    ClockedObject::regStats();

    using namespace statistics;

    stats.nvdla_cycles
        .name(name() + ".nvdla_cycles")
        .desc("Number of Cycles to run the trace");

    stats.nvdla_reads
        .name(name() + ".nvdla_reads")
        .desc("Number of reads performed");

    stats.nvdla_writes
        .name(name() + ".nvdla_writes")
        .desc("Number of writes performed");

    stats.nvdla_csb_reads
        .name(name() + ".nvdla_csb_reads")
        .desc("Number of reads performed");

    stats.nvdla_csb_writes
        .name(name() + ".nvdla_csb_writes")
        .desc("Number of writes performed");

    stats.nvdla_idle_cycles
        .name(name() + ".nvdla_idle_cycles")
        .desc("Number of idle cycles");

    stats.nvdla_avgReqCVSRAM
        .init(256)
        .name(name() + ".nvdla_avgReqCVSRAM")
        .desc("Histogram Requests onflight CVSRAM")
        .flags(pdf);

    stats.nvdla_avgReqDBBIF
        .init(256)
        .name(name() + ".nvdla_avgReqDBBIF")
        .desc("Histogram Requests onflight DBBIF")
        .flags(pdf);


    stats.num_dma_rd
        .name(name() + ".num_dma_rd")
        .desc("Number of DMA read issued by this NVDLA");
    stats.num_dma_wr
        .name(name() + ".num_dma_wr")
        .desc("Number of DMA write issued by this NVDLA");

    stats.nvdla_rtl_cycles
        .name(name() + ".nvdla_rtl_cycles")
        .desc("Number of RTL cycles to run the trace from RTL");
    //stats.nvdla_rtl_cycles_idle
    //    .name(name() + ".nvdla_rtl_cycles_idle")
    //    .desc("Number of RTL cycles to run the trace from RTL");

    stats.nvdla_total_Bdma0
        .name(name() + ".nvdla_total_Bdma0")
        .desc("Number of Cycles to run the Bdma0 from RTL");

    stats.nvdla_total_Bdma1
        .name(name() + ".nvdla_total_Bdma1")
        .desc("Number of Cycles to run the Bdma1 from RTL");

    stats.nvdla_total_Cdp0
        .name(name() + ".nvdla_total_Cdp0")
        .desc("Number of Cycles to run the Cdp0 from RTL");

    stats.nvdla_total_Cdp1
        .name(name() + ".nvdla_total_Cdp1")
        .desc("Number of Cycles to run the Cdp1 from RTL");

    stats.nvdla_total_Cmac0
        .name(name() + ".nvdla_total_Cmac0")
        .desc("Number of Cycles to run the Cmac0 from RTL");

    stats.nvdla_total_Cmac1
        .name(name() + ".nvdla_total_Cmac1")
        .desc("Number of Cycles to run the Cmac1 from RTL");

    stats.nvdla_total_Pdp0
        .name(name() + ".nvdla_total_Pdp0")
        .desc("Number of Cycles to run the Pdp0 from RTL");
    stats.nvdla_total_Pdp1
        .name(name() + ".nvdla_total_Pdp1")
        .desc("Number of Cycles to run the Pdp1 from RTL");
    stats.nvdla_total_Rubik0
        .name(name() + ".nvdla_total_Rubik0")
        .desc("Number of Cycles to run the Rubik0 from RTL");
    stats.nvdla_total_Rubik1
        .name(name() + ".nvdla_total_Rubik1")
        .desc("Number of Cycles to run the Rubik1 from RTL");
    stats.nvdla_total_Sdp0
        .name(name() + ".nvdla_total_Sdp0")
        .desc("Number of Cycles to run the Sdp0 from RTL");
    stats.nvdla_total_Sdp1
        .name(name() + ".nvdla_total_Sdp1")
        .desc("Number of Cycles to run the Sdp1 from RTL ");
    stats.nvdla_total_Cacc0
        .name(name() + ".nvdla_total_Cacc0")
        .desc("Number of Cycles to run the Cacc0 from RTL");
    stats.nvdla_total_Cacc1
        .name(name() + ".nvdla_total_Cacc1")
        .desc("Number of Cycles to run the Cacc1 from RTL");
    stats.nvdla_total_CdmaDat0
        .name(name() + ".nvdla_total_CdmaDat0")
        .desc("Number of Cycles to run the CdmaDat0 from RTL");
    stats.nvdla_total_CdmaDat1
        .name(name() + ".nvdla_total_CdmaDat1")
        .desc("Number of Cycles to run the CdmaDat1 from RTL");
    stats.nvdla_total_CdmaWt0
        .name(name() + ".nvdla_total_CdmaWt0")
        .desc("Number of Cycles to run the CdmaWt0 from RTL");
    stats.nvdla_total_CdmaWt1
        .name(name() + ".nvdla_total_CdmaWt1")
        .desc("Number of Cycles to run the CdmaWt1 from RTL");
}

//// rtlObject code ////
void
NvDlaDeviceSE::CPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the memobj is blocking.
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingResp(pkt)) {
        blockedPacket = pkt;
    }
}

AddrRangeList
NvDlaDeviceSE::CPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
NvDlaDeviceSE::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(NvDlaDeviceSE, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

void
NvDlaDeviceSE::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the memobj.
    return owner->handleFunctional(pkt);
}

bool
NvDlaDeviceSE::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    // Just forward to the memobj.
    if (pkt->req->hasPaddr()) {
        DPRINTF(NvDlaDeviceSE, "Got request for size: %d,  addr: %#x %#x\n",
            pkt->getSize(),
            pkt->req->getVaddr(),
            pkt->req->getPaddr());

    } else {
        owner->handleRequest(pkt);
    }
    // Try to handle the request by calling to
    // handleRequest() function to be implemented in
    // the rtlObject derived class
    if (!owner->handleRequest(pkt)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
    return true;
}

void
NvDlaDeviceSE::CPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

//////// NvDla CODE ////////
void
NvDlaDeviceSE::CmdCPUSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the memobj is blocking.
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingResp(pkt)) {
        blockedPacket = pkt;
    }
}

AddrRangeList
NvDlaDeviceSE::CmdCPUSidePort::getAddrRanges() const
{
    return owner->getAddrRanges();
}

void
NvDlaDeviceSE::CmdCPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        // Only send a retry if the port is now completely free
        needRetry = false;
        DPRINTF(NvDlaDeviceSE, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

void
NvDlaDeviceSE::CmdCPUSidePort::recvFunctional(PacketPtr pkt)
{
    // Just forward to the memobj.
    return owner->handleFunctional(pkt);
}

bool
NvDlaDeviceSE::CmdCPUSidePort::recvTimingReq(PacketPtr pkt)
{
    // Just forward to the memobj.
    if (pkt->req->hasPaddr()) {
        // DPRINTF(NvDlaDeviceSE, "[GEM5 LOG] got request\n");

        // uint32_t *data = new uint32_t;
        // *data = 100;

        // pkt->allocate();
        // pkt->setData((uint8_t*)data);
        // return true;

        // DPRINTF(NvDlaDeviceSE, "Got request for size: %d,  addr: %#x %#x\n",
        //     pkt->getSize(),
        //     pkt->req->getVaddr(),
        //     pkt->req->getPaddr());
        if (pkt->hasData()) {
            // this is a write request
            pkt->makeAtomicResponse();

            uint32_t write_addr =
            (uint32_t) 0xFFFF0000 + ((pkt->getAddr() - 0) >> 2);

            DPRINTF(NvDlaDeviceSE, "write req: addr: 0x%08x, data: 0x%08x\n",
            write_addr, pkt->getLE<uint32_t>());

            owner->wr->csb->write(write_addr, pkt->getLE<uint32_t>());
            owner->stats.nvdla_csb_writes++;

            if (write_addr == 0xFFFF0003) {
                // owner->interrupt->clear();
            }
        } else {
            // this is a read request
            owner->onRead = true;
            if (!owner->engineStarted) {
                owner->engineStarted = true;
                //owner->schedule(owner->tickEvent,
                //    owner->nextCycle() +
                //    (owner->freq_ratio - 1) * owner->clockPeriod());

                owner->schedule(owner->tickEvent,
                    owner->nextCycle() +
                    static_cast<Tick>(
                        (owner->freq_ratio-1)*owner->clockPeriod()
                    )
                );
            }

            uint32_t *data = new uint32_t;

            pkt->makeAtomicResponse();

            DPRINTF(NvDlaDeviceSE, "read req: addr: 0x%08x\n",
            pkt->getAddr());

            if (pkt->getAddr() == 0x20000) {
                *data = owner->interruptRaised ? 1 : 0;
                pkt->allocate();
                pkt->setData((uint8_t*)data);

                DPRINTF(NvDlaDeviceSE,
                    "read req: packet size: %d, data: 0x%08x\n",
                    pkt->getSize(), pkt->getLE<uint32_t>());

                owner->onRead = false;
                return 0;
            } else if (pkt->getAddr() == 0x20004) {
                pkt->allocate();

                DPRINTF(NvDlaDeviceSE,
                    "read req: packet size: %d, data: 0x%08x\n",
                    pkt->getSize(), pkt->getLE<uint32_t>());

                owner->netFinished = true;
                return 0;
            }

            uint32_t read_addr =
            (uint32_t) 0xFFFF0000 + ((pkt->getAddr() - 0) >> 2);
            DPRINTF(NvDlaDeviceSE, "read req: reg: 0x%08x\n", read_addr);

            owner->wr->csb->read(read_addr, 0xffffffff, 0);
            owner->stats.nvdla_csb_reads++;

            // temp solution
            while (!owner->wr->csb->done()) {
                DPRINTF(NvDlaDeviceSEDebug, "Tick NVDLA \n");
                // if we are still running trace
                // runIteration
                // schedule new iteration
                if (!owner->wr->csb->done() || (owner->quiesc_timer-- > 0)
                        || owner->waiting_for_gem5_mem
                        || owner->flushing_spm) {
                    // Update stats
                    // stats.nvdla_avgReqCVSRAM.sample(
                    //     wr->axi_cvsram->getRequestsOnFlight());
                    owner->stats.nvdla_avgReqDBBIF.sample(
                        owner->wr->axi_dbb->getRequestsOnFlight());
                    owner->stats.nvdla_cycles++;
                    owner->cyclesNVDLA++;
                    owner->wr->clearOutput();

                    if (owner->interruptRaised && !owner->wr->dla->dla_intr) {
                        printf("(%lu) interrupt finished...\n",
                            owner->wr->tickcount);
                        owner->interruptRaised = false;
                    }

                    int extevent;

                    if (!owner->waiting_for_gem5_mem)
                        extevent = owner->wr->csb->eval(owner->waiting, data);
                    else
                        extevent = 0;

                    if (extevent == TraceLoaderGem5::TRACE_AXIEVENT
                        || owner->waiting_for_gem5_mem) {
                        owner->trace->axievent(&owner->waiting_for_gem5_mem);
                    } else if (extevent == TraceLoaderGem5::TRACE_WFI) {
                        owner->waiting = 1;
                #ifndef AXI_RESP_FAST_IO
                        printf("(%lu) waiting for interrupt...\n",
                            owner->wr->tickcount);
                #endif
                    } else if (extevent == TraceLoaderGem5::TRACE_RESET) {
                        owner->wr->init();
                #ifndef AXI_RESP_FAST_IO
                        printf("nvdla#%d reset\n", owner->id_nvdla);
                #endif
                    }

                    if (owner->traceMode) {
                        if (owner->waiting && owner->wr->dla->dla_intr) {
                #ifndef AXI_RESP_FAST_IO
                            printf("(%lu) nvdla#%d interrupt!\n",
                                owner->wr->tickcount, owner->id_nvdla);
                #endif
                            owner->waiting = 0;
                        }
                    } else {
                        if (owner->wr->dla->dla_intr) {
                            if (!owner->interruptRaised) {
                #ifndef AXI_RESP_FAST_IO
                                printf("(%lu) nvdla#%d interrupt!\n",
                                    owner->wr->tickcount, owner->id_nvdla);
                #endif
                                // owner->interrupt->raise();
                                owner->interruptRaised = true;
                            }
                        }
                    }

                    if (!owner->waiting_for_gem5_mem) {
                        if (!owner->use_fake_mem) {
                            owner->wr->axi_dbb->eval_timing();
                            if (owner->wr->axi_cvsram) {
                                owner->wr->axi_cvsram->eval_timing();
                            }
                        } else {
                            owner->wr->axi_dbb->eval_ram();
                            if (owner->wr->axi_cvsram) {
                                owner->wr->axi_cvsram->eval_ram();
                            }
                        }
                    }

                    outputNVDLA& output = owner->wr->tick();

                    if (owner->dma_enable) {
                        owner->try_get_dma_read_data(owner->spm_line_size);
                        if (owner->traceMode) {
                            if (owner->wr->csb->done()) {
                                // write back dirty data in spm to main memory
                                if (!owner->flushing_spm) {
                                  owner->wr->spm->clear_and_write_back_dirty();
                                  owner->flushing_spm = 1;
                                }
                                // all items have been flushed
                                // to dma write engine
                                if (owner->flushing_spm
                                  && output.dma_write_buffer.empty()) {
                                    owner->flushing_spm = 0;
                                    // printf("nvdla#%d spm flush complete!\n",
                                    //    id_nvdla);
                                }
                            }
                        }
                    }
                    owner->processOutput(output);

                #ifdef AXI_RESP_FAST_IO
                    if (Wrapper_nvdla::buf_ptr >= PB_SIZE) {
                        std::ofstream fout;
                        fout.open(print_path,
                                std::ios::out
                                | std::ios::app
                                | std::ios::binary);
                        fout.write((char*)Wrapper_nvdla::print_buffer,
                                Wrapper_nvdla::buf_ptr * sizeof(uint64_t));
                        fout.close();
                        Wrapper_nvdla::buf_ptr = 0;
                    }
                #endif
                } else {
                    if (owner->traceMode) {
                        // we have finished running the trace
                        printf("done at %lu ticks\n", owner->wr->tickcount);
                        printf("simulation time: %lu seconds\n",
                            time(nullptr) - owner->sim_time);

                        if (!owner->trace->test_passed()) {
                            printf("*** FAIL: test failed "
                                "due to output mismatch\n");

                        } else if (!owner->wr->csb->test_passed()) {
                            printf("*** FAIL: test failed "
                                "due to CSB read mismatch\n");
                        } else {
                            printf("NVDLA %d *** PASS\n", owner->id_nvdla);
                        }

                #ifdef AXI_RESP_FAST_IO
                        if (Wrapper_nvdla::buf_ptr != 0) {
                            std::ofstream fout;
                            fout.open(print_path,
                                std::ios::out
                                | std::ios::app
                                | std::ios::binary);
                            fout.write((char*)Wrapper_nvdla::print_buffer,
                                Wrapper_nvdla::buf_ptr * sizeof(uint64_t));
                            fout.close();
                            Wrapper_nvdla::buf_ptr = 0;
                        }
                #endif
                        // we send a null packet telling we have finished
                        RequestPtr req = std::make_shared<Request>(
                            owner->id_nvdla, 1, Request::UNCACHEABLE, 0);
                        PacketPtr packet = nullptr;
                        // we create the real packet, write request
                        packet = Packet::createRead(req);
                        packet->allocate();
                        packet->makeResponse();
                        owner->cpuPort.sendPacket(packet);
                    }
                }
                // check DRAM Ports
                owner->dramPort.tick();
                owner->sramPort.tick();
            }
            pkt->allocate();
            pkt->setData((uint8_t*)data);

            DPRINTF(NvDlaDeviceSE, "read req: packet size: %d, data: 0x%08x\n",
                pkt->getSize(), pkt->getLE<uint32_t>());

            owner->onRead = false;
        }
        return true;
    }

    return false;
}

void
NvDlaDeviceSE::CmdCPUSidePort::recvRespRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}
//////// EOF NvDla CODE ////////

void
NvDlaDeviceSE::MemSidePort::sendPacket(PacketPtr pkt)
{
    // Note: This flow control is very simple since the memobj is blocking.

    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    DPRINTF(NvDlaDeviceSE, "Send Mem Req to L2 %#x %d \n",
            pkt->getAddr(), pkt->getSize());

    // If we can't send the packet across the port, store it for later.
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
NvDlaDeviceSE::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Just forward to the memobj.
    return owner->handleResponse(pkt);
}

void
NvDlaDeviceSE::MemSidePort::recvReqRetry()
{
    // We should have a blocked packet if this function is called.
    assert(blockedPacket != nullptr);

    // Grab the blocked packet.
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    // Try to resend it. It's possible that it fails again.
    sendPacket(pkt);
}

void
NvDlaDeviceSE::MemSidePort::recvRangeChange()
{
    owner->sendRangeChange();
}

void
NvDlaDeviceSE::startTranslate(Addr vaddr, ContextID contextId) {

    DPRINTF(NvDlaDeviceSE, "Started translation\n");

    BaseMMU * mmu =
        system->threads[contextId]->getMMUPtr();
    assert(mmu);

    Fault fault;
    BaseMMU::Mode mode = BaseMMU::Write;
    RequestPtr req = std::make_shared<Request>(
                        vaddr, 64, 0x40, 0, 0, contextId);

    WholeTranslationState *state =
        new WholeTranslationState(req, new uint8_t[64], NULL, mode);
    DataTranslation<NvDlaDeviceSE *> *translation
        = new DataTranslation<NvDlaDeviceSE *>(this, state);

    mmu->translateTiming(req, system->threads[contextId],
                             translation, mode);

}

void
NvDlaDeviceSE::retryTranslate() {
    printf("retryTranslate at tick = %lu\n", curTick());
    startTranslate(to_retry_vaddr, 0);
}


Tick
NvDlaDeviceSE::read(PacketPtr pkt)
{
    onRead = true;
    if (!engineStarted) {
        engineStarted = true;
        //schedule(tickEvent, nextCycle() + (freq_ratio - 1) * clockPeriod());
        schedule(tickEvent,
            nextCycle() +
            static_cast<Tick>(abs((freq_ratio-1.0))*clockPeriod()));
    }

    uint32_t *data = new uint32_t;

    pkt->makeAtomicResponse();

    // interrupt->raise();

    DPRINTF(NvDlaDeviceSE, "read req: addr: 0x%08x\n",
      pkt->getAddr());

    if (pkt->getAddr() == 0x40020000) {
        *data = interruptRaised ? 1 : 0;
        pkt->setData((uint8_t*)data);

        DPRINTF(NvDlaDeviceSE, "read req: packet size: %d, data: 0x%08x\n",
            pkt->getSize(), pkt->getLE<uint32_t>());

        onRead = false;
        return 0;
    }

    uint32_t read_addr =
      (uint32_t) 0xFFFF0000 + (0x0000FFFF & ((pkt->getAddr() - 0) >> 2));

    DPRINTF(NvDlaDeviceSE, "read req: reg: 0x%08x\n", read_addr);

    wr->csb->read(read_addr, 0xffffffff, 0);
    stats.nvdla_csb_reads++;

    // half solution
    // while (!wr->csb->done()) {
    //   wr->csb->eval(waiting, data);
    //   outputNVDLA& output = wr->tick();
    // }
    // pkt->setData((uint8_t*)data);

    // wanna-be solution
    // while (!wr->csb->done()) {
    //     tick();
    // }
    // pkt->setData((uint8_t*)readData);

    // temp solution
    while (!wr->csb->done()) {
        DPRINTF(NvDlaDeviceSEDebug, "Tick NVDLA \n");
        // if we are still running trace
        // runIteration
        // schedule new iteration
        if (!wr->csb->done() || (quiesc_timer-- > 0)
                || waiting_for_gem5_mem || flushing_spm) {
            // Update stats
            // stats.nvdla_avgReqCVSRAM.sample(
            //     wr->axi_cvsram->getRequestsOnFlight());
            stats.nvdla_avgReqDBBIF.sample(wr->axi_dbb->getRequestsOnFlight());
            stats.nvdla_cycles++;
            cyclesNVDLA++;
            wr->clearOutput();

            if (interruptRaised && !wr->dla->dla_intr) {
                printf("(%lu) interrupt finished...\n", wr->tickcount);
                interruptRaised = false;
            }

            int extevent;

            if (!waiting_for_gem5_mem)
                extevent = wr->csb->eval(waiting, data);
            else
                extevent = 0;

            if (extevent == TraceLoaderGem5::TRACE_AXIEVENT
                || waiting_for_gem5_mem) {
                trace->axievent(&waiting_for_gem5_mem);
            } else if (extevent == TraceLoaderGem5::TRACE_WFI) {
                waiting = 1;
        #ifndef AXI_RESP_FAST_IO
                printf("(%lu) waiting for interrupt...\n", wr->tickcount);
        #endif
            } else if (extevent == TraceLoaderGem5::TRACE_RESET) {
                wr->init();
        #ifndef AXI_RESP_FAST_IO
                printf("nvdla#%d reset\n", id_nvdla);
        #endif
            }

            if (traceMode) {
                if (waiting && wr->dla->dla_intr) {
        #ifndef AXI_RESP_FAST_IO
                    printf("(%lu) nvdla#%d interrupt!\n", wr->tickcount,
                        id_nvdla);
        #endif
                    waiting = 0;
                }
            } else {
                if (wr->dla->dla_intr) {
                    if (!interruptRaised) {
        #ifndef AXI_RESP_FAST_IO
                        printf("(%lu) nvdla#%d interrupt!\n", wr->tickcount,
                            id_nvdla);
        #endif
                        // interrupt->raise();
                        interruptRaised = true;
                    }
                }
            }

            if (!waiting_for_gem5_mem) {
                if (!use_fake_mem) {
                    wr->axi_dbb->eval_timing();
                    if (wr->axi_cvsram) {
                        wr->axi_cvsram->eval_timing();
                    }
                } else {
                    wr->axi_dbb->eval_ram();
                    if (wr->axi_cvsram) {
                        wr->axi_cvsram->eval_ram();
                    }
                }
            }

            outputNVDLA& output = wr->tick();

            if (dma_enable) {
                try_get_dma_read_data(spm_line_size);
                if (traceMode) {
                    if (wr->csb->done()) {
                        // write back dirty data in spm to main memory
                        if (!flushing_spm) {
                            wr->spm->clear_and_write_back_dirty();
                            flushing_spm = 1;
                        }
                        // all items have been flushed to dma write engine
                        if (flushing_spm && output.dma_write_buffer.empty()) {
                            flushing_spm = 0;
                            // printf("nvdla#%d spm flush complete!\n",
                            //   id_nvdla);
                        }
                    }
                }
            }
            processOutput(output);

        #ifdef AXI_RESP_FAST_IO
            if (Wrapper_nvdla::buf_ptr >= PB_SIZE) {
                std::ofstream fout;
                fout.open(print_path,
                        std::ios::out | std::ios::app | std::ios::binary);
                fout.write((char*)Wrapper_nvdla::print_buffer,
                        Wrapper_nvdla::buf_ptr * sizeof(uint64_t));
                fout.close();
                Wrapper_nvdla::buf_ptr = 0;
            }
        #endif
        } else {
            if (traceMode) {
                // we have finished running the trace
                printf("done at %lu ticks\n", wr->tickcount);
                printf("simulation time: %lu seconds\n",
                    time(nullptr) - sim_time);

                if (!trace->test_passed()) {
                    printf("*** FAIL: test failed due to output mismatch\n");

                } else if (!wr->csb->test_passed()) {
                    printf("*** FAIL: test failed due to CSB read mismatch\n");
                } else {
                    printf("NVDLA %d *** PASS\n", id_nvdla);
                }

        #ifdef AXI_RESP_FAST_IO
                if (Wrapper_nvdla::buf_ptr != 0) {
                    std::ofstream fout;
                    fout.open(print_path,
                        std::ios::out | std::ios::app | std::ios::binary);
                    fout.write((char*)Wrapper_nvdla::print_buffer,
                        Wrapper_nvdla::buf_ptr * sizeof(uint64_t));
                    fout.close();
                    Wrapper_nvdla::buf_ptr = 0;
                }
        #endif
                // we send a null packet telling we have finished
                RequestPtr req = std::make_shared<Request>(id_nvdla, 1,
                                                    Request::UNCACHEABLE, 0);
                PacketPtr packet = nullptr;
                // we create the real packet, write request
                packet = Packet::createRead(req);
                packet->allocate();
                packet->makeResponse();
                cpuPort.sendPacket(packet);
            }
        }
        // check DRAM Ports
        dramPort.tick();
        sramPort.tick();
    }
    pkt->setData((uint8_t*)data);

    DPRINTF(NvDlaDeviceSE, "read req: packet size: %d, data: 0x%08x\n",
        pkt->getSize(), pkt->getLE<uint32_t>());

    onRead = false;

    return 0;
}

Tick
NvDlaDeviceSE::write(PacketPtr pkt)
{
    pkt->makeAtomicResponse();

    uint32_t write_addr =
      (uint32_t) 0xFFFF0000 + (0x0000FFFF & ((pkt->getAddr() - 0) >> 2));

    DPRINTF(NvDlaDeviceSE, "write req: addr: 0x%08x, data: 0x%08x\n",
      write_addr, pkt->getLE<uint32_t>());

    wr->csb->write(write_addr, pkt->getLE<uint32_t>());
    stats.nvdla_csb_writes++;

    if (write_addr == 0xFFFF0003) {
        // interrupt->clear();
    }

    return 0;
}

} //End namespace gem5
