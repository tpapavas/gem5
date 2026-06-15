#include <iostream>

#include "params/TLM_ScNvDlaSE.hh"
#include "systemC_NVDLA/sc_nvdla_se.hh"
#include "sim/sim_exit.hh"

using namespace std;
using namespace sc_core;
using namespace gem5;

ScNvDlaSE *
gem5::TLM_ScNvDlaSEParams::create() const
{
    ScNvDlaSE *sc_nvdla_se = new ScNvDlaSE(name.c_str(), cpu_freq, freq_ratio);
    return sc_nvdla_se;
}

gem5::Port &ScNvDlaSE::gem5_getPort(const std::string &if_name, int idx)
{
    // return wrapper;

    if (if_name == "csb_target")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] csb_target\n";
        return csb_wrapper; // CSB from CPU
    }

    if (if_name == "dbb_init")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] dbb_init_wrapper\n";
        // DRAM port
        return dbb_init_wrapper;
    }

    if (if_name == "sram_init")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] sram_init_wrapper\n";
        // DRAM port
        return sram_init_wrapper;
    }

    if (if_name == "dbb_target")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] dbb_target_wrapper\n";
        // DRAM port
        return dbb_target_wrapper;
    }

    if (if_name == "sram_target")
    {
        std::cout << "[gem5::Port &ScNvDlaSE::gem5_getPort] sram_target_wrapper\n";
        // DRAM port
        return sram_target_wrapper;
    }
}


void ScNvDlaSE::handle_irq()
{
    if (irq.read())
    {
        std::cout << "[NVDLA IRQ] Interrupt raised @ " << sc_time_stamp() << std::endl;
        interruptRaised = true;

    }
    else if (irq.read() == 0)
    {
        interruptRaised = false;
        std::cout << "[NVDLA IRQ] Interrupt reset @ " << sc_time_stamp() << std::endl;
    }

}

