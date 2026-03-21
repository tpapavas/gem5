#ifndef __ACCELERATORS_SC_NVDLA_SE_HH__
#define __ACCELERATORS_SC_NVDLA_SE_HH__

#include <tlm_utils/multi_passthrough_initiator_socket.h>
#include <tlm_utils/multi_passthrough_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <tlm>
#include <vector>

#include "NV_nvdla.h"
#include "base/trace.hh"
#include "debug/NvDlaDevice.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "mem/port.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"
#include "systemc/ext/channel/sc_buffer.hh"
#include "systemc/ext/core/sc_module.hh"
#include "systemc/tlm_bridge/gem5_to_tlm.hh"
#include "systemc/tlm_bridge/tlm_to_gem5.hh"
#include "systemc/tlm_port_wrapper.hh"

// #include "params/TLM_ScNvDlaSE.hh"
#include "systemc/ext/systemc"
#include "systemc/ext/tlm"
#include "systemC_NVDLA/sc_tlm_initiator.hh"
#include "systemC_NVDLA/sc_tlm_target.hh"

// #include "systemC_NVDLA/sc_nvdla.hh"

using namespace std;
using namespace sc_core;
using namespace gem5;

// namespace gem5
// {

// SC_MODULE(ScNvDlaSE)
class ScNvDlaSE : public sc_core::sc_module
{
public:
    Initiator initiator;
    Target target1;
    Target target2;

    scsim::cmod::NV_nvdla *nvdla;

    // Interrupt signal from NVDLA
    sc_core::sc_signal<bool> irq;


    tlm_utils::simple_target_socket<ScNvDlaSE> tSocket;
    sc_gem5::TlmTargetWrapper<32> wrapper;

public:
    SC_HAS_PROCESS(ScNvDlaSE);
    ScNvDlaSE(sc_module_name name) :
        sc_module(name),
        initiator("dla_tlm_initiator"),
        tSocket("tSocket"),
        wrapper(tSocket, std::string(name) + ".tlm", InvalidPortID),
        target1("dla_tlm_target_1"),
        target2("dla_tlm_target_2")
    {

        // tickPeriod = 500;
        // panic_if(tickPeriod == 0, "Clock period cannot be zero!");

        std::cout << "ScNvDlaSE SystemC tick @: " << sc_core::sc_time_stamp()
                << " Gem5 tick @: " << curTick() << "\n";



        //////////////////////////////////

        tSocket.register_b_transport(this, &ScNvDlaSE::b_transport);

        nvdla = new scsim::cmod::NV_nvdla("nvdla_core");
        // initiator.tSocket.bind(
        //  static_cast<tlm::tlm_target_socket<
        //   32,tlm::tlm_base_protocol_types, 0, sc_core::SC_ONE_OR_MORE_BOUND>
        //  >>(nvdla->nvdla_host_master_if));
        initiator.tSocket.bind(nvdla->nvdla_host_master_if);
        nvdla->nvdla_core2dbb_axi4.bind(target1.tSocket);
        nvdla->nvdla_core2cvsram_axi4.bind(target2.tSocket);

        nvdla->nvdla_intr(irq);

        ////////////////////////////////////////////


        printf("[ScNvDlaSE] All bindings done\n");
    }


    gem5::Port &gem5_getPort(const std::string &if_name, int idx=-1) override;

    virtual void b_transport(tlm::tlm_generic_payload& trans,
                             sc_time& delay);

    // void executeTransaction(tlm::tlm_generic_payload& trans);
};

// } // namespace gem5

#endif // __ACCELERATORS_SC_NVDLA_SE_HH__
