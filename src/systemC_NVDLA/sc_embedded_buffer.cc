#include "systemC_NVDLA/sc_embedded_buffer.hh"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace sc_nvdla_spm
{

using WriteBacks = std::vector<std::pair<uint64_t, std::vector<uint8_t>>>;

void
SpmStats::dump(std::ostream &os) const
{
#define SC_NVDLA_SPM_STATS_DUMP(name, desc) \
    os << "spm." #name " : " << name << "   # " desc "\n";
    SC_NVDLA_SPM_STATS(SC_NVDLA_SPM_STATS_DUMP)
#undef SC_NVDLA_SPM_STATS_DUMP
}

static bool
byteEnabled(const uint8_t *be, uint32_t be_len, uint32_t i)
{
    return be == nullptr || be_len == 0 || be[i % be_len] != 0;
}

EmbeddedBuffer::EmbeddedBuffer(BufMode mode, uint32_t line_size,
                               uint32_t num_lines, uint32_t _assoc,
                               uint32_t fill_chunk)
    : _mode(mode), lineSz(line_size), assoc(_assoc),
      numSets(num_lines / _assoc),
      fillChunk(std::min(fill_chunk, line_size)),
      lines((size_t)(num_lines / _assoc) * _assoc)
{
    assert(line_size && (line_size & (line_size - 1)) == 0);
    assert(_assoc && num_lines % _assoc == 0 && numSets > 0);
    assert(fillChunk && line_size % fillChunk == 0);
    for (auto &l : lines) {
        l.data.assign(lineSz, 0);
        l.dmask.assign(lineSz, 0);
    }
}

EmbeddedBuffer::Line *
EmbeddedBuffer::find(uint64_t line_addr)
{
    Line *set = &lines[(size_t)setOf(line_addr) * assoc];
    for (uint32_t w = 0; w < assoc; w++)
        if (set[w].valid && set[w].tag == line_addr)
            return &set[w];
    return nullptr;
}

const EmbeddedBuffer::Line *
EmbeddedBuffer::find(uint64_t line_addr) const
{
    return const_cast<EmbeddedBuffer *>(this)->find(line_addr);
}

bool
EmbeddedBuffer::hasLine(uint64_t line_addr) const
{
    return find(line_addr) != nullptr;
}

bool
EmbeddedBuffer::inFlight(uint64_t line_addr) const
{
    return inflight.count(line_addr) != 0;
}

bool
EmbeddedBuffer::setFull(uint64_t line_addr) const
{
    const Line *set = &lines[(size_t)setOf(line_addr) * assoc];
    for (uint32_t w = 0; w < assoc; w++)
        if (!set[w].valid)
            return false;
    return true;
}

// [DIRTY_MASK] as the Verilator buffer: only the bytes NVDLA wrote are
// written back, one write per contiguous run (split at fillChunk
// boundaries). They are also kept in the overlay: a fill of this line that
// reads memory before the write-back lands must not return older data.
void
EmbeddedBuffer::collectDirty(Line &l, uint64_t base, WriteBacks &wbs)
{
    Overlay &ov = overlay[base];
    if (ov.data.empty()) {
        ov.data.assign(lineSz, 0);
        ov.mask.assign(lineSz, 0);
    }
    uint32_t i = 0;
    while (i < lineSz) {
        if (!l.dmask[i]) {
            i++;
            continue;
        }
        uint32_t start = i;
        while (i < lineSz && l.dmask[i] && (i == start || i % fillChunk))
            i++;
        for (uint32_t b = start; b < i; b++) {
            ov.data[b] = l.data[b];
            ov.mask[b] = 1;
        }
        wbs.emplace_back(base + start,
            std::vector<uint8_t>(l.data.begin() + start,
                                 l.data.begin() + i));
    }
    std::fill(l.dmask.begin(), l.dmask.end(), 0);
    l.dirty = false;
}

void
EmbeddedBuffer::writeBack(MemPort &req, uint64_t, WriteBacks &wbs)
{
    for (auto &w : wbs)
        req.memWrite(w.first, w.second.data(), w.second.size());
    wbs.clear();
}

EmbeddedBuffer::Line *
EmbeddedBuffer::allocate(MemPort &req, uint64_t line_addr, WriteBacks &wbs)
{
    Line *set = &lines[(size_t)setOf(line_addr) * assoc];
    Line *victim = nullptr;
    for (uint32_t w = 0; w < assoc; w++) {
        if (!set[w].valid)
            return &set[w];
        if (!victim || set[w].lru < victim->lru)
            victim = &set[w];
    }
    if (_mode == BUF_MODE_PFT_CUTOFF)
        return nullptr;
    SpmStats &s = req.spmStats();
    s.evictions++;
    if (victim->dirty) {
        s.dirty_evictions++;
        const size_t before = wbs.size();
        collectDirty(*victim, victim->tag, wbs);
        for (size_t k = before; k < wbs.size(); k++)
            s.writeback_bytes += wbs[k].second.size();
    }
    victim->valid = false;
    return victim;
}

EmbeddedBuffer::Line *
EmbeddedBuffer::fetchAndInstall(MemPort &req, uint64_t line_addr,
                                WriteBacks &wbs)
{
    assert(!inflight.count(line_addr));
    const uint64_t gen = flushGen;
    auto ev = std::make_shared<sc_core::sc_event>();
    inflight[line_addr] = ev;

    std::vector<uint8_t> buf(lineSz);
    req.memReadChunks(line_addr, buf.data(), lineSz, fillChunk);   // blocks
    auto ov = overlay.find(line_addr);
    if (ov != overlay.end())
        for (uint32_t b = 0; b < lineSz; b++)
            if (ov->second.mask[b])
                buf[b] = ov->second.data[b];

    SpmStats &s = req.spmStats();
    Line *l = nullptr;
    if (gen != flushGen) {
        s.fills_dropped++;   // a flush ran while the line was in flight
    } else if ((l = find(line_addr))) {
        s.fills_dropped++;   // already there
    } else if ((l = allocate(req, line_addr, wbs))) {
        l->data = buf;
        std::fill(l->dmask.begin(), l->dmask.end(), 0);
        l->tag = line_addr;
        l->valid = true;
        l->dirty = false;
        l->lru = ++lruClock;
        s.line_fills++;
    } else {
        s.fills_dropped++;   // pft-cut: set full
    }
    inflight.erase(line_addr);
    ev->notify();
    return l;
}

EmbeddedBuffer::Line *
EmbeddedBuffer::demandFill(MemPort &req, uint64_t line_addr, bool &merged,
                           WriteBacks &wbs)
{
    while (true) {
        if (Line *l = find(line_addr))
            return l;
        auto it = inflight.find(line_addr);
        if (it != inflight.end()) {
            merged = true;
            auto ev = it->second;
            sc_core::wait(*ev);
            continue;
        }
        if (Line *l = fetchAndInstall(req, line_addr, wbs))
            return l;
        // dropped by a flush: fetch again
    }
}

EmbeddedBuffer::ReadBuf &
EmbeddedBuffer::readBufOf(unsigned id)
{
    for (auto &rb : readBufs)
        if (rb.id == id)
            return rb;
    readBufs.push_back(ReadBuf{id, {}});
    return readBufs.back();
}

uint32_t
EmbeddedBuffer::read(MemPort &req, uint64_t addr, uint8_t *data,
                     uint32_t len)
{
    SpmStats &s = req.spmStats();
    uint32_t hit_beats = 0;
    uint32_t done = 0;
    WriteBacks wbs;
    while (done < len) {
        const uint64_t a = addr + done;
        const uint64_t la = lineAddr(a);
        const uint32_t off = a - la;
        const uint32_t n = std::min<uint32_t>(len - done, lineSz - off);

        if (_mode == BUF_MODE_ALL) {
            Line *l = find(la);
            if (l) {
                s.read_hits++;
                hit_beats++;
            } else {
                s.read_misses++;
                bool merged = false;
                l = demandFill(req, la, merged, wbs);
                if (merged)
                    s.read_miss_merged++;
            }
            l->lru = ++lruClock;
            std::memcpy(data + done, l->data.data() + off, n);
            writeBack(req, la, wbs);
        } else {
            ReadBuf *rb = &readBufOf(req.spmId());
            auto inRb = [&]() {
                return std::find_if(rb->lines.begin(), rb->lines.end(),
                    [la](const std::pair<uint64_t,
                                         std::vector<uint8_t>> &e)
                    { return e.first == la; });
            };
            auto it = inRb();
            if (it == rb->lines.end()) {
                // a prefetch of this line is in flight: wait for it
                auto f = inflight.find(la);
                if (f != inflight.end()) {
                    auto ev = f->second;
                    sc_core::wait(*ev);
                    s.read_miss_merged++;
                    rb = &readBufOf(req.spmId());
                    it = inRb();
                }
            }
            if (it == rb->lines.end()) {
                if (Line *l = find(la)) {
                    // the line leaves the prefetch buffer
                    rb->lines.emplace_back(la, l->data);
                    if (rb->lines.size() > READ_BUF_LINES)
                        rb->lines.pop_front();
                    it = std::prev(rb->lines.end());
                    l->valid = false;
                    s.prefetch_used++;
                }
            }
            if (it != rb->lines.end()) {
                s.read_hits++;
                hit_beats++;
                std::memcpy(data + done, it->second.data() + off, n);
            } else {
                // not prefetched: read from memory, bypassing the buffer
                s.read_misses++;
                req.memRead(a, data + done, n);
            }
        }
        done += n;
    }
    return hit_beats;
}

void
EmbeddedBuffer::updateCopies(uint64_t addr, const uint8_t *data,
                             uint32_t len, const uint8_t *be,
                             uint32_t be_len, uint32_t be_off)
{
    uint32_t done = 0;
    while (done < len) {
        const uint64_t a = addr + done;
        const uint64_t la = lineAddr(a);
        const uint32_t off = a - la;
        const uint32_t n = std::min<uint32_t>(len - done, lineSz - off);
        auto copy = [&](uint8_t *dst) {
            for (uint32_t i = 0; i < n; i++)
                if (byteEnabled(be, be_len, be_off + done + i))
                    dst[off + i] = data[done + i];
        };
        if (Line *l = find(la))
            copy(l->data.data());
        for (auto &rb : readBufs)
            for (auto &e : rb.lines)
                if (e.first == la)
                    copy(e.second.data());
        done += n;
    }
}

uint32_t
EmbeddedBuffer::write(MemPort &req, uint64_t addr, const uint8_t *data,
                      uint32_t len, const uint8_t *be, uint32_t be_len)
{
    SpmStats &s = req.spmStats();
    uint32_t hit_beats = 0;
    uint32_t done = 0;
    WriteBacks wbs;
    while (done < len) {
        const uint64_t a = addr + done;
        const uint64_t la = lineAddr(a);
        const uint32_t off = a - la;
        const uint32_t n = std::min<uint32_t>(len - done, lineSz - off);

        if (_mode == BUF_MODE_ALL) {
            Line *l = find(la);
            if (l) {
                s.write_hits++;
                hit_beats++;
            } else {
                // write miss: the beat covers part of the line, so fetch the
                // line first (write allocate)
                s.write_misses++;
                bool merged = false;
                l = demandFill(req, la, merged, wbs);
            }
            l->lru = ++lruClock;
            for (uint32_t i = 0; i < n; i++) {
                if (!byteEnabled(be, be_len, done + i))
                    continue;
                l->data[off + i] = data[done + i];
                l->dmask[off + i] = 1;
                l->dirty = true;
            }
            writeBack(req, la, wbs);
        } else {
            // prefetch buffer modes: writes go to memory (only the enabled
            // bytes, one write per contiguous run)
            s.write_bypass++;
            updateCopies(a, data + done, n, be, be_len, done);
            uint32_t i = 0;
            while (i < n) {
                if (!byteEnabled(be, be_len, done + i)) {
                    i++;
                    continue;
                }
                uint32_t start = i;
                while (i < n && byteEnabled(be, be_len, done + i))
                    i++;
                req.memWrite(a + start, data + done + start, i - start);
            }
        }
        done += n;
    }
    return hit_beats;
}

void
EmbeddedBuffer::flush(MemPort &req)
{
    SpmStats &s = req.spmStats();
    bool any = false;
    flushGen++;
    WriteBacks wbs;
    for (auto &l : lines) {
        if (!l.valid)
            continue;
        any = true;
        if (l.dirty) {
            const size_t before = wbs.size();
            collectDirty(l, l.tag, wbs);
            for (size_t k = before; k < wbs.size(); k++)
                s.flush_writeback_bytes += wbs[k].second.size();
        }
        l.valid = false;
    }
    for (auto &rb : readBufs) {
        any = any || !rb.lines.empty();
        rb.lines.clear();
    }
    if (any)
        s.flushes++;
    writeBack(req, 0, wbs);   // blocks until memory has every byte
    // memory is current now; the CPU may change it before the next task
    overlay.clear();
}

bool
EmbeddedBuffer::prefetchLine(MemPort &req, uint64_t line_addr)
{
    if (find(line_addr) || inflight.count(line_addr))
        return false;
    req.spmStats().prefetch_issued++;
    WriteBacks wbs;
    Line *l = fetchAndInstall(req, line_addr, wbs);
    writeBack(req, line_addr, wbs);
    return l != nullptr;
}

} // namespace sc_nvdla_spm
