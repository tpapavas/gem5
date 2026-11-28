#include <iostream>

#include "params/TLM_ScNvDlaSE.hh"
#include "sc_nvdla_se.hh"
#include "sim/sim_exit.hh"

using namespace std;
using namespace sc_core;
using namespace gem5;

ScNvDlaSE *
gem5::TLM_ScNvDlaSEParams::create() const
{
    ScNvDlaSE *sc_nvdla_se = new ScNvDlaSE(name.c_str(), cpu_freq, freq_ratio, nvdla_id);
    // [SC_SPM]
    ScNvDlaSpmConfig spm_cfg;
    spm_cfg.dma_enable = dma_enable;
    spm_cfg.spm_size = spm_size;
    spm_cfg.spm_line_size = spm_line_size;
    spm_cfg.spm_latency = spm_latency;
    spm_cfg.assoc = (assoc == "full") ? 0 : (uint32_t)std::stoul(assoc);
    spm_cfg.use_shared_spm = use_shared_spm;
    spm_cfg.buffer_mode = buffer_mode;
    spm_cfg.prefetch_enable = prefetch_enable;
    spm_cfg.mem_fill_chunk = mem_fill_chunk;
    sc_nvdla_se->configureSpm(spm_cfg);
    std::cout << "[gem5::TLM_ScNvDlaSEParams::create] "
              "Created ScNvDlaSE instance with name: " << name
              << ", cpu_freq: " << cpu_freq
              << ", freq_ratio: " << freq_ratio
              << ", nvdla_id: " << nvdla_id
              << std::endl;
    return sc_nvdla_se;
}

gem5::Port &ScNvDlaSE::gem5_getPort(const std::string &if_name, int idx)
{
    // return wrapper;

    if (if_name == "csb_target")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] "
            "csb_target\n";
        return csb_wrapper; // CSB from CPU
    }

    if (if_name == "dbb_init")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] "
            "dbb_init_wrapper\n";
        // DRAM port
        return dbb_init_wrapper;
    }

    if (if_name == "sram_init")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] "
            "sram_init_wrapper\n";
        // DRAM port
        return sram_init_wrapper;
    }

    if (if_name == "dbb_target")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] "
            "dbb_target_wrapper\n";
        // DRAM port
        return dbb_target_wrapper;
    }

    if (if_name == "sram_target")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] "
            "sram_target_wrapper\n";
        // DRAM port
        return sram_target_wrapper;
    }
}

void ScNvDlaSE::handle_irq()
{
    if (irq.read())
    {
        std::cout << "[NVDLA IRQ] Interrupt raised @ "
                  << sc_time_stamp() << std::endl;
        interruptRaised = true;
    }
    else if (irq.read() == 0)
    {
        interruptRaised = false;
        std::cout << "[NVDLA IRQ] Interrupt reset @ "
                  << sc_time_stamp() << std::endl;
    }
}