void ScNvDlaSE::b_transport_csb(tlm::tlm_generic_payload &trans,
                                sc_time &delay)
{
    std::cout << "[SystemC RECEIVE] CSB addr=0x"
              << std::hex << trans.get_address()
              << " cmd="
              << (trans.is_read() ? "READ" : "WRITE")
              << " time=" << std::dec << sc_time_stamp().value()
              << std::endl;

    uint32_t data;
    uint32_t data2;
    if (trans.is_read())
    {   gNvdlaStats.csbReads++;
        if (trans.get_address() == 0x20000)
        {
            // data = irq.read() ? 1 : 0;
            // interruptRaised = false;
            data = interruptRaised ? 1 : 0;
            if(data == 1){
                gNvdlaStats.interruptsRaised++;
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

        std::cout << "[ScNvDlaSE::b_transport] CSB data are : " << std::hex << data << "\n";
        memcpy(trans.get_data_ptr(), &data, 4);
        std::cout << "[ScNvDlaSE::b_transport] CSB memCpy done\n";
    }
    else if (trans.is_write())
    {
        gNvdlaStats.csbWrites++;
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



void ScNvDlaSE::b_transport_dbb(tlm::tlm_generic_payload &trans,
                                sc_time &delay)
{
    std::cout << "[SystemC RECEIVE] DBB addr=0x"
              << std::hex << trans.get_address()
              << " cmd=" << (trans.is_read() ? "READ" : "WRITE")
              << " byte_enable_ptr=" << (void *)trans.get_byte_enable_ptr()
              << " bel=" << trans.get_byte_enable_length()
              << " time=" << std::dec << sc_time_stamp().value() << std::endl;

    unsigned char *be = trans.get_byte_enable_ptr();
    unsigned int bel = trans.get_byte_enable_length();
    trans.set_byte_enable_ptr(nullptr);
    trans.set_byte_enable_length(0);

    dbb_init->b_transport(trans, delay); // TO GEM5
    trans.set_byte_enable_ptr(be);
    trans.set_byte_enable_length(bel);


    std::cout << "[DBB RSP] addr=0x" << std::hex << trans.get_address()
              << " response=" << trans.get_response_string()
              << " time=" << std::dec << sc_time_stamp().value() << std::endl;

    if (trans.is_read() && trans.get_response_status() == tlm::TLM_OK_RESPONSE)
    {

        unsigned char *data_ptr = trans.get_data_ptr();
        unsigned int data_len = trans.get_data_length();
        gNvdlaStats.dbbReads++;
        gNvdlaStats.dbbReadBytes += data_len;

        std::cout << "[DBB DATA READ] addr=0x" << std::hex << trans.get_address()
                  << " length=" << std::dec << data_len << " bytes:" << std::endl;

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

    else if (trans.is_write() && trans.get_response_status() == tlm::TLM_OK_RESPONSE)
    {
        unsigned char *data_ptr = trans.get_data_ptr();
        unsigned int data_len = trans.get_data_length();

        gNvdlaStats.dbbWrites++;
        gNvdlaStats.dbbWriteBytes += data_len;

        std::cout << "[DBB DATA WRITE] addr=0x" << std::hex << trans.get_address()
                  << " length=" << std::dec << data_len << " bytes:" << std::endl;

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
    std::cout << "[SystemC RECEIVE] SRAM addr=0x"
              << std::hex << trans.get_address()
              << " cmd="
              << (trans.is_read() ? "READ" : "WRITE")
              << " time=" << std::dec << sc_time_stamp().value()
              << std::endl;
}

/*
void ScNvDlaSE::b_transport_dbb(tlm::tlm_generic_payload &trans,
                            sc_time &delay)
{
     std::cout << "[SystemC RECEIVE] DBB addr=0x"
              << std::hex << trans.get_address()
              << " cmd=" << (trans.is_read() ? "READ" : "WRITE")
              << " byte_enable_ptr=" << (void*)trans.get_byte_enable_ptr()
              << " bel=" << trans.get_byte_enable_length()
              << " time=" << sc_time_stamp() << std::endl;

    // Critical fix: NVDLA may set byte enables, but gem5 memory doesn't support them
    if (trans.get_byte_enable_ptr() != nullptr) {
        std::cout << "[DBB FIX] Clearing unsupported byte enables" << std::endl;
        trans.set_byte_enable_ptr(nullptr);
        trans.set_byte_enable_length(0);
    }

    dbb_init->b_transport(trans, delay);

    if (trans.is_read() && trans.get_response_status() == tlm::TLM_OK_RESPONSE) {
        unsigned char* data_ptr = trans.get_data_ptr();
        unsigned int data_len   = trans.get_data_length();

        std::cout << "[DBB DATA READ] addr=0x" << std::hex << trans.get_address()
                  << " length=" << std::dec << data_len << " bytes:" << std::endl;

        // Print in hex, 16 bytes per line (very readable)
        for (unsigned int i = 0; i < data_len; ++i) {
            if (i % 16 == 0 && i > 0) std::cout << std::endl;
            if (i % 16 == 0) std::cout << "  0x" << std::setw(4) << std::setfill('0') << std::hex << (i) << ": ";
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data_ptr[i] << " ";
        }
        std::cout << std::dec << std::endl << std::endl;
    }

    std::cout << "[DBB RSP] addr=0x" << std::hex << trans.get_address()
              << " response=" << trans.get_response_string()
              << " time=" << sc_time_stamp() << std::endl;
}
*/
//tlm::tlm_sync_enum ScNvDlaSE::nb_transport_fw(
//    tlm::tlm_generic_payload &trans,
//    tlm::tlm_phase &phase,
//    sc_time &delay)
//{
//    printf("tlm::tlm_sync_enum ScNvDlaSE::nb_transport_fw\n");
//}
//
//tlm::tlm_sync_enum ScNvDlaSE::nb_transport_bw(
//    tlm::tlm_generic_payload &trans,
//    tlm::tlm_phase &phase,
//    sc_time &delay)
//{
//    printf("tlm::tlm_sync_enum ScNvDlaSE::nb_transport_bw\n");
//}
//

//
//
// tlm::tlm_sync_enum
// ScNvDlaSE::nb_transport_fw(tlm::tlm_generic_payload &trans,
//                           tlm::tlm_phase &phase,
//                           sc_core::sc_time &delay)
//{
//    if (phase == tlm::BEGIN_REQ)
//    {
//        uint64_t addr = trans.get_address();
//        unsigned char *data_ptr = trans.get_data_ptr();
//
//        std::cout << "[NB_FW] addr=0x"
//                  << std::hex << addr
//                  << " cmd=" << (trans.is_read() ? "READ" : "WRITE")
//                  << std::endl;
//
//        if (trans.is_read())
//        {
//            uint32_t data = initiator.read_reg_fw(addr);
//            std::memcpy(data_ptr, &data, 4);
//        }
//        else if (trans.is_write())
//        {
//            //uint32_t data;
//            //std::memcpy(&data, data_ptr, 4);
//            //initiator.write_reg(addr, data);
//        }
//
//        trans.set_response_status(tlm::TLM_OK_RESPONSE);
//
//        phase = tlm::BEGIN_RESP;
//        return tlm::TLM_UPDATED;
//    }
//
//    if (phase == tlm::END_RESP)
//    {
//        return tlm::TLM_COMPLETED;
//    }
//
//    return tlm::TLM_ACCEPTED;
//}

// Port &
// ScNvDlaSE::getPort(const std::string &if_name, PortID idx)
// {
//     if (if_name == "accel_port")
//         return accel_port;
//     // if (if_name == "csb")
//     //     return csbBridge->gem5_getPort("gem5", idx);
//     // if (if_name == "sram")
//     //     return sramBridge->gem5_getPort("gem5", idx);
//     // if (if_name == "dbb")
//     //     return dbbBridge->gem5_getPort("gem5", idx);
//     return SimObject::getPort(if_name, idx);
// }

// void ScNvDlaSE::startup()
// {
//     // schedule(event, curTick() + 1);
//     schedule(event, curTick() + tickPeriod);
// }

// void ScNvDlaSE::tick()
// {

//     DPRINTF(NvDlaDevice, "SystemC tick @: %d Gem5 tick @: %d\n",
//             sc_core::sc_time_stamp(), curTick());

//     sc_core::sc_time duration = sc_core::sc_time::from_value(tickPeriod);

//     // sc_core::sc_start(duration);
//     //::sc_gem5::scheduler.oneCycle();

//     // mod->nvdla->print(std::cout); // prints NVDLA hierarchy
//     // mod->nvdla->dump(std::cout);  // more detailed
//     // auto &children = mod->nvdla->get_child_objects();
//     // std::cout << "NVDLA has " << children.size() << " child objects\n";
//     // for (auto child : mod->nvdla->get_child_objects()) {
//     //    std::cout << "  - " << child->name() << "  kind: " << child->kind() << "\n";
//     // }
//     // exitSimLoop("done");
//     panic_if(sc_time_stamp() > sc_time::from_value(curTick()), "Somethink wrong with systemC timing");
//     // sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
//     // if (mod->irq.read())
//     // {
//     //     exitSimLoop("done");
//     //     return;
//     // }

//     schedule(event, curTick() + tickPeriod);
// }

// ScNVDLAModule *
// ScNVDLAModuleParams::create() const
// {
//     std::cout << "Called ScNVDLAModule create()\n";
//     return new ScNVDLAModule(name.c_str());
// }

// bool ScNvDlaSE::AccelSlavePort::recvTimingReq(PacketPtr pkt)
// {
//     std::cout << " NOhinkg send to nvdla.\n";
//     std::cout << " SystemC Time @ " << sc_core::sc_time_stamp() << "\n";
//     // uint32_t value = owner->mod->initSocket.read_reg(0x9004);
//     owner->initiator->read_reg(0x9004);

//     return true;
// }

// } // namespace gem5
