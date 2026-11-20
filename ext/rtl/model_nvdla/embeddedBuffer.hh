#ifndef GEM5_NVDLA_EMBEDDEDBUFFER_HH
#define GEM5_NVDLA_EMBEDDEDBUFFER_HH

#include <queue>
#include <map>
#include <unordered_map>
#include <vector>

#include "wrapper_nvdla.hh"


class abstractSet {
protected:
    Wrapper_nvdla* const wrapper;
    const uint32_t spm_latency;
    const uint32_t spm_line_size;
    const uint32_t assoc;

    std::unordered_map<uint64_t, uint32_t> addr_map;
    // leave the vector to subclasses
public:
    abstractSet(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _assoc);
    virtual ~abstractSet() = 0;
    inline size_t size() { return addr_map.size(); }
    inline virtual bool read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out) { assert(false); return true; }
    inline virtual bool read_spm_line(uint64_t aligned_addr, std::vector<uint8_t>& data_out) { assert(false); return true; }
    inline bool check_has_line(uint64_t aligned_addr) {
        assert((aligned_addr & (uint64_t)(spm_line_size - 1)) == 0);
        return addr_map.find(aligned_addr) != addr_map.end();
    }
    // [SHARED_SPM] req = the NVDLA whose access causes this call. Write-backs (eviction or flush)
    // are issued on req's DMA engine. nullptr = the NVDLA that created the buffer (old behaviour).
    inline virtual void write_spm_axi_line_with_mask(uint64_t axi_addr, const uint8_t* data, uint64_t mask,
                                                     Wrapper_nvdla* req) { assert(false); }
    inline virtual void clear_and_write_back_dirty(Wrapper_nvdla* req) { assert(false); }
    virtual void fill_spm_line(uint64_t aligned_addr, const uint8_t* data, Wrapper_nvdla* req) = 0;
    inline Wrapper_nvdla* requester(Wrapper_nvdla* req) const { return req ? req : wrapper; }
};


class allBufferSet: virtual public abstractSet {
protected:
    struct allBufferLineWithTag {
        std::vector<uint8_t> spm_line;
        std::list<uint32_t>::iterator lru_it;
        std::unordered_map<uint64_t, uint32_t>::iterator map_it;
        uint8_t dirty;
        uint8_t valid;
        // [DIRTY_MASK] one flag per byte: 1 = written by this NVDLA since the line came in.
        // Only these bytes are written back, so two NVDLAs (or NVDLA and CPU) that write
        // different bytes of the same line do not overwrite each other on write-back.
        std::vector<uint8_t> dirty_mask;

        allBufferLineWithTag(uint32_t spm_line_size, std::list<uint32_t>::iterator _lru_it,
                             std::unordered_map<uint64_t, uint32_t>::iterator _map_it):
                spm_line(spm_line_size, 0), lru_it(_lru_it), map_it(_map_it), dirty(0), valid(0),
                dirty_mask(spm_line_size, 0) {}
    };
    std::vector<allBufferLineWithTag> lines;
    std::list<uint32_t> lru_order;

    // [DIRTY_MASK] issue one DMA write per contiguous run of dirty bytes, then clear the mask
    // [SHARED_SPM] on req's DMA engine; on eviction of a shared buffer also keep the bytes in wb_overlay
    void write_back_dirty_bytes(allBufferLineWithTag& entry, Wrapper_nvdla* req, bool keep_in_overlay);

    // [SHARED_SPM] Bytes written back on eviction of a shared buffer, kept until the next flush.
    // The DMA reads and DMA writes of an NVDLA are separate engines, and with a shared buffer the
    // write-back of a line can sit in the queue of one NVDLA while the other NVDLA misses on the
    // same line and reads it from memory. Every fill of a line applies these bytes on top of the
    // data that came from memory, so a fill never returns data older than the buffer had.
    struct wbOverlayLine {
        std::vector<uint8_t> data;
        std::vector<uint8_t> mask;
    };
    std::unordered_map<uint64_t, wbOverlayLine> wb_overlay;
    const bool shared;

public:
    allBufferSet(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _assoc);
    ~allBufferSet() override = default;
    bool read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out) override;
    bool read_spm_line(uint64_t aligned_addr, std::vector<uint8_t>& data_out) override;
    void write_spm_axi_line_with_mask(uint64_t axi_addr, const uint8_t* data, uint64_t mask,
                                      Wrapper_nvdla* req) override;
    void clear_and_write_back_dirty(Wrapper_nvdla* req) override;
    void fill_spm_line(uint64_t aligned_addr, const uint8_t* data, Wrapper_nvdla* req) override;
    uint32_t erase_victim(Wrapper_nvdla* req);
};