void ScNvDlaSE::b_transport_csb(tlm::tlm_generic_payload &trans,
                                sc_time &delay)
{
    std::cout << "[SystemC RECEIVE] # " << nvdlaId << " CSB addr=0x"
              << std::hex << trans.get_address()
              << " cmd="
              << (trans.is_read() ? "READ" : "WRITE")
              << " time=" << std::dec << sc_time_stamp().value()
              << std::endl;

    uint32_t data;
    uint32_t data2;
    if (trans.is_read())
    {
        gNvdlaStats[nvdlaId].csbReads++;
        if (trans.get_address() == 0x20000)
        {
            // data = irq.read() ? 1 : 0;
            // interruptRaised = false;
            data = interruptRaised ? 1 : 0;

            if (data == 1) {
                gNvdlaStats[nvdlaId].interruptsRaised++;
            }
            memcpy(trans.get_data_ptr(), &data, 4);
            std::cout << "[NVDLA POLL] CSB read 0x20000 -> "
                      << data << std::endl;
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }

        if (trans.get_address() == 0x20004)
        {
            data2 = 1;
            interruptRaised = false;
            // [SC_SPM] end of an NVDLA task: write back the dirty bytes and
            // empty the buffer (Verilator NvDlaDeviceSE: netFinished), so
            // the CPU reads the outputs from memory and the next task sees
            // the inputs the CPU wrote
            if (spm) {
                spm->flush(*this);
                pftKick.notify();
            }
            memcpy(trans.get_data_ptr(), &data2, 4);
            std::cout << "[NVDLA] CSB read 0x20004 -> "
                      << data2 << std::endl;
            trans.set_response_status(tlm::TLM_OK_RESPONSE);
            return;
        }

        printf("[ScNvDlaSE::b_transport] CSB is read \n");
        data = initiator.read_reg(trans.get_address());

        std::cout << "[ScNvDlaSE::b_transport] CSB data are : "
                  << std::hex << data << "\n";
        memcpy(trans.get_data_ptr(), &data, 4);
        trans.set_response_status(tlm::TLM_OK_RESPONSE);

        std::cout << "[ScNvDlaSE::b_transport] CSB memCpy done\n";
    }
    else if (trans.is_write())
    {
        gNvdlaStats[nvdlaId].csbWrites++;
        printf("[ScNvDlaSE::b_transport] CSB is write\n");

        uint32_t addr = trans.get_address();

        uint32_t data;
        memcpy(&data, trans.get_data_ptr(), 4);

        std::cout << "[WRITE] addr=0x" << std::hex << addr
                  << " data=0x" << data << std::endl;

        snoopCsbWriteForPrefetch(addr, data);   // [SC_SPM]

        // Pass pointer if API expects bytes
        initiator.write_reg(addr, (uint8_t*)&data);

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
}

tlm::tlm_sync_enum ScNvDlaSE::nb_transport_fw(
    tlm::tlm_generic_payload &trans,
    tlm::tlm_phase &phase,
    sc_time &delay)
{
    printf("tlm::tlm_sync_enum ScNvDlaSE::nb_transport_fw\n");

    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);

    return dbb_init->nb_transport_fw(trans, phase, delay);
}

tlm::tlm_sync_enum ScNvDlaSE::nb_transport_bw(
    tlm::tlm_generic_payload &trans,
    tlm::tlm_phase &phase,
    sc_time &delay)
{
    printf("tlm::tlm_sync_enum ScNvDlaSE::nb_transport_bw\n");
    return dbb_target->nb_transport_bw(trans, phase, delay);
}

