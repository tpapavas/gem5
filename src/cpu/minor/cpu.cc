/*
 * Copyright (c) 2012-2014, 2017 ARM Limited
 * All rights reserved
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
 */

#include "cpu/minor/cpu.hh"

#include "../../dev/arm/nvdla_device.hh"
#include "../../dev/arm/nvdla_device_se.hh"
#include "cpu/minor/dyn_inst.hh"
#include "cpu/minor/fetch1.hh"
#include "cpu/minor/lsq.hh"
#include "cpu/minor/pipeline.hh"
#include "debug/Drain.hh"
#include "debug/MinorCPU.hh"
#include "debug/NvDlaDevice.hh"
#include "debug/Quiesce.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"

namespace gem5
{

MinorCPU::MinorCPU(const BaseMinorCPUParams &params) :
    BaseCPU(params),
    threadPolicy(params.threadPolicy),
    stats(this)
{
    /* This is only written for one thread at the moment */
    minor::MinorThread *thread;

    for (ThreadID i = 0; i < numThreads; i++) {
        if (FullSystem) {
            thread = new minor::MinorThread(this, i, params.system,
                    params.mmu, params.isa[i], params.decoder[i]);
            thread->setStatus(ThreadContext::Halted);
        } else {
            thread = new minor::MinorThread(this, i, params.system,
                    params.workload[i], params.mmu,
                    params.isa[i], params.decoder[i]);
        }

        threads.push_back(thread);
        ThreadContext *tc = thread->getTC();
        threadContexts.push_back(tc);
    }


    if (params.checker) {
        fatal("The Minor model doesn't support checking (yet)\n");
    }

    pipeline = new minor::Pipeline(*this, params);
    activityRecorder = pipeline->getActivityRecorder();

    fetchEventWrapper = NULL;
}

MinorCPU::~MinorCPU()
{
    delete pipeline;

    if (fetchEventWrapper != NULL)
        delete fetchEventWrapper;

    for (ThreadID thread_id = 0; thread_id < threads.size(); thread_id++) {
        delete threads[thread_id];
    }
}

void
MinorCPU::init()
{
    BaseCPU::init();

    if (!params().switched_out && system->getMemoryMode() != enums::timing) {
        fatal("The Minor CPU requires the memory system to be in "
            "'timing' mode.\n");
    }
}

/** Stats interface from SimObject (by way of BaseCPU) */
void
MinorCPU::regStats()
{
    BaseCPU::regStats();
    pipeline->regStats();
}

void
MinorCPU::serializeThread(CheckpointOut &cp, ThreadID thread_id) const
{
    threads[thread_id]->serialize(cp);
}

void
MinorCPU::unserializeThread(CheckpointIn &cp, ThreadID thread_id)
{
    threads[thread_id]->unserialize(cp);
}

void
MinorCPU::serialize(CheckpointOut &cp) const
{
    pipeline->serialize(cp);
    BaseCPU::serialize(cp);
}

void
MinorCPU::unserialize(CheckpointIn &cp)
{
    pipeline->unserialize(cp);
    BaseCPU::unserialize(cp);
}

void
MinorCPU::wakeup(ThreadID tid)
{
    DPRINTF(Drain, "[tid:%d] MinorCPU wakeup\n", tid);
    assert(tid < numThreads);

    if (threads[tid]->status() == ThreadContext::Suspended) {
        threads[tid]->activate();
    }
}

void
MinorCPU::startup()
{
    DPRINTF(MinorCPU, "MinorCPU startup\n");

    BaseCPU::startup();

    for (ThreadID tid = 0; tid < numThreads; tid++)
        pipeline->wakeupFetch(tid);
}

DrainState
MinorCPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    if (switchedOut()) {
        DPRINTF(Drain, "Minor CPU switched out, draining not needed.\n");
        return DrainState::Drained;
    }

    DPRINTF(Drain, "MinorCPU drain\n");

