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


    dbb_init->b_transport(trans, delay); // TO GEM5

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


