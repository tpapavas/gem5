#include "dev/ucie/ucie_link.hh"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/logging.hh"
#include "base/trace.hh"
#include "mem/packet.hh"
#include "sim/core.hh"
#include "sim/stats.hh"

namespace gem5 {

// ---------------------------------------------------------------- ports

UCIeLink::Upstream::Upstream(const std::string &n, UCIeLink &o)
    : ResponsePort(n), owner(o) {}

UCIeLink::Downstream::Downstream(const std::string &n, UCIeLink &o)
    : RequestPort(n), owner(o) {}

// ---------------------------------------------------------------- stats

UCIeLink::DirStats::DirStats(statistics::Group *parent, const char *name)
    : statistics::Group(parent, name),
      ADD_STAT(pkts, statistics::units::Count::get(),
               "Packets transferred"),
      ADD_STAT(bytes, statistics::units::Byte::get(),
               "Payload bytes transferred (packet data only)"),
      ADD_STAT(bits, statistics::units::Bit::get(),
               "Link bits transferred (payload + header)"),
      ADD_STAT(busyTicks, statistics::units::Tick::get(),
               "Ticks the link was serializing (sum of occupancies of delivered packets)"),
      ADD_STAT(totLatency, statistics::units::Tick::get(),
               "Sum of (ready - arrival): queueing + model latency"),
      ADD_STAT(totQueueing, statistics::units::Tick::get(),
               "Sum of (start - arrival): wait for link to be free"),
      ADD_STAT(totDelivery, statistics::units::Tick::get(),
               "Sum of (delivered - arrival), incl. far-side backpressure"),
      ADD_STAT(retriesSent, statistics::units::Count::get(),
               "Packets refused because the link queue was full"),
      ADD_STAT(blockedByPeer, statistics::units::Count::get(),
               "Times the far side refused a packet"),
      ADD_STAT(maxQueueOcc, statistics::units::Count::get(),
               "Max queue occupancy"),
      ADD_STAT(latencyHist, statistics::units::Tick::get(),
               "Histogram of (ready - arrival)"),
      ADD_STAT(queueingHist, statistics::units::Tick::get(),
               "Histogram of (start - arrival)"),
      ADD_STAT(avgLatency, statistics::units::Rate<
               statistics::units::Tick, statistics::units::Count>::get(),
               "Average link latency incl. queueing", totLatency / pkts),
      ADD_STAT(avgQueueing, statistics::units::Rate<
               statistics::units::Tick, statistics::units::Count>::get(),
               "Average queueing delay", totQueueing / pkts),
      ADD_STAT(avgDelivery, statistics::units::Rate<
               statistics::units::Tick, statistics::units::Count>::get(),
               "Average arrival-to-delivery time", totDelivery / pkts),
      ADD_STAT(utilization, statistics::units::Ratio::get(),
               "Fraction of time the link was serializing",
               busyTicks / simTicks),
      ADD_STAT(bandwidth, statistics::units::Rate<
               statistics::units::Byte, statistics::units::Second>::get(),
               "Achieved payload bandwidth", bytes / simSeconds),
      ADD_STAT(tlpBytes, statistics::units::Byte::get(),
               "flit mode: TLP bytes packed (data + TLP header, DWORD aligned)"),
      ADD_STAT(flitsSent, statistics::units::Count::get(),
               "flit mode: flit transmissions (incl. replays)"),
      ADD_STAT(flitBytesSent, statistics::units::Byte::get(),
               "flit mode: transmitted flit bytes (flitsSent * flit size)"),
      ADD_STAT(emptySlots, statistics::units::Count::get(),
               "flit mode: NOP-equivalent empty grid slots between transmissions (analytic, not simulated)"),
      ADD_STAT(flitErrors, statistics::units::Count::get(),
               "flit mode: corrupted flit transmissions (Nak + replay)"),
      ADD_STAT(replaySlots, statistics::units::Count::get(),
               "flit mode: flit slots lost to Nak round-trips/replays"),
      ADD_STAT(retryBufStalls, statistics::units::Count::get(),
               "flit mode: times the packer stalled on a full retry buffer"),
      ADD_STAT(retryBufStallTicks, statistics::units::Tick::get(),
               "flit mode: ticks lost to retry-buffer stalls"),
      ADD_STAT(totAccum, statistics::units::Tick::get(),
               "flit mode: sum of accumulation time (last-byte cycle end -> flit end)"),
      ADD_STAT(totLinkLat, statistics::units::Tick::get(),
               "flit mode: sum of (ready - first byte cycle start)"),
      ADD_STAT(accumHist, statistics::units::Tick::get(),
               "flit mode: histogram of accumulation time"),
      ADD_STAT(linkLatHist, statistics::units::Tick::get(),
               "flit mode: histogram of (ready - first byte)"),
      ADD_STAT(avgAccum, statistics::units::Rate<
               statistics::units::Tick, statistics::units::Count>::get(),
               "flit mode: average accumulation time", totAccum / pkts),
      ADD_STAT(avgLinkLat, statistics::units::Rate<
               statistics::units::Tick, statistics::units::Count>::get(),
               "flit mode: average (ready - first byte) = MUG5 Table I latency",
               totLinkLat / pkts),
      ADD_STAT(dataEfficiency, statistics::units::Ratio::get(),
               "flit mode: packet data bytes / transmitted flit bytes", bytes / flitBytesSent),
      ADD_STAT(packEfficiency, statistics::units::Ratio::get(),
               "flit mode: TLP bytes / transmitted flit bytes", tlpBytes / flitBytesSent),
      ADD_STAT(flitErrorRate, statistics::units::Ratio::get(),
               "flit mode: flitErrors / flitsSent", flitErrors / flitsSent),
      ADD_STAT(csbReads, statistics::units::Count::get(),
               "csb_sync: register reads through the link"),
      ADD_STAT(csbWrites, statistics::units::Count::get(),
               "csb_sync: register writes through the link"),
      ADD_STAT(csbStallTicks, statistics::units::Tick::get(),
               "csb_sync: total round-trip ticks returned to the CPU as stall")
{
    latencyHist.init(20);
    queueingHist.init(20);
    accumHist.init(20);
    linkLatHist.init(20);
}

// ------------------------------------------------------------ direction

UCIeLink::Direction::Direction(UCIeLink &o, const std::string &n,
                               bool is_req)
    : owner(o), isReq(is_req),
      sendEvent([this]{ trySend(); }, o.name() + "." + n + ".sendEvent"),
      stats(&o, is_req ? "req" : "resp")
{}

void
UCIeLink::Direction::accept(PacketPtr pkt, Tick lat, Tick occ)
{
    const Tick now = curTick();
    Entry e;
    e.pkt = pkt;
    e.arrival = now;
    e.start = std::max(now, linkFreeAt);
    linkFreeAt = e.start + occ;
    e.ready = e.start + lat;
    e.occ = occ;
    queue.push_back(e);

    stats.pkts++;
    stats.bytes += pkt->hasData() ? pkt->getSize() : 0;
    stats.totLatency += e.ready - e.arrival;
    stats.totQueueing += e.start - e.arrival;
    stats.latencyHist.sample(e.ready - e.arrival);
    stats.queueingHist.sample(e.start - e.arrival);
    if ((double)queue.size() > stats.maxQueueOcc.value())
        stats.maxQueueOcc = queue.size();

    if (queue.size() == 1 && !peerBusy)
        scheduleNext();
}

// task13: event-driven flit packer (JETCAS "opt": nothing happens on idle
// grid slots, positions are computed when a packet arrives).
// Place a TLP of B bytes arriving at `now` on the flit grid; updates the
// cursor and the flit/latency stats; returns ready (delivery) tick.
Tick
UCIeLink::Direction::placeTlp(uint64_t B, uint64_t dataBytes, Tick now,
                              Tick &firstByte, unsigned &opened)
{
    const uint64_t P = owner.payloadBytes, W = owner.dpBytes;

    uint64_t s, off;
    owner.positionAt(now, s, off);              // first cycle >= now
    if (haveFlit && (curSlot > s || (curSlot == s && curOff > off))) {
        s = curSlot;                            // link busy: behind cursor
        off = curOff;
    }
    if (off >= P) { s++; off = 0; }

    uint64_t remaining = B;
    opened = 0;
    bool first = true;
    firstByte = now;
    for (;;) {
        if (first) {
            // first attempt: replay/retry-buffer penalties of this TLP's
            // first flit count as link latency, not queueing
            firstByte = std::max(now, owner.cycleStart(s, off / W));
            first = false;
        }
        if (!haveFlit || s != curSlot) {
            s = openFlit(s);
            haveFlit = true;
            curSlot = s;
            opened++;
        }
        const uint64_t n = std::min(remaining, P - off);
        off += n;
        remaining -= n;
        curOff = off;
        if (remaining == 0)
            break;
        s++;
        off = 0;
    }
    const uint64_t lastCyc = (off - 1) / W;
    const Tick flitEnd = owner.slotStart(s + 1);
    const Tick lastEnd = std::min(owner.cycleStart(s, lastCyc + 1), flitEnd);
    const Tick ready = std::max<Tick>(now + 1, flitEnd + owner.phyTicks);

    stats.pkts++;
    stats.bytes += dataBytes;
    stats.tlpBytes += B;
    stats.bits += B * 8;
    stats.totLatency += ready - now;
    stats.totQueueing += firstByte - now;
    stats.latencyHist.sample(ready - now);
    stats.queueingHist.sample(firstByte - now);
    stats.totAccum += flitEnd - lastEnd;
    stats.accumHist.sample(flitEnd - lastEnd);
    stats.totLinkLat += ready - firstByte;
    stats.linkLatHist.sample(ready - firstByte);
    return ready;
}

void
UCIeLink::Direction::acceptFlit(PacketPtr pkt)
{
    const Tick now = curTick();
    Entry e;
    unsigned opened = 0;
    e.pkt = pkt;
    e.arrival = now;
    e.ready = placeTlp(owner.tlpBytesOf(pkt),
                       pkt->hasData() ? pkt->getSize() : 0, now, e.start,
                       opened);
    e.occ = opened * owner.tFlit;
    queue.push_back(e);
    if ((double)queue.size() > stats.maxQueueOcc.value())
        stats.maxQueueOcc = queue.size();

    if (queue.size() == 1 && !peerBusy)
        scheduleNext();
}

// Transmit a new flit in grid slot `slot` (or later). Applies the retry
// buffer limit (Acks return rttTicks after the flit end) and BER errors
// (Nak -> go-back-N replay from this flit once the Nak arrives). Returns
// the slot of the first good transmission.
uint64_t
UCIeLink::Direction::openFlit(uint64_t slot)
{
    for (;;) {
        const Tick start = owner.slotStart(slot);
        while (!ackTimes.empty() && ackTimes.front() <= start)
            ackTimes.pop_front();
        if (ackTimes.size() >= owner.retryBufFlits) {
            const uint64_t ns = owner.slotAtOrAfter(ackTimes.front());
            stats.retryBufStalls++;
            stats.retryBufStallTicks += owner.slotStart(ns) - start;
            slot = std::max(ns, slot + 1);
            continue;
        }
        if (txCount > 0 && slot > lastTxSlot + 1)
            stats.emptySlots += slot - lastTxSlot - 1;
        txCount++;
        stats.flitsSent++;
        stats.flitBytesSent += owner.flitBytes;
        const Tick end = owner.slotStart(slot + 1);
        ackTimes.push_back(end + owner.rttTicks);
        lastTxSlot = slot;
        if (!owner.drawError(txCount))
            return slot;
        // CRC fails at the receiver: Nak reaches the sender after the
        // round trip; everything from this flit on is replayed.
        stats.flitErrors++;
        const uint64_t r = std::max(owner.slotAtOrAfter(end + owner.rttTicks),
                                    slot + 1);
        stats.replaySlots += r - slot;
        lastTxSlot = r - 1;
        slot = r;
    }
}

void
UCIeLink::Direction::scheduleNext()
{
    if (queue.empty() || peerBusy || sendEvent.scheduled())
        return;
    owner.schedule(sendEvent, std::max(curTick(), queue.front().ready));
}

void
UCIeLink::Direction::trySend()
{
    while (!queue.empty() && !peerBusy &&
           queue.front().ready <= curTick()) {
        Entry &e = queue.front();
        bool ok = isReq ? owner.memSide.sendTimingReq(e.pkt)
                        : owner.cpuSide.sendTimingResp(e.pkt);
        if (!ok) {
            peerBusy = true;
            stats.blockedByPeer++;
            return;
        }
        stats.totDelivery += curTick() - e.arrival;
        // counted at delivery: start+occ <= ready <= now, so utilization
        // can never exceed 1 at the end of a run
        stats.busyTicks += e.occ;
        queue.pop_front();
        retryNearSide();
    }
    scheduleNext();
}

void
UCIeLink::Direction::retryNearSide()
{
    if (needRetry && !full()) {
        needRetry = false;
        if (isReq)
            owner.cpuSide.sendRetryReq();
        else
            owner.memSide.sendRetryResp();
    }
}

// ----------------------------------------------------------------- link

UCIeLink::UCIeLink(const UCIeLinkParams &p)
    : SimObject(p),
      cpuSide(name() + ".cpu_side", *this),
      memSide(name() + ".mem_side", *this),
      delayModel(nullptr),
      delayUnitPs(p.delay_unit_ps),
      responseDelay(p.apply_response_delay),
      queueSize(p.queue_size),
      headerBits(p.header_bits),
      constOffsetPs(0),
      csbSync(p.csb_sync),
      mode(Mode::Packet), phy(Phy::Leandros),
      rng(p.rng_seed),
      reqDir(*this, "req", true),
      respDir(*this, "resp", false)
{
    fatal_if(queueSize == 0, "UCIeLink %s: queue_size must be > 0", name());
    delayModel = ucie_delay_create(p.config_file.c_str());
    if (!delayModel)
        fatal("UCIeLink: failed to initialize delay model from %s",
              p.config_file);

    const int fdi = ucie_delay_fdi_width(delayModel);   // bytes (task16)
    const double wp = ucie_delay_word_period(delayModel);
    fatal_if(fdi <= 0 || !(wp > 0), "UCIeLink %s: bad model constants",
             name());
    constOffsetPs = ucie_delay_get(delayModel, fdi) - wp;

    inform("UCIeLink %s: config %s fdiWidth=%d word_period=%.2fps "
           "digital_period=%.2fps d_channel=%.4fps lat(8B)=%.2fps "
           "lat(64B)=%.2fps lat(72B)=%.2fps occ(72B)=%.2fps "
           "queue_size=%u header_bits=%u resp_delay=%d",
           name(), p.config_file, fdi, wp,
           ucie_delay_digital_period(delayModel),
           ucie_delay_channel(delayModel),
           ucie_delay_get(delayModel, 8), ucie_delay_get(delayModel, 64),
           ucie_delay_get(delayModel, 72),
           ucie_delay_get(delayModel, 72) - constOffsetPs,
           queueSize, headerBits, (int)responseDelay);

    // ---------------- task13 flit mode
    if (p.link_mode == "packet") {
        mode = Mode::Packet;
    } else if (p.link_mode == "flit256_std") {
        mode = Mode::Flit; flitBytes = 256; payloadBytes = 236;
    } else if (p.link_mode == "flit256_leandros") {
        mode = Mode::Flit; flitBytes = 256; payloadBytes = 236;
        leandrosGrid = true;
    } else if (p.link_mode == "flit68") {
        mode = Mode::Flit; flitBytes = 68; payloadBytes = 64;
    } else {
        fatal("UCIeLink %s: unknown link_mode '%s'", name(), p.link_mode);
    }
    if (mode == Mode::Flit) {
        if (p.flit_payload_bytes)
            payloadBytes = p.flit_payload_bytes;
        fatal_if(payloadBytes == 0 || payloadBytes > flitBytes ||
                 payloadBytes % 4, "UCIeLink %s: bad flit payload %llu",
                 name(), (unsigned long long)payloadBytes);
        fatal_if(p.datapath_bits == 0 || p.datapath_bits % 32,
                 "UCIeLink %s: datapath_bits must be a multiple of 32", name());
        dpBytes = p.datapath_bits / 8;
        tlpHdr = p.tlp_header_bytes;
        bwMbps = (uint64_t)std::llround(p.lanes * p.gtps * 1000.0);
        fatal_if(bwMbps == 0, "UCIeLink %s: zero bandwidth", name());
        tCycle = bitsToTicks(p.datapath_bits);
        tFlit = bitsToTicks(flitBytes * 8);
        if (leandrosGrid) {
            // task15: whole flit timing from the Leandros model, rounded to
            // integer ps. occupancy = E(F)*word_period, full PHY pipeline
            // (CDC, serializer, NN channel, RX) = get_delay(F).
            // task16: get_delay(X) takes X in bytes; a word is fdiWidth B.
            const int F = (int)flitBytes;
            const double dF = ucie_delay_get(delayModel, F);
            const double occPs = dF - ucie_delay_get(delayModel, fdi) + wp;
            fatal_if(!(occPs > 0) || !(dF >= occPs), "UCIeLink %s: bad "
                     "Leandros flit timing", name());
            const Tick psT = sim_clock::as_int::ps;
            const Tick scale = delayUnitPs ? psT : 1;
            tFlit = (Tick)std::llround(occPs) * scale;
            const Tick latF = (Tick)std::llround(dF) * scale;
            nWords = (flitBytes + fdi - 1) / fdi;
            dpBytes = fdi;
            tCycle = tFlit / nWords;
            phyTicks = latF - tFlit;
            bwMbps = (uint64_t)std::llround((double)F * 8 * 1e6 /
                                            ((double)tFlit / psT));
            inform("UCIeLink %s: flit256_leandros F=%d bytes fdiWidth=%d B "
                   "word_period=%.3fps words/flit=%llu T_word=%llups "
                   "get_delay(F)=%.3fps get_delay(fdi)=%.3fps occ(F)=%.3fps "
                   "-> T_f=%llu ticks, flit latency=%llu ticks "
                   "(phy_model/lanes/gtps/datapath_bits ignored)",
                   name(), F, fdi, wp, (unsigned long long)nWords,
                   (unsigned long long)tCycle, dF,
                   ucie_delay_get(delayModel, fdi), occPs,
                   (unsigned long long)tFlit, (unsigned long long)latF);
        } else if (p.phy_model == "leandros") {
            phy = Phy::Leandros;
            phyTicks = toTicks(constOffsetPs);
        } else if (p.phy_model == "spec") {
            phy = Phy::Spec;
            phyTicks = toTicks((double)p.spec_adapter_ps + p.spec_phy_ps +
                               ucie_delay_channel(delayModel));
        } else if (p.phy_model == "none") {
            phy = Phy::None;
            phyTicks = 0;
        } else {
            fatal("UCIeLink %s: unknown phy_model '%s'", name(), p.phy_model);
        }
        rttTicks = 2 * phyTicks + tFlit;
        fatal_if(p.ber < 0 || p.ber >= 1, "UCIeLink %s: bad ber", name());
        pFlit = p.ber > 0 ? -std::expm1((double)flitBytes * 8 *
                                        std::log1p(-p.ber)) : 0.0;
        detPeriod = (p.ber_deterministic && pFlit > 0) ?
            (uint64_t)std::ceil(1.0 / pFlit) : 0;
        retryBufFlits = std::min(255u, std::max(1u, p.retry_buffer_flits));
        inform("UCIeLink %s: link_mode=%s flit=%lluB payload=%lluB "
               "bw=%.1fGb/s T_c=%llups T_f=%llups tlp_hdr=%lluB "
               "phy_model=%s phy=%llups rtt=%llups ber=%g P_flit=%.3e "
               "det_period=%llu retry_buf=%u seed=%llu",
               name(), p.link_mode, (unsigned long long)flitBytes,
               (unsigned long long)payloadBytes, bwMbps / 1000.0,
               (unsigned long long)tCycle, (unsigned long long)tFlit,
               (unsigned long long)tlpHdr,
               leandrosGrid ? std::string("leandros_grid") : p.phy_model,
               (unsigned long long)phyTicks, (unsigned long long)rttTicks,
               p.ber, pFlit, (unsigned long long)detPeriod, retryBufFlits,
               (unsigned long long)p.rng_seed);
    }
}

UCIeLink::~UCIeLink()
{
    ucie_delay_destroy(delayModel);
}

void
UCIeLink::init()
{
    SimObject::init();
    fatal_if(!cpuSide.isConnected() || !memSide.isConnected(),
             "UCIeLink %s: both ports must be connected", name());
    cpuSide.sendRangeChange();
}

Port &
UCIeLink::getPort(const std::string &n, PortID id)
{
    if (n == "cpu_side") return cpuSide;
    if (n == "mem_side") return memSide;
    return SimObject::getPort(n, id);
}

DrainState
UCIeLink::drain()
{
    return (reqDir.queue.empty() && respDir.queue.empty()) ?
        DrainState::Drained : DrainState::Draining;
}

unsigned
UCIeLink::payloadBits(PacketPtr p) const
{
    const uint64_t data = p->hasData() ? (uint64_t)p->getSize() * 8 : 0;
    const uint64_t b = data + headerBits;
    fatal_if(b > (uint64_t)std::numeric_limits<int>::max(),
             "UCIe packet too large");
    return std::max<uint64_t>(1, b);
}

Tick
UCIeLink::toTicks(double ps) const
{
    if (!std::isfinite(ps) || ps < 0)
        fatal("UCIeLink %s: invalid model delay %f", name(), ps);
    const double t = delayUnitPs ? ps * (double)sim_clock::as_int::ps : ps;
    return static_cast<Tick>(std::ceil(t));
}

Tick
UCIeLink::latencyTicks(unsigned bits) const
{
    // task16: Leandros' get_delay(X) takes X in BYTES (fdiWidth = 64 B).
    return std::max<Tick>(1,
        toTicks(ucie_delay_get(delayModel, (int)bytesOf(bits))));
}

Tick
UCIeLink::occupancyTicks(unsigned bits) const
{
    return std::max<Tick>(1,
        toTicks(ucie_delay_get(delayModel, (int)bytesOf(bits)) -
                constOffsetPs));
}

Tick
UCIeLink::bitsToTicks(unsigned __int128 bits) const
{
    // ps = bits * 1e6 / (Mb/s), rounded up; exact for integer inputs
    const unsigned __int128 num = bits * 1000000u;
    return (Tick)((num + bwMbps - 1) / bwMbps) *
           (Tick)(sim_clock::as_int::ps);
}

Tick
UCIeLink::slotStart(uint64_t slot) const
{
    if (leandrosGrid)
        return (Tick)slot * tFlit;
    return bitsToTicks((unsigned __int128)slot * flitBytes * 8);
}

Tick
UCIeLink::cycleStart(uint64_t slot, uint64_t cyc) const
{
    if (cyc * dpBytes >= flitBytes)
        return slotStart(slot + 1);
    if (leandrosGrid)   // word c of the flit starts at c*T_f/nWords
        return slotStart(slot) + (Tick)((unsigned __int128)cyc * tFlit /
                                        nWords);
    return bitsToTicks((unsigned __int128)slot * flitBytes * 8 +
                       cyc * dpBytes * 8);
}

uint64_t
UCIeLink::slotAtOrAfter(Tick t) const
{
    if (leandrosGrid)
        return (uint64_t)((t + tFlit - 1) / tFlit);
    const Tick ps = t / sim_clock::as_int::ps;
    uint64_t s = (uint64_t)(((unsigned __int128)ps * bwMbps) /
                            ((unsigned __int128)flitBytes * 8 * 1000000u));
    while (slotStart(s) < t) s++;
    while (s > 0 && slotStart(s - 1) >= t) s--;
    return s;
}

void
UCIeLink::positionAt(Tick t, uint64_t &slot, uint64_t &off) const
{
    uint64_t s = slotAtOrAfter(t);
    if (slotStart(s) > t && s > 0)
        s--;                                    // slot containing t
    uint64_t c = 0;
    while (cycleStart(s, c) < t) c++;
    off = c * dpBytes;
    slot = s;
    if (off >= payloadBytes) { slot = s + 1; off = 0; }
}

uint64_t
UCIeLink::tlpBytesOf(PacketPtr p) const
{
    const uint64_t b = (p->hasData() ? (uint64_t)p->getSize() : 0) + tlpHdr;
    return std::max<uint64_t>(4, (b + 3) & ~(uint64_t)3);
}

bool
UCIeLink::drawError(uint64_t tx)
{
    if (pFlit <= 0)
        return false;
    if (detPeriod)
        return tx % detPeriod == 0;
    return uni(rng) < pFlit;
}

// task13 CSB: synchronous register path (MinorCPU NvDlaReadReg/WriteReg
// read the response right after sendTimingReq). The packet is forwarded
// immediately (device answers inside recvTimingReq); the round trip of the
// link model is added to headerDelay and the CPU stalls for it.
Tick
UCIeLink::csbRoundTrip(PacketPtr p, bool isWrite, unsigned size)
{
    const Tick now = curTick();
    Tick rtt = 0;
    if (mode == Mode::Flit) {
        Tick fb; unsigned op;
        auto tlp = [this](uint64_t data) {
            const uint64_t b = data + tlpHdr;
            return std::max<uint64_t>(4, (b + 3) & ~(uint64_t)3);
        };
        const uint64_t reqData = isWrite ? size : 0;
        const uint64_t respData = isWrite ? 0 : size;
        const Tick r1 = reqDir.placeTlp(tlp(reqData), reqData, now, fb, op);
        reqDir.stats.busyTicks += op * tFlit;
        const Tick r2 = responseDelay ?
            respDir.placeTlp(tlp(respData), respData, r1, fb, op) : r1;
        if (responseDelay)
            respDir.stats.busyTicks += op * tFlit;
        rtt = r2 - now;
    } else {
        auto acct = [this](Direction &d, unsigned bits, uint64_t data) {
            const Tick l = latencyTicks(bits);
            d.stats.pkts++; d.stats.bits += bits; d.stats.bytes += data;
            d.stats.totLatency += l; d.stats.latencyHist.sample(l);
            d.stats.busyTicks += occupancyTicks(bits);
            return l;
        };
        const uint64_t reqBits = (isWrite ? size * 8 : 0) + headerBits;
        const uint64_t respBits = (isWrite ? 0 : size * 8) + headerBits;
        rtt = acct(reqDir, std::max<uint64_t>(1, reqBits), isWrite ? size : 0);
        if (responseDelay)
            rtt += acct(respDir, std::max<uint64_t>(1, respBits),
                        isWrite ? 0 : size);
    }
    reqDir.stats.totDelivery += rtt;
    if (isWrite) reqDir.stats.csbWrites++; else reqDir.stats.csbReads++;
    reqDir.stats.csbStallTicks += rtt;
    return rtt;
}

bool
UCIeLink::Upstream::recvTimingReq(PacketPtr p)
{
    if (owner.csbSync) {
        const bool isWrite = p->isWrite();
        const unsigned size = p->getSize();
        // NvDlaDeviceSE answers inside recvTimingReq and returns false for
        // the status polls (0x20000/0x20004) although the response data is
        // valid; MinorCPU ignores the return value, so do we.
        const bool ok = owner.memSide.sendTimingReq(p);
        p->headerDelay += owner.csbRoundTrip(p, isWrite, size);
        return ok;
    }
    Direction &d = owner.reqDir;
    if (d.full() || d.needRetry) {
        if (!d.needRetry)
            d.stats.retriesSent++;
        d.needRetry = true;
        return false;
    }
    if (owner.mode == Mode::Flit) {
        d.acceptFlit(p);
        return true;
    }
    const unsigned b = owner.payloadBits(p);
    d.stats.bits += b;
    d.accept(p, owner.latencyTicks(b), owner.occupancyTicks(b));
    return true;
}

bool
UCIeLink::Downstream::recvTimingResp(PacketPtr p)
{
    Direction &d = owner.respDir;
    if (d.full() || d.needRetry) {
        if (!d.needRetry)
            d.stats.retriesSent++;
        d.needRetry = true;
        return false;
    }
    if (owner.mode == Mode::Flit) {
        if (owner.responseDelay)
            d.acceptFlit(p);
        else
            d.accept(p, 1, 0);
        return true;
    }
    const unsigned b = owner.payloadBits(p);
    d.stats.bits += b;
    if (owner.responseDelay)
        d.accept(p, owner.latencyTicks(b), owner.occupancyTicks(b));
    else
        d.accept(p, 1, 0);
    return true;
}

void
UCIeLink::Downstream::recvReqRetry()
{
    owner.reqDir.peerBusy = false;
    owner.reqDir.trySend();
}

void
UCIeLink::Upstream::recvRespRetry()
{
    owner.respDir.peerBusy = false;
    owner.respDir.trySend();
}

Tick
UCIeLink::Upstream::recvAtomic(PacketPtr p)
{
    auto lat = [this](PacketPtr q) -> Tick {
        if (owner.mode == Mode::Flit) {
            const uint64_t nf = (owner.tlpBytesOf(q) + owner.payloadBytes - 1)
                                / owner.payloadBytes;
            return nf * owner.tFlit + owner.phyTicks;
        }
        return owner.latencyTicks(owner.payloadBits(q));
    };
    const Tick req = lat(p);
    const Tick t = owner.memSide.sendAtomic(p);
    Tick resp = 0;
    if (p->isResponse() && owner.responseDelay)
        resp = lat(p);
    return t + req + resp;
}

void
UCIeLink::Upstream::recvFunctional(PacketPtr p)
{
    // Check in-flight packets first so functional accesses see them.
    for (auto &e : owner.reqDir.queue)
        if (p->trySatisfyFunctional(e.pkt))
            return;
    for (auto &e : owner.respDir.queue)
        if (p->trySatisfyFunctional(e.pkt))
            return;
    owner.memSide.sendFunctional(p);
}

AddrRangeList
UCIeLink::Upstream::getAddrRanges() const
{
    return owner.memSide.getAddrRanges();
}

void
UCIeLink::Downstream::recvRangeChange()
{
    owner.cpuSide.sendRangeChange();
}

} // namespace gem5