class prefetchThrottleSet: virtual public abstractSet {
protected:
    struct prefetchThrottleLineWithTag {
        std::vector<uint8_t> spm_line;
        uint8_t valid;

        explicit prefetchThrottleLineWithTag(uint32_t spm_line_size) : spm_line(spm_line_size, 0), valid(0) {}
    };
    std::vector<prefetchThrottleLineWithTag> lines;

public:
    prefetchThrottleSet(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _assoc);
    ~prefetchThrottleSet() override = default;
    bool read_spm_line(uint64_t aligned_addr, std::vector<uint8_t>& data_out) override;
    void clear_and_write_back_dirty(Wrapper_nvdla* req) override;
    void fill_spm_line(uint64_t aligned_addr, const uint8_t* data, Wrapper_nvdla* req) override;
};


class embeddedBuffer {
protected:
    Wrapper_nvdla* const wrapper;
    const uint32_t spm_line_num;
    std::vector<abstractSet*> sets;

public:
    const uint32_t spm_latency;
    const uint32_t spm_line_size;   // all the sizes are in bytes
    const uint32_t assoc;
    const uint32_t num_sets;

    embeddedBuffer(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _line_num, uint32_t _assoc);
    virtual ~embeddedBuffer() = default;
    // [BUF_MODE] req = the reading NVDLA (prefetchBuffer keeps one read buffer per NVDLA and stream)
    virtual bool read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out, uint8_t stream_id,
                                   Wrapper_nvdla* req = nullptr) = 0;
    inline bool check_has_line(uint64_t aligned_addr) {
        uint32_t set_id = (aligned_addr / spm_line_size) % num_sets;
        return sets[set_id]->check_has_line(aligned_addr);
    }

    inline virtual void write_spm_axi_line_with_mask(uint64_t axi_addr, const uint8_t* data, uint64_t mask, uint8_t stream,
                                                     Wrapper_nvdla* req = nullptr) {
        assert(false);
    }

    // [SHARED_SPM] req: see abstractSet. With a shared buffer the NVDLA that flushes writes back
    // the dirty bytes of both NVDLAs, on its own DMA engine, and waits for them.
    inline virtual void clear_and_write_back_dirty(Wrapper_nvdla* req = nullptr) {
        for (auto& set: sets) {
            set->clear_and_write_back_dirty(req);
        }
    }

    inline void fill_spm_line(uint64_t aligned_addr, const uint8_t* data, Wrapper_nvdla* req = nullptr) {
        uint32_t set_id = (aligned_addr / spm_line_size) % num_sets;
        sets[set_id]->fill_spm_line(aligned_addr, data, req);
    }
};


class allBuffer: virtual public embeddedBuffer {
public:
    allBuffer(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _line_num, uint32_t _assoc);
    ~allBuffer() override;
    bool read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out, uint8_t stream_id,
                           Wrapper_nvdla* req = nullptr) override;
    void write_spm_axi_line_with_mask(uint64_t axi_addr, const uint8_t* data, uint64_t mask, uint8_t stream,
                                      Wrapper_nvdla* req = nullptr) override;
};


class prefetchBuffer: virtual public embeddedBuffer {
private:
    //! buffers for each stream (used only for reading)
    // [BUF_MODE] one set per NVDLA: with a shared buffer the AXI stream ids of the two NVDLAs overlap
    static const int MAX_RB_NVDLA = 4;
    static const int MAX_RB_STREAMS = 16;
    std::pair<uint64_t, std::vector<uint8_t> > read_buffers[MAX_RB_NVDLA][MAX_RB_STREAMS];

public:
    prefetchBuffer(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _line_num, uint32_t _assoc);
    ~prefetchBuffer() override;
    bool read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out, uint8_t stream_id,
                           Wrapper_nvdla* req = nullptr) override;
    uint32_t num_valid(uint64_t try_addr);
};

#endif //GEM5_NVDLA_EMBEDDEDBUFFER_HH