    /* Need to suspend all threads and wait for Execute to idle.
     * Tell Fetch1 not to fetch */
    if (pipeline->drain()) {
        DPRINTF(Drain, "MinorCPU drained\n");
        return DrainState::Drained;
    } else {
        DPRINTF(Drain, "MinorCPU not finished draining\n");
        return DrainState::Draining;
    }
}

void
MinorCPU::signalDrainDone()
{
    DPRINTF(Drain, "MinorCPU drain done\n");
    Drainable::signalDrainDone();
}

void
MinorCPU::drainResume()
{
    /* When taking over from another cpu make sure lastStopped
     * is reset since it might have not been defined previously
     * and might lead to a stats corruption */
    pipeline->resetLastStopped();

    if (switchedOut()) {
        DPRINTF(Drain, "drainResume while switched out.  Ignoring\n");
        return;
    }

    DPRINTF(Drain, "MinorCPU drainResume\n");

    if (!system->isTimingMode()) {
        fatal("The Minor CPU requires the memory system to be in "
            "'timing' mode.\n");
    }

    for (ThreadID tid = 0; tid < numThreads; tid++){
        wakeup(tid);
    }

    pipeline->drainResume();

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}

void
MinorCPU::memWriteback()
{
    DPRINTF(Drain, "MinorCPU memWriteback\n");
}

void
MinorCPU::switchOut()
{
    DPRINTF(MinorCPU, "MinorCPU switchOut\n");

    assert(!switchedOut());
    BaseCPU::switchOut();

    /* Check that the CPU is drained? */
    activityRecorder->reset();
}

void
MinorCPU::takeOverFrom(BaseCPU *old_cpu)
{
    DPRINTF(MinorCPU, "MinorCPU takeOverFrom\n");

    BaseCPU::takeOverFrom(old_cpu);
}

void
MinorCPU::activateContext(ThreadID thread_id)
{
    DPRINTF(MinorCPU, "ActivateContext thread: %d\n", thread_id);

    /* Do some cycle accounting.  lastStopped is reset to stop the
     *  wakeup call on the pipeline from adding the quiesce period
     *  to BaseCPU::numCycles */
    stats.quiesceCycles += pipeline->cyclesSinceLastStopped();
    pipeline->resetLastStopped();

    /* Wake up the thread, wakeup the pipeline tick */
    threads[thread_id]->activate();
    wakeupOnEvent(minor::Pipeline::CPUStageId);

    if (!threads[thread_id]->getUseForClone())//the thread is not cloned
    {
        pipeline->wakeupFetch(thread_id);
    } else { //the thread from clone
        if (fetchEventWrapper != NULL)
            delete fetchEventWrapper;
        fetchEventWrapper = new EventFunctionWrapper([this, thread_id]
                  { pipeline->wakeupFetch(thread_id); }, "wakeupFetch");
        schedule(*fetchEventWrapper, clockEdge(Cycles(0)));
    }

    BaseCPU::activateContext(thread_id);
}

void
MinorCPU::suspendContext(ThreadID thread_id)
{
    DPRINTF(MinorCPU, "SuspendContext %d\n", thread_id);

    threads[thread_id]->suspend();

    BaseCPU::suspendContext(thread_id);
}

void
MinorCPU::wakeupOnEvent(unsigned int stage_id)
{
    DPRINTF(Quiesce, "Event wakeup from stage %d\n", stage_id);

    /* Mark that some activity has taken place and start the pipeline */
    activityRecorder->activateStage(stage_id);
    pipeline->start();
}

Port &
MinorCPU::getInstPort()
{
    return pipeline->getInstPort();
}

Port &
MinorCPU::getDataPort()
{
    return pipeline->getDataPort();
}

Counter
MinorCPU::totalInsts() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numInst;

    return ret;
}

Counter
MinorCPU::totalOps() const
{
    Counter ret = 0;

    for (auto i = threads.begin(); i != threads.end(); i ++)
        ret += (*i)->numOp;

    return ret;
}

