#ifndef __DEV_UCIE_LINK_HH__
#define __DEV_UCIE_LINK_HH__

/*
 * UCIeLink: pipelined, bidirectional die-to-die link model.
 *
 * Each direction (request: cpu_side -> mem_side, response: mem_side ->
 * cpu_side) has an independent bounded FIFO. A packet of B bits occupies
 * the link for
 *     occ(B) = get_delay(B) - get_delay(fdiWidth) + word_period
 * (serialization), starts when the link is free (linkFreeAt) and arrives
 * at the far side after the full model latency get_delay(B). Data-less
 * packets (read requests, write acks, evictions) cost header_bits only;
 * data-carrying packets cost size*8 + header_bits.
 * Snoops are not forwarded (the link is a non-snooping point-to-point
 * connection).
 *
 * task13: link_mode=flit256_std|flit68 selects a flit-accurate D2D adapter
 * model (MUG5/JETCAS): each packet becomes a TLP of
 * roundup4(data + tlp_header_bytes) bytes, packed event-driven (no NOP
 * flits are simulated) into the payload positions of flits laid on an
 * absolute time grid (flit n = [n*T_f, (n+1)*T_f), datapath cycles of
 * T_c inside it). A TLP is delivered at the end of the flit holding its
 * last byte (CRC accumulation) plus a selectable PHY latency. Flit errors
 * (BER) cause a Nak and go-back-N replay; a finite retry buffer stalls the
 * packer when Acks are outstanding.
 *
 * task15: link_mode=flit256_leandros keeps the flit256_std packer but takes
 * the grid from the Leandros model: T_f = get_delay(F) - get_delay(fdiWidth)
 * + word_period (F = 2048 flit bits), words of fdiWidth bits at
 * T_w = T_f / ceil(F/fdiWidth), delivery = flit start + get_delay(F).
 */

#include <deque>
#include <random>
#include <string>

#include "base/statistics.hh"
#include "base/types.hh"
#include "dev/ucie/delay_bridge.h"
#include "mem/port.hh"
#include "params/UCIeLink.hh"
#include "sim/core.hh"
#include "sim/eventq.hh"
#include "sim/sim_object.hh"

namespace gem5 {

class UCIeLink : public SimObject
{
  private:
    class Upstream : public ResponsePort
    {
        UCIeLink &owner;
      public:
        Upstream(const std::string &n, UCIeLink &o);
      protected:
        bool recvTimingReq(PacketPtr p) override;
        Tick recvAtomic(PacketPtr p) override;
        void recvFunctional(PacketPtr p) override;
        AddrRangeList getAddrRanges() const override;
        void recvRespRetry() override;
    };

    class Downstream : public RequestPort
    {
        UCIeLink &owner;
      public:
        Downstream(const std::string &n, UCIeLink &o);
      protected:
        bool recvTimingResp(PacketPtr p) override;
        void recvReqRetry() override;
        void recvRangeChange() override;
    };

    struct Entry
    {
        PacketPtr pkt;
        Tick arrival;   // when the link accepted it
        Tick start;     // when serialization started
        Tick ready;     // when it can be delivered at the far side
        Tick occ;       // serialization time on the link
    };

    enum class Mode { Packet, Flit };
    enum class Phy { Leandros, Spec, None };

    struct DirStats : public statistics::Group
    {
        DirStats(statistics::Group *parent, const char *name);
        statistics::Scalar pkts;
        statistics::Scalar bytes;
        statistics::Scalar bits;
        statistics::Scalar busyTicks;
        statistics::Scalar totLatency;
        statistics::Scalar totQueueing;
        statistics::Scalar totDelivery;
        statistics::Scalar retriesSent;
        statistics::Scalar blockedByPeer;
        statistics::Scalar maxQueueOcc;
        statistics::Histogram latencyHist;
        statistics::Histogram queueingHist;
        statistics::Formula avgLatency;
        statistics::Formula avgQueueing;
        statistics::Formula avgDelivery;
        statistics::Formula utilization;
        statistics::Formula bandwidth;
        // task13 flit-mode stats
        statistics::Scalar tlpBytes;
        statistics::Scalar flitsSent;
        statistics::Scalar flitBytesSent;
        statistics::Scalar emptySlots;
        statistics::Scalar flitErrors;
        statistics::Scalar replaySlots;
        statistics::Scalar retryBufStalls;
        statistics::Scalar retryBufStallTicks;
        statistics::Scalar totAccum;
        statistics::Scalar totLinkLat;
        statistics::Histogram accumHist;
        statistics::Histogram linkLatHist;
        statistics::Formula avgAccum;
        statistics::Formula avgLinkLat;
        statistics::Formula dataEfficiency;
        statistics::Formula packEfficiency;
        statistics::Formula flitErrorRate;
        statistics::Scalar csbReads;
        statistics::Scalar csbWrites;
        statistics::Scalar csbStallTicks;
    };

