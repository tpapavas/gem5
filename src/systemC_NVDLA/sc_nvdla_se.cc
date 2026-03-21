#include <iostream>

// #include "params/ScNVDLAModule.hh"
#include "params/TLM_ScNvDlaSE.hh"
#include "sc_nvdla_se.hh"
#include "sim/sim_exit.hh"

using namespace std;
using namespace sc_core;
using namespace gem5;
// namespace gem5
// {

// ScNvDlaSE::ScNvDlaSE(sc_module_name name)
//     : sc_module(name),
//     //   accel_port(name() + ".accel_port", this),
//     //   mod(params.module),
//     //   transactor(params.transactor),
//     //   target(params.tlm_target),
//         initiator(params.tlm_initiator),
//         tSocket("tSocket"),
//         wrapper(tSocket, std::string(name) + ".tlm", InvalidPortID)
//     //   event(this)//,
//     //   csbBridge(params.csb),
//     //   sramBridge(params.sram),
//     //   dbbBridge(params.dbb)
// {
//     // panic_if(!csbBridge || !sramBridge || !dbbBridge,
//     //          "All TLM bridges must be provided");

//     tickPeriod = 500;
//     panic_if(tickPeriod == 0, "Clock period cannot be zero!");

//     std::cout << "ScNvDlaSE SystemC tick @: " << sc_core::sc_time_stamp()
//         << " Gem5 tick @: " << curTick() << "\n";



//     //////////////////////////////////

//     tSocket.register_b_transport(this, &ScNvDlaSE::b_transport);

//     nvdla = new scsim::cmod::NV_nvdla("nvdla_core");
//     initiator->tSocket.bind(nvdla->nvdla_host_master_if);



//     ////////////////////////////////////////////




//     // nvdla->nvdla_host_master_if.register_b_transport(
//          nvdla, &Target::b_transport);
//     //  cpu = new Initiator("initiator");
//     // mod = new ScNVDLAModule("ScNVDLAs");
//     // panic_if(!mod, "No systemC NVDLA");

//     // mod->accel_sc_port = &accel_port;
//     // csbBridge->getSocket().bind(mod->nvdla->nvdla_host_master_if);

//     // mod->nvdla->nvdla_core2dbb_axi4.bind(dbbBridge->getSocket());
//     // mod->nvdla->nvdla_core2cvsram_axi4.bind(sramBridge->getSocket());

//     printf("[ScNvDlaSE] All bindings done\n");
// }

// This "create" method bridges the python configuration and the systemc
// objects. It instantiates the Printer object and sets it up using the
// parameter values from the config, just like it would for a SimObject. The
// systemc object could accept those parameters however it likes, for instance
// through its constructor or by assigning them to a member variable.
ScNvDlaSE *
gem5::TLM_ScNvDlaSEParams::create() const
{
    ScNvDlaSE *sc_nvdla_se = new ScNvDlaSE(name.c_str());
    return sc_nvdla_se;
}

gem5::Port
&ScNvDlaSE::gem5_getPort(const std::string &if_name, int idx)
{
    return wrapper;
}

void ScNvDlaSE::b_transport(tlm::tlm_generic_payload& trans,
                            sc_time& delay)
{
    // cout << "ok";
    // initiator.tSocket.bind(nvdla->nvdla_host_master_if);
    initiator.write_reg(0x5034, 0x0000c000);
    uint32_t val = initiator.read_reg(0x5034);
    printf("[GEM5-PLUS] got value from NVDLA: 0x%08x\n", val);
    return;
}

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

    //     sc_core::sc_time duration =
    //          sc_core::sc_time::from_value(tickPeriod);

    //     // sc_core::sc_start(duration);
    //     //::sc_gem5::scheduler.oneCycle();

    //     // mod->nvdla->print(std::cout); // prints NVDLA hierarchy
    //     // mod->nvdla->dump(std::cout);  // more detailed
    //     // auto &children = mod->nvdla->get_child_objects();
    //     // std::cout << "NVDLA has " << children.size()
    //          << " child objects\n";
    //     // for (auto child : mod->nvdla->get_child_objects()) {
    //     //    std::cout << "  - " << child->name() << "  kind: "
    //          << child->kind() << "\n";
    //     // }
    //     // exitSimLoop("done");
    //     panic_if(sc_time_stamp() > sc_time::from_value(curTick()),
    //          "Somethink wrong with systemC timing");
    //     // sc_core::sc_start(sc_core::sc_time(1, sc_core::SC_NS));
    //     // if (mod->irq.read())
    //     // {
    //     //     exitSimLoop("done");
    //     //     return;
    //     // }

    //     schedule(event, curTick() + tickPeriod);
    // }

// } // namespace gem5