void
MinorCPU::startAccel(Addr vaddr, int elements, Addr region_nvdla)
{
    if (num_accels>3) {
        RequestPtr req = std::make_shared<Request>(vaddr, elements,
                                  0, Request::funcRequestorId,0,0);
        PacketPtr pkt = new Packet(req, MemCmd::ReadReq, elements);
        nvdla_port_3.sendTimingReq(pkt);

        finishedAccelerator3 = false;
    }
    if (num_accels>2) {
        RequestPtr req = std::make_shared<Request>(vaddr, elements,
                                  0, Request::funcRequestorId,0,0);
        PacketPtr pkt = new Packet(req, MemCmd::ReadReq, elements);
        nvdla_port_2.sendTimingReq(pkt);

        finishedAccelerator2 = false;
    }
    if (num_accels>1) {
        RequestPtr req = std::make_shared<Request>(vaddr, elements,
                                  0, Request::funcRequestorId,0,0);
        PacketPtr pkt = new Packet(req, MemCmd::ReadReq, elements);
        nvdla_port_1.sendTimingReq(pkt);

        finishedAccelerator1 = false;
    }
    if (num_accels>0) {
        RequestPtr req = std::make_shared<Request>(vaddr, elements,
                                  0, Request::funcRequestorId,0,0);
        PacketPtr pkt = new Packet(req, MemCmd::ReadReq, elements);
        nvdla_port_0.sendTimingReq(pkt);

        finishedAccelerator0 = false;
    }

}

void
MinorCPU::startAccelID(Addr vaddr, int elements, Addr region_nvdla,
    int accel_id)
{
    RequestPtr req = std::make_shared<Request>(vaddr, elements,
                              0, Request::funcRequestorId,0,0);
    PacketPtr pkt = new Packet(req, MemCmd::ReadReq, elements);
    switch (accel_id) {
        case 0:
            nvdla_port_0.sendTimingReq(pkt);
            finishedAccelerator0 = false;
            break;
        case 1:
            nvdla_port_1.sendTimingReq(pkt);
            finishedAccelerator1 = false;
            break;
        case 2:
            nvdla_port_2.sendTimingReq(pkt);
            finishedAccelerator2 = false;
            break;
        case 3:
            nvdla_port_3.sendTimingReq(pkt);
            finishedAccelerator3 = false;
            break;
        default:
            break;
    }
}

uint64_t
MinorCPU::waitAccel(Addr vaddr, int elements)
{

    // DPRINTF(Accelerator, "Wait for Accelerator \n");
    // std::cout << "Wait Accelerator " << std::endl;
    if (num_accels == 1) {
        return !finishedAccelerator0;
    } else if (num_accels == 2) {
        return !finishedAccelerator0 |
               !finishedAccelerator1;
    } else if (num_accels == 3) {
        return !finishedAccelerator0 |
               !finishedAccelerator1 |
               !finishedAccelerator2;
    } else {
        return !finishedAccelerator0 |
               !finishedAccelerator1 |
               !finishedAccelerator2 |
               !finishedAccelerator3;
    }
}


uint64_t
MinorCPU::waitAccelID(int accel_id)
{
    switch (accel_id) {
        case 0:
            return !finishedAccelerator0;
        case 1:
            return !finishedAccelerator1;
        case 2:
            return !finishedAccelerator2;
        case 3:
            return !finishedAccelerator3;
        default:
            fatal("waitAccelID: Unknown accel id.\n");
    }
}

uint32_t
MinorCPU::NvDlaReadReg(int accel_id, Addr addr)
{
    DPRINTF(NvDlaDevice, "[GEM5 LOG] DLA #%d: Trying to read_reg(0x%016x)\n",
        accel_id, addr);
    // we send a null packet telling we have finished
    RequestPtr req = std::make_shared<Request>(addr, 4,
                                            Request::UNCACHEABLE, 0);
    PacketPtr pkt = nullptr;
    // we create the real packet, write request
    pkt = Packet::createRead(req);

    std::cout << "[NvDlaReadReg] " << pkt->cmdString()
                  << " addr=0x" << std::hex << addr
                  << " size=" << std::dec << pkt->getSize();


    switch(accel_id) {
        case 0:
            nvdla_port_plus_0.sendTimingReq(pkt);
            break;
        case 1:
            nvdla_port_plus_1.sendTimingReq(pkt);
            break;
        default:
            assert(false);
    }

    DPRINTF(NvDlaDevice, "[GEM5 LOG] DLA #%d: Trying to get response...\n",
        accel_id);
    DPRINTF(NvDlaDevice, "[GEM5 LOG] DLA #%d: Got response: %u\n",
        accel_id, pkt->getLE<uint32_t>());

    return pkt->getLE<uint32_t>();
}

