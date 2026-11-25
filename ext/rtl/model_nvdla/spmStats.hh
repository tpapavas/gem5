#ifndef GEM5_NVDLA_SPMSTATS_HH
#define GEM5_NVDLA_SPMSTATS_HH

#include <cstdint>

// [SPM_STATS] Counters of the embedded buffer (SPM), kept per NVDLA. With a shared buffer every
// event is charged to the NVDLA whose access caused it. rtlNVDLA copies them into gem5 stats
// (<nvdla>.spm.<name> in stats.txt, src/dev/nvdla_spm_stats.hh). A "beat" is one AXI_WIDTH/8 = 64 B AXI data beat.
//
// X(name, description)
#define NVDLA_SPM_STATS(X) \
    X(read_hits,            "AXI read beats served from the embedded buffer") \
    X(read_misses,          "AXI read beats not in the embedded buffer (served by a DMA line read)") \
    X(read_miss_merged,     "Read misses on a line whose DMA read was already in flight (no new DMA)") \
    X(write_hits,           "AXI write beats written into the embedded buffer without a miss") \
    X(write_misses,         "AXI write beats that missed and waited for a line fill (write allocate)") \
    X(write_bypass,         "AXI write beats sent straight to memory (prefetch buffer modes)") \
    X(line_fills,           "Lines brought into the embedded buffer from memory") \
    X(fills_dropped,        "Lines from memory not kept (prefetch buffer full or line already there)") \
    X(evictions,            "Valid lines evicted to make room") \
    X(dirty_evictions,      "Evicted lines that had dirty bytes (written back)") \
    X(writeback_bytes,      "Dirty bytes written back to memory on eviction") \
    X(flushes,              "Flushes of a non-empty embedded buffer (end of an NVDLA task)") \
    X(flush_writeback_bytes,"Dirty bytes written back to memory by flushes") \
    X(prefetch_issued,      "Lines requested by the software prefetcher (DMA reads)") \
    X(prefetch_used,        "Prefetched lines consumed by NVDLA reads (prefetch buffer modes)")

struct SpmStats {
#define NVDLA_SPM_STATS_FIELD(name, desc) uint64_t name = 0;
    NVDLA_SPM_STATS(NVDLA_SPM_STATS_FIELD)
#undef NVDLA_SPM_STATS_FIELD
};

#endif //GEM5_NVDLA_SPMSTATS_HH