void ScNvDlaSE::b_transport_dbb(tlm::tlm_generic_payload &trans,
                                sc_time &delay)
{

    std::cout << "[SystemC RECEIVE] #" << nvdlaId << " DBB addr=0x"
              << std::hex << trans.get_address()
              << " cmd=" << (trans.is_read() ? "READ" : "WRITE")
              << " byte_enable_ptr=" << (void *)trans.get_byte_enable_ptr()
              << " bel=" << trans.get_byte_enable_length()
              << " time=" << std::dec << sc_time_stamp().value() << std::endl;


    if (spm) {   // [SC_SPM] through the embedded buffer
        const uint64_t addr = trans.get_address();
        const uint32_t len = trans.get_data_length();
        if (spmCfg.prefetch_enable) {
            if (trans.is_read())
                logDemandRead(addr, len);
            pftKick.notify();
        }
        if (trans.is_read()) {
            const uint32_t hits =
                spm->read(*this, addr, trans.get_data_ptr(), len);
            // buffer latency of the beats served by the buffer
            if (hits)
                wait(hits * spmCfg.spm_latency *
                     gNvdlaStats[nvdlaId].nvdlaClockPeriod);
        } else {
            spm->write(*this, addr, trans.get_data_ptr(), len,
                       trans.get_byte_enable_ptr(),
                       trans.get_byte_enable_length());
        }
        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    } else {
        dbb_init->b_transport(trans, delay); // TO GEM5
    }

    std::cout << "[DBB RSP] addr=0x" << std::hex << trans.get_address()
              << " response=" << trans.get_response_string()
              << " time=" << std::dec << sc_time_stamp().value() << std::endl;

    if (trans.is_read()
      && trans.get_response_status() == tlm::TLM_OK_RESPONSE)
    {
        unsigned char *data_ptr = trans.get_data_ptr();
        unsigned int data_len = trans.get_data_length();
        gNvdlaStats[nvdlaId].dbbReads++;
        gNvdlaStats[nvdlaId].dbbReadBytes += data_len;

        std::cout << "[DBB DATA READ] addr=0x" << std::hex
                  << trans.get_address()
                  << " length=" << std::dec << data_len << " bytes:"
                  << std::endl;

        for (unsigned int i = 0; i < data_len; ++i)
        {
            if (i % 16 == 0 && i > 0)
                std::cout << std::endl;
            if (i % 16 == 0)
                std::cout << "  0x"
                          << std::setw(4) << std::setfill('0')
                          << std::hex << i << ": ";
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << (int)data_ptr[i] << " ";
        }
        std::cout << std::dec << std::endl
                  << std::endl;
    }
    else if (trans.is_write()
      && trans.get_response_status() == tlm::TLM_OK_RESPONSE)
    {
        unsigned char *data_ptr = trans.get_data_ptr();
        unsigned int data_len = trans.get_data_length();

        gNvdlaStats[nvdlaId].dbbWrites++;
        gNvdlaStats[nvdlaId].dbbWriteBytes += data_len;

        std::cout << "[DBB DATA WRITE] addr=0x" << std::hex
                  << trans.get_address()
                  << " length=" << std::dec << data_len << " bytes:"
                  << std::endl;

        for (unsigned int i = 0; i < data_len; ++i)
        {
            if (i % 16 == 0 && i > 0)
                std::cout << std::endl;
            if (i % 16 == 0)
                std::cout << "  0x"
                          << std::setw(4) << std::setfill('0')
                          << std::hex << i << ": ";
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                      << (int)data_ptr[i] << " ";
        }
        std::cout << std::dec << std::endl
                  << std::endl;
    }
    else if (trans.get_response_status() == tlm::TLM_BURST_ERROR_RESPONSE ||
             trans.get_response_status() == tlm::TLM_ADDRESS_ERROR_RESPONSE ||
             trans.get_response_status() == tlm::TLM_COMMAND_ERROR_RESPONSE ||
             trans.get_response_status() == tlm::TLM_GENERIC_ERROR_RESPONSE)
    {
        std::cerr << "[DBB ERROR] Bad response\n";
    }
}



void ScNvDlaSE::b_transport_sram(tlm::tlm_generic_payload &trans,
                                 sc_time &delay)
{
    std::cout << "[SystemC RECEIVE] #" << nvdlaId << " SRAM addr=0x"
              << std::hex << trans.get_address()
              << " cmd="
              << (trans.is_read() ? "READ" : "WRITE")
              << " time=" << std::dec << sc_time_stamp().value()
              << std::endl;
}




// ---------------------------------------------------------------------------
// [SC_SPM] embedded buffer and weight prefetcher
// ---------------------------------------------------------------------------

#ifndef NVDLA_CDMA_REG_BASE
#define NVDLA_CDMA_REG_BASE 0x5000   // nv_full (nv_small: 0x3000, SConscript)
#endif

sc_nvdla_spm::EmbeddedBuffer *ScNvDlaSE::sharedSpm = nullptr;

