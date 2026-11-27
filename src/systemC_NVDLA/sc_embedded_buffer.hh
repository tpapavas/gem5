/*
 * Embedded buffer (SPM) of the SystemC NVDLA, with the buffer modes and the
 * weight prefetcher of the Verilator NVDLA (ext/rtl/model_nvdla/embeddedBuffer
 * and AXIResponder). It sits between the CMOD DBB AXI port and the gem5
 * memory (ScNvDlaSE::b_transport_dbb) and works on TLM transactions of any
 * size, so the same code serves nv_full (64 B beats) and nv_small (8 B beats
 * with byte enables).
 *
 * Modes (--buffer-mode, as the Verilator NVDLA):
 *  - all (0):     all DBB reads and writes go through the buffer
 *                 (write-allocate, LRU, write-back of the dirty bytes only).
 *  - pft (1):     the buffer holds only prefetched weight lines. A read that
 *                 finds its line moves it to a per-NVDLA read buffer (the line
 *                 leaves the buffer); other reads and all writes go straight
 *                 to memory.
 *  - pft-cut (2): as pft, but a prefetch is issued only while the set of the
 *                 line has room (no eviction of prefetched lines).
 *
 * Memory accesses (MemPort) block the calling SystemC thread for the memory
 * time (the TLM-to-gem5 bridge of a timing-mode system serves b_transport
 * through nb_transport and waits). So a line can be in flight while another
 * thread (the other NVDLA of a shared buffer, or the prefetcher) looks for
 * it: in-flight fills are tracked, and an access to such a line waits for
 * it. Dirty bytes of an evicted line are kept in an overlay until their
 * write-back completes, so a fill never returns older data than the buffer
 * had (as the shared buffer of the Verilator model).
 */

#ifndef __SYSTEMC_NVDLA_SC_EMBEDDED_BUFFER_HH__
#define __SYSTEMC_NVDLA_SC_EMBEDDED_BUFFER_HH__

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "systemc.h"

namespace sc_nvdla_spm
{

enum BufMode
{
    BUF_MODE_ALL = 0,
    BUF_MODE_PFT = 1,
    BUF_MODE_PFT_CUTOFF = 2,
};

// Same counters (names and meaning) as the Verilator NVDLA
// (ext/rtl/model_nvdla/spmStats.hh). Here a "beat" is one DBB TLM
// transaction, or the part of it inside one buffer line.
#define SC_NVDLA_SPM_STATS(X) \
    X(read_hits, \
      "DBB read beats served from the embedded buffer") \
    X(read_misses, \
      "DBB read beats not in the embedded buffer") \
    X(read_miss_merged, \
      "Read misses on a line whose fill was already in flight") \
    X(write_hits, \
      "DBB write beats written into the buffer without a miss") \
    X(write_misses, \
      "DBB write beats that missed (line fetched first, write allocate)") \
    X(write_bypass, \
      "DBB write beats sent straight to memory (prefetch buffer modes)") \
    X(line_fills, \
      "Lines brought into the embedded buffer from memory") \
    X(fills_dropped, \
      "Lines from memory not kept (set full or line already there)") \
    X(evictions, \
      "Valid lines evicted to make room") \
    X(dirty_evictions, \
      "Evicted lines that had dirty bytes (written back)") \
    X(writeback_bytes, \
      "Dirty bytes written back to memory on eviction") \
    X(flushes, \
      "Flushes of a non-empty embedded buffer (end of an NVDLA task)") \
    X(flush_writeback_bytes, \
      "Dirty bytes written back to memory by flushes") \
    X(prefetch_issued, \
      "Lines requested by the weight prefetcher") \
    X(prefetch_used, \
      "Prefetched lines consumed by NVDLA reads (prefetch buffer modes)") \
    X(pft_log_entries, \
      "Weight tensors added to the prefetch log (CDMA op enables)") \
    X(mem_reads, \
      "Reads sent to memory by the buffer (fill chunks, bypass reads)") \
    X(mem_read_bytes, \
      "Bytes read from memory by the buffer") \
    X(mem_writes, \
      "Writes sent to memory by the buffer (write-backs, bypass writes)") \
    X(mem_write_bytes, \
      "Bytes written to memory by the buffer")

struct SpmStats
{
#define SC_NVDLA_SPM_STATS_FIELD(name, desc) uint64_t name = 0;
    SC_NVDLA_SPM_STATS(SC_NVDLA_SPM_STATS_FIELD)
#undef SC_NVDLA_SPM_STATS_FIELD

    void dump(std::ostream &os) const;
};

// Memory below the buffer, seen from one NVDLA. The calls block the calling
// SystemC thread until memory answers. spmStats() is where the events caused
// by this NVDLA are counted (with a shared buffer every event is charged to
// the NVDLA whose access caused it, as in the Verilator model).
class MemPort
{
  public:
    virtual ~MemPort() = default;
    virtual void memRead(uint64_t addr, uint8_t *data, uint32_t len) = 0;
    virtual void memWrite(uint64_t addr, const uint8_t *data,
                          uint32_t len) = 0;
    // Read [addr, addr+len) as requests of chunk bytes (gem5 cache lines,
    // so the CPU caches are snooped line by line).
    virtual void memReadChunks(uint64_t addr, uint8_t *data, uint32_t len,
                               uint32_t chunk) = 0;
    virtual SpmStats &spmStats() = 0;
    virtual unsigned spmId() const = 0;
};

class EmbeddedBuffer
{
  public:
    EmbeddedBuffer(BufMode mode, uint32_t line_size, uint32_t num_lines,
                   uint32_t assoc, uint32_t fill_chunk);