void
MinorCPU::NvDlaWriteReg(int accel_id, uint32_t data, Addr addr)
{
    DPRINTF(NvDlaDevice, "[GEM5 LOG] DLA #%d: Trying to write_reg(0x%016x), "
        "data: 0x%08x\n",
        accel_id, addr, data);
    // we send a null packet telling we have finished
    RequestPtr req = std::make_shared<Request>(addr, 4,
                                            Request::UNCACHEABLE, 0);
    PacketPtr pkt = nullptr;
    // we create the real packet, write request
    pkt = Packet::createWrite(req);
    pkt->allocate();
    pkt->setLE<uint32_t>(data);

     std::cout << "[NvDlaWriteReg] " << pkt->cmdString()
                  << " addr=0x" << std::hex << addr
                  << " size=" << std::dec << pkt->getSize();

    switch(accel_id) {
        case 0:
            nvdla_port_plus_0.sendTimingReq(pkt);
            break;
        case 1:
            nvdla_port_plus_1.sendTimingReq(pkt);
            break;
        default:
            assert(false);
    }
}


void MinorCPU::accelStartCountBdma0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto dev = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Bdma0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Bdma0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, dev);
}
void MinorCPU::accelEndCountBdma0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Bdma0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Bdma0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Bdma0 +=
            (nvdla_device->nvdla_end_Bdma0 -
            nvdla_device->nvdla_start_Bdma0);
    }, nvdla_device);
}
void MinorCPU::accelStartCountBdma1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Bdma1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Bdma1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountBdma1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Bdma1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Bdma1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Bdma1 +=
            (nvdla_device->nvdla_end_Bdma1 -
            nvdla_device->nvdla_start_Bdma1) ;
    }, nvdla_device);
}
void MinorCPU::accelStartCountCdp0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Cdp0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Cdp0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountCdp0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Cdp0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Cdp0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Cdp0 +=
            (nvdla_device->nvdla_end_Cdp0 -
            nvdla_device->nvdla_start_Cdp0);
    }, nvdla_device);
}
void MinorCPU::accelStartCountCdp1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Cdp1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Cdp1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountCdp1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Cdp1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Cdp1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Cdp1 +=
            (nvdla_device->nvdla_end_Cdp1 -
            nvdla_device->nvdla_start_Cdp1);
    }, nvdla_device);
}
void MinorCPU::accelStartCountCmac0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Cmac0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Cmac0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountCmac0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Cmac0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Cmac0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Cmac0 +=
            (nvdla_device->nvdla_end_Cmac0 -
            nvdla_device->nvdla_start_Cmac0);
    }, nvdla_device);
}
void MinorCPU::accelStartCountCmac1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Cmac1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Cmac1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountCmac1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Cmac1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Cmac1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Cmac1 +=
            (nvdla_device->nvdla_end_Cmac1 -
            nvdla_device->nvdla_start_Cmac1);
    }, nvdla_device);
}
void MinorCPU::accelStartCountPdp0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Pdp0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Pdp0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountPdp0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Pdp0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Pdp0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Pdp0 +=
            (nvdla_device->nvdla_end_Pdp0 -
            nvdla_device->nvdla_start_Pdp0);
    }, nvdla_device);
}
void MinorCPU::accelStartCountPdp1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Pdp1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Pdp1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountPdp1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Pdp1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Pdp1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Pdp1 +=
            (nvdla_device->nvdla_end_Pdp1 -
            nvdla_device->nvdla_start_Pdp1);
    }, nvdla_device);
}