void
ScNvDlaSE::configureSpm(const ScNvDlaSpmConfig &cfg)
{
    spmCfg = cfg;
    if (!cfg.dma_enable) {
        spmCfg.prefetch_enable = false;   // as Verilator: prefetch needs DMA
        return;
    }
    const uint32_t num_lines = cfg.spm_size / cfg.spm_line_size;
    const uint32_t ways = cfg.assoc ? cfg.assoc : num_lines;
    if (cfg.buffer_mode > sc_nvdla_spm::BUF_MODE_PFT_CUTOFF)
        SC_REPORT_FATAL("ScNvDlaSE", "invalid buffer_mode");
    if (cfg.use_shared_spm && sharedSpm) {
        spm = sharedSpm;
    } else {
        spm = new sc_nvdla_spm::EmbeddedBuffer(
            (sc_nvdla_spm::BufMode)cfg.buffer_mode, cfg.spm_line_size,
            num_lines, ways, cfg.mem_fill_chunk);
        if (cfg.use_shared_spm)
            sharedSpm = spm;
    }
    std::cout << "[SC_SPM] nvdla#" << nvdlaId
              << " embedded buffer: " << cfg.spm_size << " B, line "
              << cfg.spm_line_size << " B, " << num_lines << " lines, "
              << ways << "-way, mode " << cfg.buffer_mode
              << (cfg.use_shared_spm ? ", shared" : ", private")
              << ", latency " << cfg.spm_latency << " cycles"
              << ", prefetch " << (cfg.prefetch_enable ? "on" : "off")
              << ", CDMA regs 0x" << std::hex << NVDLA_CDMA_REG_BASE
              << std::dec << std::endl;
    if (spmCfg.prefetch_enable) {
        sc_spawn(sc_bind(&ScNvDlaSE::prefetchThread, this),
                 sc_gen_unique_name("spm_prefetch"));
    }
}

void
ScNvDlaSE::memAccess(tlm::tlm_command cmd, uint64_t addr, uint8_t *data,
                     uint32_t len)
{
    // the TLM-to-gem5 bridge reads the byte enables of every transaction
    std::vector<uint8_t> be(len, TLM_BYTE_ENABLED);
    tlm::tlm_generic_payload gp;
    gp.set_command(cmd);
    gp.set_address(addr);
    gp.set_data_ptr(data);
    gp.set_data_length(len);
    gp.set_streaming_width(len);
    gp.set_byte_enable_ptr(be.data());
    gp.set_byte_enable_length(len);
    gp.set_dmi_allowed(false);
    gp.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    sc_time t = SC_ZERO_TIME;
    // the demand path, the prefetcher and a flush may use the port at the
    // same time: one request at a time (see memReadChunks)
    dbbPortMutex.lock();
    dbb_init->b_transport(gp, t);   // blocks for the memory time
    dbbPortMutex.unlock();
    if (gp.get_response_status() != tlm::TLM_OK_RESPONSE)
        std::cerr << "[SC_SPM] nvdla#" << nvdlaId << " memory "
                  << (cmd == tlm::TLM_READ_COMMAND ? "read" : "write")
                  << " error at 0x" << std::hex << addr << std::dec
                  << " len " << len << std::endl;
}

void
ScNvDlaSE::memRead(uint64_t addr, uint8_t *data, uint32_t len)
{
    spmStatsData.mem_reads++;
    spmStatsData.mem_read_bytes += len;
    memAccess(tlm::TLM_READ_COMMAND, addr, data, len);
}

void
ScNvDlaSE::memWrite(uint64_t addr, const uint8_t *data, uint32_t len)
{
    spmStatsData.mem_writes++;
    spmStatsData.mem_write_bytes += len;
    memAccess(tlm::TLM_WRITE_COMMAND, addr, const_cast<uint8_t *>(data),
              len);
}

void
ScNvDlaSE::memReadChunks(uint64_t addr, uint8_t *data, uint32_t len,
                         uint32_t chunk)
{
    // One request at a time: the DBB port is a TLM socket whose initiator
    // must not start a request before the previous one is accepted, and the
    // CMOD AXI adaptor issues the NVDLA DBB beats one by one as well.
    for (uint32_t off = 0; off < len; off += chunk)
        memRead(addr + off, data + off, std::min(chunk, len - off));
}

