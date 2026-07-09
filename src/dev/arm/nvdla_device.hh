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

#ifndef __NVDLA_DEVICE_HH__
#define __NVDLA_DEVICE_HH__

#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "cpu/base.hh"
#include "cpu/translation.hh"
#include "debug/rtlNVDLA.hh"
#include "debug/rtlNVDLADebug.hh"
#include "dev/arm/base_gic.hh"
#include "dev/dma_device.hh"
#include "dev/dma_nvdla.hh"
#include "dev/io_device.hh"
#include "params/NvDlaDevice.hh"
#include "params/rtlNVDLA.hh"
#include "rtl/rtlObject.hh"
#include "rtl/traceLoaderGem5.hh"
#include "sim/clocked_object.hh"
#include "sim/system.hh"
#include "wrapper_nvdla.hh"

namespace gem5
{

class TraceLoaderGem5;
class ArmInterruptPin;

/**
 * NvDlaDevice class
 */
class NvDlaDevice : public BasicPioDevice
{
  private:
    time_t sim_time;

    /**
     * Port on the CPU-side that receives requests.
     * Mostly just forwards requests to the owner.
     * Part of a vector of ports. One for each CPU port (e.g., data, inst)
     */
    class CPUSidePort : public ResponsePort
    {
      private:
        /// The object that owns this object (NvDlaDevice)
        NvDlaDevice *owner;

        /// True if the port needs to send a retry req.
        bool needRetry;

        /// If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        CPUSidePort(const std::string& name, NvDlaDevice *owner) :
            ResponsePort(name, owner), owner(owner), needRetry(false),
            blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

        /**
         * Get a list of the non-overlapping address ranges the owner is
         * responsible for. All response ports must override this function
         * and return a populated list with at least one item.
         *
         * @return a list of ranges responded to
         */
        AddrRangeList getAddrRanges() const override;

        /**
         * Send a retry to the peer port only if it is needed. This is called
         * from the NvDlaDevice whenever it is unblocked.
         */
        void trySendRetry();

      protected:
        /**
         * Receive an atomic request packet from the request port.
         * No need to implement in this simple memobj.
         */
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        /**
         * Receive a functional request packet from the request port.
         * Performs a "debug" access updating/reading the data in place.
         *
         * @param packet the requestor sent.
         */
        void recvFunctional(PacketPtr pkt) override;

        /**
         * Receive a timing request from the request port.
         *
         * @param the packet that the requestor sent
         * @return whether this object can consume the packet. If false, we
         *         will call sendRetry() when we can try to receive this
         *         request again.
         */
        bool recvTimingReq(PacketPtr pkt) override;

        /**
         * Called by the request port if sendTimingResp was called on this
         * response port (causing recvTimingResp to be called on the request
         * port) and was unsuccesful.
         */
        void recvRespRetry() override;
    };

    /**
     * Port on the CPU-side that receives requests.
     * Mostly just forwards requests to the owner.
     * Part of a vector of ports. One for each CPU port (e.g., data, inst)
     */
    class CmdCPUSidePort : public ResponsePort
    {
      private:
        /// The object that owns this object (NvDlaDevice)
        NvDlaDevice *owner;

        /// True if the port needs to send a retry req.
        bool needRetry;

        /// If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        CmdCPUSidePort(const std::string& name, NvDlaDevice *owner) :
            ResponsePort(name, owner), owner(owner), needRetry(false),
            blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

        /**
         * Get a list of the non-overlapping address ranges the owner is
         * responsible for. All response ports must override this function
         * and return a populated list with at least one item.
         *
         * @return a list of ranges responded to
         */
        AddrRangeList getAddrRanges() const override;

        /**
         * Send a retry to the peer port only if it is needed. This is called
         * from the NvDlaDevice whenever it is unblocked.
         */
        void trySendRetry();

      protected:
        /**
         * Receive an atomic request packet from the request port.
         * No need to implement in this simple memobj.
         */
        Tick recvAtomic(PacketPtr pkt) override
        { panic("recvAtomic unimpl."); }

        /**
         * Receive a functional request packet from the request port.
         * Performs a "debug" access updating/reading the data in place.
         *
         * @param packet the requestor sent.
         */
        void recvFunctional(PacketPtr pkt) override;