    struct Direction
    {
        Direction(UCIeLink &o, const std::string &n, bool is_req);
        UCIeLink &owner;
        bool isReq;
        std::deque<Entry> queue;
        Tick linkFreeAt = 0;
        bool peerBusy = false;     // far side refused, waiting for retry
        bool needRetry = false;    // we refused near side, owe a retry
        // flit-mode packer state
        bool haveFlit = false;     // a flit has been opened
        uint64_t curSlot = 0;      // grid slot of the open flit
        uint64_t curOff = 0;       // next free payload offset in it
        uint64_t lastTxSlot = 0;   // last slot that carried a transmission
        uint64_t txCount = 0;      // flit transmissions (incl. replays)
        std::deque<Tick> ackTimes; // Ack arrival per un-acked flit
        EventFunctionWrapper sendEvent;
        DirStats stats;

        bool full() const { return queue.size() >= owner.queueSize; }
        void accept(PacketPtr pkt, Tick lat_ticks, Tick occ_ticks);
        void acceptFlit(PacketPtr pkt);
        Tick placeTlp(uint64_t B, uint64_t dataBytes, Tick now,
                      Tick &firstByte, unsigned &opened);
        uint64_t openFlit(uint64_t slot);
        void trySend();
        void scheduleNext();
        void retryNearSide();
    };

    Upstream cpuSide;
    Downstream memSide;
    UcieDelayHandle *delayModel;
    bool delayUnitPs;
    bool responseDelay;
    unsigned queueSize;
    unsigned headerBits;
    double constOffsetPs; // get_delay(fdiWidth) - word_period
    bool csbSync;
    // task13 flit mode
    Mode mode;
    Phy phy;
    uint64_t flitBytes = 0, payloadBytes = 0, dpBytes = 0, tlpHdr = 0;
    uint64_t bwMbps = 0;
    Tick phyTicks = 0, tFlit = 0, tCycle = 0, rttTicks = 0;
    bool leandrosGrid = false;   // task15 flit256_leandros
    uint64_t nWords = 0;         // words (fdiWidth bits) per flit
    double pFlit = 0;
    uint64_t detPeriod = 0;
    unsigned retryBufFlits = 64;
    std::mt19937_64 rng;
    std::uniform_real_distribution<double> uni{0.0, 1.0};
    Direction reqDir;
    Direction respDir;

    unsigned payloadBits(PacketPtr p) const;
    Tick toTicks(double ps) const;
    // bits -> bytes (rounded up): the Leandros model input unit
    static unsigned bytesOf(unsigned bits) { return (bits + 7) / 8; }
    Tick latencyTicks(unsigned bits) const;
    Tick occupancyTicks(unsigned bits) const;
    // flit grid helpers (exact integer arithmetic, ticks = ps)
    Tick bitsToTicks(unsigned __int128 bits) const;
    Tick slotStart(uint64_t slot) const;
    Tick cycleStart(uint64_t slot, uint64_t cyc) const;
    uint64_t slotAtOrAfter(Tick t) const;
    void positionAt(Tick t, uint64_t &slot, uint64_t &off) const;
    uint64_t tlpBytesOf(PacketPtr p) const;
    bool drawError(uint64_t tx);
    Tick csbRoundTrip(PacketPtr p, bool isWrite, unsigned size);

  public:
    UCIeLink(const UCIeLinkParams &p);
    ~UCIeLink() override;
    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;
    void init() override;
    DrainState drain() override;
};

} // namespace gem5
#endif