// Same registers as NvDlaDeviceSE::snoopCsbWriteForPrefetch (Verilator), at
// the CDMA base of the configuration: D_OP_ENABLE +0x10, D_MISC_CFG +0x14,
// D_WEIGHT_ADDR_HIGH +0x78, D_WEIGHT_ADDR_LOW +0x7c, D_WEIGHT_BYTES +0x80.
// The firmware programs op N+1 while op N runs, so the weights of the next op
// are prefetched during the current one.
void
ScNvDlaSE::snoopCsbWriteForPrefetch(uint32_t addr, uint32_t data)
{
    if (!spmCfg.prefetch_enable)
        return;
    const uint32_t base = NVDLA_CDMA_REG_BASE;
    switch (addr & 0xFFFF) {
      case base + 0x14: pftCdmaMiscCfg = data; break;
      case base + 0x78: pftWtAddrHi = data; break;
      case base + 0x7c: pftWtAddrLo = data; break;
      case base + 0x80: pftWtBytes = data; break;
      case base + 0x10: {
        const bool weight_reuse = (pftCdmaMiscCfg >> 20) & 1;
        if ((data & 1) && !weight_reuse && pftWtBytes) {
            const uint64_t a = (pftWtAddrHi << 32) | pftWtAddrLo;
            rdVarLog.push_back({a, pftWtBytes, 0});
            spmStatsData.pft_log_entries++;
            std::cout << "[PFT_LOG] nvdla#" << nvdlaId
                      << " weight tensor addr 0x" << std::hex << a
                      << " bytes 0x" << pftWtBytes << std::dec
                      << " (entry " << spmStatsData.pft_log_entries << ")"
                      << std::endl;
        }
        break;
      }
      default: break;
    }
}

// AXIResponder::log_req_issue: a demand read of a logged tensor moves its
// issue point, so the prefetcher does not fetch what NVDLA already read
void
ScNvDlaSE::logDemandRead(uint64_t addr, uint32_t len)
{
    for (auto it = rdVarLog.begin(); it != rdVarLog.end(); ++it) {
        if (it->addr <= addr && addr < it->addr + it->len) {
            const uint64_t end = addr + len - it->addr;
            if (end > it->issued)
                it->issued = std::min<uint64_t>(end, it->len);
            if (it->issued >= it->len)
                rdVarLog.erase(it);
            break;
        }
    }
}

// AXIResponder::generate_prefetch_request: one line of the oldest logged
// tensor. [PFT_EDGE] lines not entirely inside the tensor are not prefetched
// (they may hold activations NVDLA writes later).
bool
ScNvDlaSE::generatePrefetch()
{
    if (rdVarLog.empty())
        return false;
    RdVarLogEntry &e = rdVarLog.front();
    const uint32_t L = spm->lineSize();
    const uint64_t to_issue = e.addr + e.issued;
    const uint64_t line = to_issue & ~(uint64_t)(L - 1);
    const bool partial = line < e.addr || line + L > e.addr + e.len;
    if (!partial && !spm->hasLine(line) && !spm->inFlight(line)) {
        if (spmCfg.buffer_mode == sc_nvdla_spm::BUF_MODE_PFT_CUTOFF &&
            spm->setFull(line))
            return false;   // throttled: retry on a later kick
        // the line counts as issued before the (blocking) fetch, so a
        // demand read meanwhile does not move the log back
        e.issued += line + L - to_issue;
        if (e.issued >= e.len)
            rdVarLog.pop_front();
        spm->prefetchLine(*this, line);
        return true;
    }
    e.issued += line + L - to_issue;
    if (e.issued >= e.len)
        rdVarLog.pop_front();
    return true;
}

void
ScNvDlaSE::prefetchThread()
{
    while (true) {
        wait(pftKick);
        // lines already covered (partial, present, in flight) cost nothing:
        // skip them and issue at most one fetch per kick
        const uint64_t issued = spmStatsData.prefetch_issued;
        while (generatePrefetch() &&
               spmStatsData.prefetch_issued == issued) {
        }
    }
}