    BufMode mode() const { return _mode; }
    uint32_t lineSize() const { return lineSz; }

    // Demand accesses of one DBB transaction (they may block). be = TLM
    // byte enables (nullptr or be_len == 0: all bytes enabled). Returns the
    // number of beats served by the buffer (the caller charges the buffer
    // latency for them).
    uint32_t read(MemPort &req, uint64_t addr, uint8_t *data, uint32_t len);
    uint32_t write(MemPort &req, uint64_t addr, const uint8_t *data,
                   uint32_t len, const uint8_t *be, uint32_t be_len);

    // Write back every dirty byte and empty the buffer (end of an NVDLA
    // task). Fills in flight when the flush starts are dropped.
    void flush(MemPort &req);

    // Prefetcher support
    bool hasLine(uint64_t line_addr) const;
    bool inFlight(uint64_t line_addr) const;
    // pft-cut: true if the set of line_addr has no free entry
    bool setFull(uint64_t line_addr) const;
    // Fetch one line into the buffer (blocks). Returns false if dropped.
    bool prefetchLine(MemPort &req, uint64_t line_addr);

  private:
    struct Line
    {
        uint64_t tag = 0;
        bool valid = false;
        bool dirty = false;
        uint64_t lru = 0;
        std::vector<uint8_t> data;
        std::vector<uint8_t> dmask;   // 1 = byte written by NVDLA since fill
    };

    // Lines a read took out of the buffer in the prefetch buffer modes,
    // per NVDLA (Verilator: one per NVDLA and AXI stream; here the TLM
    // transactions carry no stream id, so a few lines per NVDLA).
    struct ReadBuf
    {
        unsigned id;
        std::deque<std::pair<uint64_t, std::vector<uint8_t>>> lines;
    };
    static const size_t READ_BUF_LINES = 16;

    // Dirty bytes of evicted lines whose write-back is not complete yet
    struct Overlay
    {
        std::vector<uint8_t> data;
        std::vector<uint8_t> mask;
        unsigned pending = 0;   // write-backs in progress for this line
    };

    const BufMode _mode;
    const uint32_t lineSz;
    const uint32_t assoc;
    const uint32_t numSets;
    const uint32_t fillChunk;   // fills/write-backs in chunks of this size
    std::vector<Line> lines;    // numSets * assoc
    uint64_t lruClock = 0;
    uint64_t flushGen = 0;
    std::vector<ReadBuf> readBufs;
    std::map<uint64_t, std::shared_ptr<sc_core::sc_event>> inflight;
    std::map<uint64_t, Overlay> overlay;

    uint64_t
    lineAddr(uint64_t addr) const
    {
        return addr & ~(uint64_t)(lineSz - 1);
    }
    uint32_t
    setOf(uint64_t line_addr) const
    {
        return (line_addr / lineSz) % numSets;
    }
    Line *find(uint64_t line_addr);
    const Line *find(uint64_t line_addr) const;
    // Bring line_addr into the buffer for a demand access (waits for a fill
    // already in flight, fetches again if a flush dropped it). The dirty
    // bytes of an evicted line are appended to wbs: the caller writes them
    // back (writeBack) after it has used the returned line.
    Line *demandFill(MemPort &req, uint64_t line_addr, bool &merged,
                     std::vector<std::pair<uint64_t, std::vector<uint8_t>>>
                         &wbs);
    // Fetch the line from memory (blocks) and install it, evicting the LRU
    // line. Returns the line (or the copy already there), or nullptr if not
    // installed (pft-cut set full, or dropped by a flush).
    Line *fetchAndInstall(MemPort &req, uint64_t line_addr,
                          std::vector<std::pair<uint64_t,
                                                std::vector<uint8_t>>> &wbs);
    Line *allocate(MemPort &req, uint64_t line_addr,
                   std::vector<std::pair<uint64_t, std::vector<uint8_t>>>
                       &wbs);
    void collectDirty(Line &l, uint64_t base,
                      std::vector<std::pair<uint64_t, std::vector<uint8_t>>>
                          &wbs);
    void writeBack(MemPort &req, uint64_t line_addr,
                   std::vector<std::pair<uint64_t, std::vector<uint8_t>>>
                       &wbs);
    ReadBuf &readBufOf(unsigned id);
    // Keep every copy of the written bytes current: the prefetch buffer
    // modes write to memory, but a line of the same address may sit in the
    // buffer or in a read buffer.
    void updateCopies(uint64_t addr, const uint8_t *data, uint32_t len,
                      const uint8_t *be, uint32_t be_len, uint32_t be_off);
};

} // namespace sc_nvdla_spm

#endif // __SYSTEMC_NVDLA_SC_EMBEDDED_BUFFER_HH__
