#ifndef __ACCELERATORS_SC_NVDLA_STATS_HH__
#define __ACCELERATORS_SC_NVDLA_STATS_HH__

//#include "base/statistics.hh"
//#include "sim/eventq.hh"
//#include "sim/sim_object.hh"
//#include "systemc/ext/channel/sc_buffer.hh"
//#include "systemc/tlm_port_wrapper.hh"
//#include "systemc/ext/core/sc_module.hh"
//#include "systemc/tlm_bridge/tlm_to_gem5.hh"
//#include "systemc/tlm_bridge/gem5_to_tlm.hh"
//#include "mem/port.hh"
//#include "mem/port.hh"
//#include "mem/packet.hh"
//#include "mem/packet_access.hh"
//#include "sim/eventq.hh"
//#include <tlm>
//#include <tlm_utils/simple_target_socket.h>
//#include <tlm_utils/multi_passthrough_target_socket.h>
//#include <tlm_utils/multi_passthrough_initiator_socket.h>
//#include <tlm_utils/simple_initiator_socket.h>
//#include <tlm_utils/simple_initiator_socket.h>
//#include <tlm_utils/simple_target_socket.h>
//#include <iomanip>
//#include <iostream>
//#include <map>
//#include <queue>
//#include <vector>
//#include "base/trace.hh"
//#include "systemc.h"
//#include "systemc/ext/tlm"
//#include <fstream>
//#include "sim/sim_exit.hh"
//#include "base/output.hh"
//#include <fstream>

#include <cstdint>

#include "systemc.h"

using namespace std;
using namespace sc_core;
//using namespace gem5;

struct NvdlaStats
{
    uint64_t csbReads = 0;
    uint64_t csbWrites = 0;

    uint64_t dbbReads = 0;
    uint64_t dbbWrites = 0;

    uint64_t dbbReadBytes = 0;
    uint64_t dbbWriteBytes = 0;

    uint64_t sramReads = 0;
    uint64_t sramWrites = 0;

    uint64_t sramReadBytes = 0;
    uint64_t sramWriteBytes = 0;

    uint64_t interruptsRaised = 0;

    uint64_t bdmaReadReqs = 0;
    uint64_t bdmaReadBytes = 0;

    uint64_t bdmaWriteReqs = 0;
    uint64_t bdmaWriteBytes = 0;

    sc_time bdmaStartGrp0;
    sc_time bdmaStartGrp1;
    uint64_t bdmaGrp0Cycles = 0;
    uint64_t bdmaGrp1Cycles = 0;

    sc_time caccStartGrp0;
    sc_time caccStartGrp1;
    uint64_t caccGrp0Cycles = 0;
    uint64_t caccGrp1Cycles = 0;

    sc_time cdmaStartGrp0;
    sc_time cdmaStartGrp1;
    uint64_t cdmaGrp0Cycles = 0;
    uint64_t cdmaGrp1Cycles = 0;
    uint64_t cdmaDataReadReqs = 0;
    uint64_t cdmaDataReadBytes = 0;
    uint64_t cdmaWeightReadReqs = 0;
    uint64_t cdmaWeightReadBytes = 0;

    sc_time cdpStartGrp0;
    sc_time cdpStartGrp1;
    uint64_t cdpGrp0Cycles = 0;
    uint64_t cdpGrp1Cycles = 0;

    sc_time cmacStartGrp0;
    sc_time cmacStartGrp1;
    uint64_t cmacGrp0Cycles = 0;
    uint64_t cmacGrp1Cycles = 0;

    sc_time cscStartGrp0;
    sc_time cscStartGrp1;
    uint64_t cscGrp0Cycles = 0;
    uint64_t cscGrp1Cycles = 0;

    sc_time pdpStartGrp0;
    sc_time pdpStartGrp1;
    uint64_t pdpGrp0Cycles = 0;
    uint64_t pdpGrp1Cycles = 0;

    sc_time rubikStartGrp0;
    sc_time rubikStartGrp1;
    uint64_t rubikGrp0Cycles = 0;
    uint64_t rubikGrp1Cycles = 0;

    sc_time sdpStartGrp0;
    sc_time sdpStartGrp1;
    uint64_t sdpGrp0Cycles = 0;
    uint64_t sdpGrp1Cycles = 0;

    uint64_t cbufDataWrites = 0;
    uint64_t cbufDataWriteBytes = 0;
    uint64_t cbufDataReads = 0;
    uint64_t cbufDataReadBytes = 0;


    uint64_t cbufWeightWrites = 0;
    uint64_t cbufWeightWriteBytes = 0;
    uint64_t cbufWeightReads = 0;
    uint64_t cbufWeightReadBytes = 0;

    sc_time nvdlaClockPeriod;
};

extern NvdlaStats gNvdlaStats;

#endif // __ACCELERATORS_SC_NVDLA_STATS_HH__