        /**
         * Receive a timing request from the request port.
         *
         * @param the packet that the requestor sent
         * @return whether this object can consume the packet. If false, we
         *         will call sendRetry() when we can try to receive this
         *         request again.
         */
        bool recvTimingReq(PacketPtr pkt) override;

        /**
         * Called by the request port if sendTimingResp was called on this
         * response port (causing recvTimingResp to be called on the request
         * port) and was unsuccesful.
         */
        void recvRespRetry() override;
    };

    /**
     * Port on the memory-side that receives responses.
     * Mostly just forwards requests to the owner
     */
    class MemSidePort : public RequestPort
    {
      private:
        // The object that owns this object (NvDlaDevice)
        NvDlaDevice *owner;

      public:

        // If we tried to send a packet and it was blocked, store it here
        PacketPtr blockedPacket;

        /**
         * Constructor. Just calls the superclass constructor.
         */
        MemSidePort(const std::string& name, NvDlaDevice *owner) :
            RequestPort(name, owner), owner(owner), blockedPacket(nullptr)
        { }

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt);

        bool isBlocked() {
            return blockedPacket != nullptr;
        }

      protected:
        /**
         * Receive a timing response from the response port.
         */
        bool recvTimingResp(PacketPtr pkt) override;

        /**
         * Called by the response port if sendTimingReq was called on this
         * request port (causing recvTimingReq to be called on the responder
         * port) and was unsuccesful.
         */
        void recvReqRetry() override;

        /**
         * Called to receive an address range change from the peer responder
         * port. The default implementation ignores the change and does
         * nothing. Override this function in a derived class if the owner
         * needs to be aware of the address ranges, e.g. in an
         * interconnect component like a bus.
         */
        void recvRangeChange() override;
    };


    /**
     * Port on the memory-side that receives responses.
     * Mostly just forwards requests to the owner
     */
    class MemNVDLAPort : public RequestPort
    {
      private:
        /// The object that owns this object (rtlNVDLA)
        NvDlaDevice *owner;

        /// If we tried to send a packet and it was blocked, store it here
        //PacketPtr blockedPacket;

      public:
        /**
         * Constructor. Just calls the superclass constructor.
         */
        MemNVDLAPort(const std::string& name, NvDlaDevice *owner, bool sram_) :
            RequestPort(name, owner),
            owner(owner),
            sram(sram_),
            blockedRetry(false)
        { }

        uint8_t recentData;

        uint32_t recentData32;

        const uint8_t *recentDataptr;

        std::queue<PacketPtr> pending_req;

        bool sram;
        // if we are blocked due to a req retry
        bool blockedRetry;

        /**
         * Send a packet across this port. This is called by the owner and
         * all of the flow control is hanled in this function.
         *
         * @param packet to send.
         */
        void sendPacket(PacketPtr pkt, bool timing);

        /**
         * We check if we have any pending request and we try to send it
         *
         */
        void tick();

      protected:
        /**
         * Receive a timing response from the slave port.
         */
        bool recvTimingResp(PacketPtr pkt) override;

        /**
         * Called by the slave port if sendTimingReq was called on this
         * master port (causing recvTimingReq to be called on the slave
         * port) and was unsuccesful.
         */
        void recvReqRetry() override;

        /**
         * Called to receive an address range change from the peer slave
         * port. The default implementation ignores the change and does
         * nothing. Override this function in a derived class if the owner
         * needs to be aware of the address ranges, e.g. in an
         * interconnect component like a bus.
         */
        void recvRangeChange() override;
    };

    /**
     * Handle the request from the CPU side
     *
     * @param requesting packet
     * @return true if we can handle the request this cycle, false if the
     *         requestor needs to retry later
     */
    bool handleRequest(PacketPtr pkt);


    /*
     * @param responding packet
     * @return true if we can handle the response this cycle, false if the
     *         responder needs to retry later
     */
    bool handleResponse(PacketPtr pkt);

    /**
     * Handle the response from the memory side for NVDLA
     *
     * @param responding packet
     * @return true if we can handle the response this cycle, false if the
     *         responder needs to retry later
     */
    bool handleResponseNVDLA(PacketPtr pkt, bool sram);