void MinorCPU::accelStartCountRubik0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Rubik0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Rubik0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountRubik0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Rubik0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Rubik0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Rubik0 +=
            (nvdla_device->nvdla_end_Rubik0 -
            nvdla_device->nvdla_start_Rubik0);
    }, nvdla_device);

}

void MinorCPU::accelStartCountRubik1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Rubik1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Rubik1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountRubik1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Rubik1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Rubik1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Rubik1 +=
            (nvdla_device->nvdla_end_Rubik1 -
            nvdla_device->nvdla_start_Rubik1) ;
    }, nvdla_device);
}

void MinorCPU::accelStartCountSdp0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Sdp0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Sdp0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountSdp0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Sdp0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Sdp0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Sdp0 +=
            (nvdla_device->nvdla_end_Sdp0 -
            nvdla_device->nvdla_start_Sdp0) ;
    }, nvdla_device);
}

void MinorCPU::accelStartCountSdp1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Sdp1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Sdp1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}
void MinorCPU::accelEndCountSdp1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Sdp1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Sdp1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Sdp1 +=
            (nvdla_device->nvdla_end_Sdp1 -
            nvdla_device->nvdla_start_Sdp1) ;
    }, nvdla_device);
}

void MinorCPU::accelStartCountCacc0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Cacc0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Cacc0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);

}

void MinorCPU::accelEndCountCacc0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Cacc0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Cacc0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Cacc0 +=
            (nvdla_device->nvdla_end_Cacc0 -
            nvdla_device->nvdla_start_Cacc0) ;
    }, nvdla_device);
}

void MinorCPU::accelStartCountCacc1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting Cacc1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_Cacc1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountCacc1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting Cacc1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_Cacc1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_Cacc1 +=
            (nvdla_device->nvdla_end_Cacc1 -
            nvdla_device->nvdla_start_Cacc1);
    }, nvdla_device);

}

void MinorCPU::accelStartCountCdmaDat0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting CdmaDat0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_CdmaDat0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountCdmaDat0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting CdmaDat0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_CdmaDat0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_CdmaDat0 +=
            (nvdla_device->nvdla_end_CdmaDat0 -
            nvdla_device->nvdla_start_CdmaDat0);
    }, nvdla_device);
}

void MinorCPU::accelStartCountCdmaDat1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting CdmaDat1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_CdmaDat1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountCdmaDat1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting CdmaDat1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_CdmaDat1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_CdmaDat1 +=
            (nvdla_device->nvdla_end_CdmaDat1 -
            nvdla_device->nvdla_start_CdmaDat1);
    }, nvdla_device);
}

void MinorCPU::accelStartCountCdmaWt0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting CdmaWt0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_CdmaWt0 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountCdmaWt0(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting CdmaWt0 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_CdmaWt0 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_CdmaWt0 +=
            (nvdla_device->nvdla_end_CdmaWt0 -
            nvdla_device->nvdla_start_CdmaWt0);
    }, nvdla_device);
}

void MinorCPU::accelStartCountCdmaWt1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "Start counting CdmaWt1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_start_CdmaWt1 =
            nvdla_device->getTickfromWrapperNVDLA();
    }, nvdla_device);
}

void MinorCPU::accelEndCountCdmaWt1(int accel_id)
{
    assert(accel_id == 0 || accel_id == 1);
    auto nvdla_device = ( (accel_id == 0) ? nvdla_device_0 : nvdla_device_1 );

    std::visit([&](auto* nvdla_device) {
        DPRINTF(NvDlaDevice, "End counting CdmaWt1 for DLA #%d @ %llu\n",
            accel_id, nvdla_device->getTickfromWrapperNVDLA());
        nvdla_device->nvdla_end_CdmaWt1 =
            nvdla_device->getTickfromWrapperNVDLA();
        nvdla_device->getStats().nvdla_total_CdmaWt1 +=
            (nvdla_device->nvdla_end_CdmaWt1 -
            nvdla_device->nvdla_start_CdmaWt1);
    }, nvdla_device);
}

} // namespace gem5
