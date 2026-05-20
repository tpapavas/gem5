#ifndef __ACCELERATORS_SC_NVDLA_SE_HH__
#define __ACCELERATORS_SC_NVDLA_SE_HH__

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

//#include "systemc/ext/systemc"
#include "systemc.h"

#include "systemc/ext/tlm"
#include "systemC_NVDLA/sc_tlm_initiator.hh"
#include "systemC_NVDLA/sc_tlm_target.hh"

using namespace std;
using namespace sc_core;
using namespace gem5;



// SC_MODULE(ScNvDlaSE)
class ScNvDlaSE : public sc_core::sc_module
{
public:
    Initiator initiator;
    //Target dbb_target;
    //Target sram_target;

    scsim::cmod::NV_nvdla *nvdla;

    // Interrupt signal from NVDLA
    sc_core::sc_signal<bool,SC_MANY_WRITERS> irq;
    bool interruptRaised = false;
    //sc_signal_resolved

    bool dbb_busy = false;
    sc_core::sc_event dbb_done;
    sc_core::sc_event dbb_event;

    std::queue<tlm::tlm_generic_payload*> dbb_queue;


    tlm_utils::simple_target_socket<ScNvDlaSE> csb_target;
    sc_gem5::TlmTargetWrapper<32> csb_wrapper;

    tlm_utils::simple_initiator_socket<ScNvDlaSE> dbb_init;
    tlm_utils::simple_initiator_socket<ScNvDlaSE> sram_init;

    tlm_utils::simple_target_socket<ScNvDlaSE> dbb_target;
    tlm_utils::simple_target_socket<ScNvDlaSE> sram_target;


    sc_gem5::TlmInitiatorWrapper<32> dbb_init_wrapper;
    sc_gem5::TlmInitiatorWrapper<32> sram_init_wrapper;

    sc_gem5::TlmTargetWrapper<32> dbb_target_wrapper;
    sc_gem5::TlmTargetWrapper<32> sram_target_wrapper;


public:
    SC_HAS_PROCESS(ScNvDlaSE);
    ScNvDlaSE(sc_module_name name) :
        sc_module(name),
        initiator("dla_tlm_initiator"),
        csb_target("csb_target"),
        csb_wrapper(csb_target,
            std::string(name) + ".csb_target", InvalidPortID),
        //dbb_target("dbb_target"),
        //sram_target("sram_target"),
        dbb_init("dbb_init"),
        dbb_init_wrapper(dbb_init,
            std::string(name) + ".dbb_init", InvalidPortID),
        sram_init("sram_init"),
        sram_init_wrapper(sram_init,
            std::string(name) + ".sram_init", InvalidPortID),
        dbb_target("dbb_target"),
        dbb_target_wrapper(dbb_target,
            std::string(name) + ".dbb_target", InvalidPortID),
        sram_target("sram_target"),
        sram_target_wrapper(sram_target,
            std::string(name) + ".sram_target", InvalidPortID)
    {
        //////////////////////////////////
        // sc_core::sc_report_handler::set_verbosity_level(sc_core::SC_DEBUG);

        csb_target.register_b_transport(this, &ScNvDlaSE::b_transport_csb);
        // dbb_target.register_b_transport(this, &ScNvDlaSE::b_transport_dbb);

        dbb_target.register_nb_transport_fw(this,
            &ScNvDlaSE::nb_transport_fw);
        dbb_init.register_nb_transport_bw(this, &ScNvDlaSE::nb_transport_bw);

        sram_target.register_b_transport(this, &ScNvDlaSE::b_transport_sram);

        nvdla = new scsim::cmod::NV_nvdla("nvdla_core");
        initiator.tSocket.bind(nvdla->nvdla_host_master_if);
        nvdla->nvdla_core2dbb_axi4.bind(dbb_target);
        nvdla->nvdla_core2cvsram_axi4.bind(sram_target);
        nvdla->nvdla_intr(irq);

        SC_METHOD(handle_irq);
        sensitive << irq;

        // SC_THREAD(dbb_worker);
        dont_initialize();
        ////////////////////////////////////////////


        printf("[ScNvDlaSE] All bindings done\n");
    }

    void handle_irq();

    gem5::Port &gem5_getPort(const std::string &if_name, int idx=-1) override;

    virtual void b_transport_csb(tlm::tlm_generic_payload& trans,
                             sc_time& delay);
    virtual void b_transport_dbb(tlm::tlm_generic_payload& trans,
                             sc_time& delay);
    virtual void b_transport_sram(tlm::tlm_generic_payload& trans,
                             sc_time& delay);

    void dbb_worker();

   tlm::tlm_sync_enum nb_transport_fw(
       tlm::tlm_generic_payload& trans,
       tlm::tlm_phase& phase,
       sc_time& delay
   );

   tlm::tlm_sync_enum nb_transport_bw(
       tlm::tlm_generic_payload& trans,
       tlm::tlm_phase& phase,
       sc_time& delay
   );
};

// } // namespace gem5

#endif // __ACCELERATORS_SC_NVDLA_SE_HH__