    /**
     * Handle a packet functionally. Update the data on a write and get the
     * data on a read.
     *
     * @param packet to functionally handle
     */
    void handleFunctional(PacketPtr pkt);

    /**
     * Return the address ranges this memobj is responsible for. Just use the
     * same as the next upper level of the hierarchy.
     *
     * @return the address ranges this memobj is responsible for
     */
    AddrRangeList getAddrRanges() const override;

    // function that is called at every cycle
    void tick();

    /**
     * Tell the CPU side to ask for our memory ranges.
     */
    void sendRangeChange();

    /// Instantiation of the CPU-side ports
    CPUSidePort cpuPort;

    CmdCPUSidePort cmdCpuPort;

    /// Instantiation of the memory-side port
    MemSidePort memPort;

    MemNVDLAPort sramPort;

    MemNVDLAPort dramPort;

    DmaPort dmaPort;

    // System pointer
    System * system;
    BaseCPU * cpu;


    // Enable RTL Object
    bool enableObject;

    // Enable RTL Object Trace
    bool enableWaveform;

    Addr to_retry_vaddr;

    /** The tick event used for scheduling CPU ticks. */
    EventFunctionWrapper tickEvent;
    EventFunctionWrapper retryTranslateEvent;

    uint64_t cyclesStat;

    int bytesToRead;    // it works as a counter
    unsigned int bytesReaded;

    // True if this is currently blocked waiting for a response.
    bool blocked;

    const unsigned int max_req_inflight;

    const uint32_t freq_ratio;

    uint32_t id_nvdla;

    uint64_t baseAddrDRAM;
    uint64_t baseAddrSRAM;

    uint32_t startBaseTrace;
    char *ptrTrace;

    struct nvdla_stats
    {
        statistics::Scalar nvdla_cycles;
        statistics::Scalar nvdla_reads;
        statistics::Scalar nvdla_writes;

        statistics::Scalar nvdla_csb_reads;
        statistics::Scalar nvdla_csb_writes;


        statistics::Scalar nvdla_idle_cycles;


        statistics::Histogram nvdla_avgReqCVSRAM;
        statistics::Histogram nvdla_avgReqDBBIF;

        statistics::Scalar num_dma_rd;
        statistics::Scalar num_dma_wr;

        statistics::Scalar num_spm_hit;
        statistics::Scalar num_spm_miss;
        statistics::Scalar num_spm_use;
        statistics::Scalar nvdla_rtl_cycles = 0;
        //statistics::Scalar nvdla_rtl_cycles_idle = 0;

        statistics::Scalar nvdla_total_Bdma0 =0;
        statistics::Scalar nvdla_total_Bdma1 =0;
        statistics::Scalar nvdla_total_Cdp0 =0;
        statistics::Scalar nvdla_total_Cdp1 =0;
        statistics::Scalar nvdla_total_Cmac0 =0;
        statistics::Scalar nvdla_total_Cmac1 =0;
        statistics::Scalar nvdla_total_Pdp0 =0;
        statistics::Scalar nvdla_total_Pdp1 =0;
        statistics::Scalar nvdla_total_Rubik0 =0;
        statistics::Scalar nvdla_total_Rubik1 =0;
        statistics::Scalar nvdla_total_Sdp0 =0;
        statistics::Scalar nvdla_total_Sdp1 =0;
        statistics::Scalar nvdla_total_Cacc0 =0;
        statistics::Scalar nvdla_total_Cacc1 =0;
        statistics::Scalar nvdla_total_CdmaDat0 =0;
        statistics::Scalar nvdla_total_CdmaDat1 =0;
        statistics::Scalar nvdla_total_CdmaWt0 =0;
        statistics::Scalar nvdla_total_CdmaWt1 =0;

    };
    nvdla_stats stats;
    void processOutput(outputNVDLA& out);

public:
    /**
     * This read always returns -1.
     * @param pkt The memory request.
     * @param data Where to put the data.
     */
    virtual Tick read(PacketPtr pkt);

    /**
     * All writes are simply ignored.
     * @param pkt The memory request.
     * @param data the data to not write.
     */
    virtual Tick write(PacketPtr pkt);

    // NVDLA pointers
    Wrapper_nvdla *wr;
    TraceLoaderGem5 *trace;

    // rtl packet
    inputNVDLA input;

    ~NvDlaDevice();
    void runIterationNVDLA();
    void initNVDLA(bool use_shared_spm);
    void loadTraceNVDLA(char *ptr);

    // variables for the NVDLA
    int quiesc_timer;
    int waiting;
    int waiting_for_gem5_mem;   // an indicator only used for AXI_DUMPMEM
    // we need to flush output data in spm to main memory after csb->done()
    int flushing_spm;
    uint32_t cyclesNVDLA;

    /**
      * The constructor for NVDLA device just registers itself with the MMU.
      * @param p params structure
      */
    NvDlaDevice(const NvDlaDeviceParams &params);

    gem5::Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;

    void finishTranslation(WholeTranslationState *state);

    const uint8_t * readAXIVariable(uint64_t addr, bool sram, bool timing,
      bool cacheable, unsigned int size);
    void writeAXI(uint64_t addr, uint8_t data, bool sram, bool timing);
    void writeAXILong(uint64_t addr, uint32_t length, uint8_t* data,
      uint64_t mask, bool sram, bool timing, bool cacheable);

    uint64_t getRealAddr(uint64_t addr, bool sram);
    uint64_t getAddrNVDLA(uint64_t addr, bool sram);
    /**
     * Register the stats
     */
    void regStats();

    int prefetch_enable;
    uint32_t pft_threshold;

    uint32_t spm_latency;
    uint32_t spm_line_size;
    uint32_t spm_line_num;
    uint32_t assoc;

    int dma_enable;
    DmaReadFifo* dma_rd_engine;
    DmaNvdla* dma_wr_engine;

    // control the mode of using embedded SPM / cache,
    // whether as an all-in-one buffer or simply a prefetch buffer
    BufferMode buffer_mode;
    bool use_fake_mem;

    std::string print_path;

    void try_get_dma_read_data(uint32_t size);

    /*
    * Functions for TLB connection
    * finishTranslation needs to be override on derived class
    */
    bool isSquashed() const { return false; }
    void startTranslate(Addr vaddr, ContextID contextId);
    void retryTranslate();

    Tick nvdla_start_Bdma0;
    Tick nvdla_end_Bdma0;
    Tick nvdla_start_Bdma1;
    Tick nvdla_end_Bdma1;
    Tick nvdla_start_Cdp0;
    Tick nvdla_end_Cdp0;
    Tick nvdla_start_Cdp1;
    Tick nvdla_end_Cdp1;
    Tick nvdla_start_Cmac0;
    Tick nvdla_end_Cmac0;
    Tick nvdla_start_Cmac1;
    Tick nvdla_end_Cmac1;
    Tick nvdla_start_Pdp0;
    Tick nvdla_end_Pdp0;
    Tick nvdla_start_Pdp1;
    Tick nvdla_end_Pdp1;
    Tick nvdla_start_Rubik0;
    Tick nvdla_end_Rubik0;
    Tick nvdla_start_Rubik1;
    Tick nvdla_end_Rubik1;
    Tick nvdla_start_Sdp0;
    Tick nvdla_end_Sdp0;
    Tick nvdla_start_Sdp1;
    Tick nvdla_end_Sdp1;
    Tick nvdla_start_Cacc0;
    Tick nvdla_end_Cacc0;
    Tick nvdla_start_Cacc1;
    Tick nvdla_end_Cacc1;
    Tick nvdla_start_CdmaDat0;
    Tick nvdla_end_CdmaDat0;
    Tick nvdla_start_CdmaDat1;
    Tick nvdla_end_CdmaDat1;
    Tick nvdla_start_CdmaWt0;
    Tick nvdla_end_CdmaWt0;
    Tick nvdla_start_CdmaWt1;
    Tick nvdla_end_CdmaWt1;
    nvdla_stats& getStats() { return stats; }

    const uint32_t getFreqRatio() const { return freq_ratio; }

    uint64_t getTickfromWrapperNVDLA() const { return (wr->tickcount); }

  protected:
    ArmInterruptPin *const interrupt;
    bool interruptRaised;
    bool onRead;
    uint32_t readData;

    bool traceMode;
    bool engineStarted;
};

} //End namespace gem5

#endif // __NVDLA_DEVICE_HH__
