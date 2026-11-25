#include <algorithm>   // [DIRTY_MASK] std::fill
#include "embeddedBuffer.hh"


abstractSet::abstractSet(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _assoc) :
        wrapper(wrap), spm_latency(_lat), spm_line_size(_line_size), assoc(_assoc) {
    addr_map.reserve(assoc);
}


abstractSet::~abstractSet() = default;


allBufferSet::allBufferSet(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _assoc) :
        abstractSet(wrap, _lat, _line_size, _assoc), shared(wrap->use_shared_spm) {
    for (uint32_t i = 0; i < assoc; i++) {
        lru_order.emplace_back(i);
    }

    lines.reserve(assoc);
    for (auto it = lru_order.begin(); it != lru_order.end(); it++) {
        lines.emplace_back(spm_line_size, it, addr_map.end());
    }
}


bool allBufferSet::read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out) {
    assert((axi_addr & (uint64_t)(AXI_WIDTH / 8 - 1)) == 0);

    uint64_t addr_base = axi_addr & ~(uint64_t)(spm_line_size - 1);
    uint64_t offset = axi_addr & (uint64_t)(spm_line_size - 1);

    auto addr_map_it = addr_map.find(addr_base);
    if (addr_map_it == addr_map.end())
        return false;

    auto& entry = lines[addr_map_it->second];
#ifndef NO_DATA
    for (int i = 0; i < AXI_WIDTH / 8; i++)
        data_out[i] = entry.spm_line[offset + i];
#endif
    lru_order.splice(lru_order.end(), lru_order, entry.lru_it);
    return true;
}


bool allBufferSet::read_spm_line(uint64_t aligned_addr, std::vector<uint8_t>& data_out) {
    assert((aligned_addr & (uint64_t)(spm_line_size - 1)) == 0);

    auto addr_map_it = addr_map.find(aligned_addr);
    if (addr_map_it == addr_map.end()) {
        return false;
    }
    auto& entry = lines[addr_map_it->second];
    entry.valid = 0;
    auto& line = entry.spm_line;
    data_out.assign(line.begin(), line.end());
    addr_map.erase(addr_map_it);
    entry.map_it = addr_map.end();
    // leave the lru_it there. It will be managed once new data is written to this line
    // this function is called in a prefetchBuffer. So leave dirty bit untouched since it should always be 0.
    return true;
}


void allBufferSet::write_spm_axi_line_with_mask(uint64_t axi_addr, const uint8_t* data, uint64_t mask,
                                                Wrapper_nvdla* req) {
    assert((axi_addr & (uint64_t)(AXI_WIDTH / 8 - 1)) == 0);

    uint64_t addr_base = axi_addr & ~(uint64_t)(spm_line_size - 1);
    uint64_t offset = axi_addr & (uint64_t)(spm_line_size - 1);

    auto addr_map_it = addr_map.find(addr_base);
    if (addr_map_it == addr_map.end()) {
        uint32_t to_write_vec_id = assoc;
        if (addr_map.size() >= assoc) {
            to_write_vec_id = erase_victim(req);   // lru_order maintenance of erasing (moving to lru back) is done inside
        } else {
            // find a currently invalid entry to write in
            for (uint32_t i = 0; i < lines.size(); i++) {
                if (!lines[i].valid) {
                    to_write_vec_id = i;
                    lru_order.splice(lru_order.end(), lru_order, lines[i].lru_it);
                    break;
                }
            }
        }
        assert(to_write_vec_id < assoc);
        // assign mapping entry to this new line
        bool alloc_succ;
        std::tie(addr_map_it, alloc_succ) = addr_map.emplace(addr_base, to_write_vec_id);

        auto& entry = lines[addr_map_it->second];
        entry.map_it = addr_map_it;
        entry.valid = 1;
        std::fill(entry.dirty_mask.begin(), entry.dirty_mask.end(), 0);  // [DIRTY_MASK]
    }
    auto& entry = lines[addr_map_it->second];
    entry.dirty = 1;
    // [DIRTY_MASK] remember exactly which bytes this beat wrote
    for (int i = 0; i < AXI_WIDTH / 8; i++) {
        if ((mask >> i) & 1)
            entry.dirty_mask[offset + i] = 1;
    }
#ifndef NO_DATA
    if (mask == 0xFFFFFFFFFFFFFFFF) {
        for (int i = 0; i < AXI_WIDTH / 8; i++) {
            entry.spm_line[offset + i] = data[i];
        }
    } else {
        for (int i = 0; i < AXI_WIDTH / 8; i++) {
            if (!((mask >> i) & 1))
                continue;
            entry.spm_line[offset + i] = data[i];
        }
    }
#endif
}


// [DIRTY_MASK] Write back only the bytes NVDLA wrote. The old code wrote the whole 1 KiB line,
// so bytes this NVDLA never wrote (stale copies of what another NVDLA wrote meanwhile into the
// same line) overwrote memory. With 2 DLAs and batch 2 the two outputs share one line
// (e.g. 0x2001d800 / 0x2001d820), and the later flush erased the other batch.
void allBufferSet::write_back_dirty_bytes(allBufferLineWithTag& entry, Wrapper_nvdla* req, bool keep_in_overlay) {
    const uint64_t base = entry.map_it->first;
    Wrapper_nvdla* const out = requester(req);   // [SHARED_SPM]
    if (keep_in_overlay) {
        auto& ov = wb_overlay[base];
        if (ov.data.empty()) {
            ov.data.assign(spm_line_size, 0);
            ov.mask.assign(spm_line_size, 0);
        }
        for (uint32_t b = 0; b < spm_line_size; b++) {
            if (entry.dirty_mask[b]) {
                ov.data[b] = entry.spm_line[b];
                ov.mask[b] = 1;
            }
        }
    }
    uint32_t i = 0;
    while (i < spm_line_size) {
        if (!entry.dirty_mask[i]) {
            i++;
            continue;
        }
        uint32_t start = i;
        while (i < spm_line_size && entry.dirty_mask[i])
            i++;
        out->addDMAWriteReq(base + start,
            std::vector<uint8_t>(entry.spm_line.begin() + start, entry.spm_line.begin() + i));
    }
    std::fill(entry.dirty_mask.begin(), entry.dirty_mask.end(), 0);
    entry.dirty = 0;
}


void allBufferSet::clear_and_write_back_dirty(Wrapper_nvdla* req) {
    for (auto& line: lines) {
        if (line.dirty) {
            write_back_dirty_bytes(line, req, false);   // [DIRTY_MASK] was: addDMAWriteReq(addr, whole line)
        }
        line.map_it = addr_map.end();
        line.valid = 0;
        // don't clear lru_order and the vector itself because they may be used for data coming in afterward
    }
    addr_map.clear();
    // [SHARED_SPM] after the flush memory has every byte; the CPU may change it before the next task
    wb_overlay.clear();
}


void allBufferSet::fill_spm_line(uint64_t aligned_addr, const uint8_t* data, Wrapper_nvdla* req) {
    assert((aligned_addr & (uint64_t)(spm_line_size - 1)) == 0);

    auto addr_map_it = addr_map.find(aligned_addr);
    if (addr_map_it == addr_map.end()) {
        uint32_t to_write_vec_id = assoc;
        if (addr_map.size() >= assoc) {
            to_write_vec_id = erase_victim(req);       // lru_order maintenance of erasing is done inside
        } else {
            // find a currently invalid entry to write in
            for (uint32_t i = 0; i < lines.size(); i++) {
                if (!lines[i].valid) {
                    to_write_vec_id = i;
                    lru_order.splice(lru_order.end(), lru_order, lines[i].lru_it);
                    break;
                }
            }
        }
        assert(to_write_vec_id < assoc);
        // assign mapping entry to this new line
        bool alloc_succ;
        std::tie(addr_map_it, alloc_succ) = addr_map.emplace(aligned_addr, to_write_vec_id);
        auto& entry = lines[to_write_vec_id];
        entry.map_it = addr_map_it;
        entry.valid = 1;
        entry.dirty = 0;
        std::fill(entry.dirty_mask.begin(), entry.dirty_mask.end(), 0);  // [DIRTY_MASK]
#ifndef NO_DATA
        entry.spm_line.assign(data, data + spm_line_size);
        // [SHARED_SPM] bytes written back on eviction may not be in memory yet: they win over memory
        auto ov_it = wb_overlay.find(aligned_addr);
        if (ov_it != wb_overlay.end()) {
            for (uint32_t b = 0; b < spm_line_size; b++) {
                if (ov_it->second.mask[b])
                    entry.spm_line[b] = ov_it->second.data[b];
            }
        }
#endif
    } else if (!shared) {
        printf("(%lu) Weird: request the DRAM when it hits the embedded buffer.\n", wrapper->tickcount);
    }
    // [SHARED_SPM] else: the other NVDLA brought (and maybe changed) this line while our DMA was in
    // flight. Keep the buffer copy; the waiting reads are served from it (AXIResponder::inflight_dma_resp).
}


uint32_t allBufferSet::erase_victim(Wrapper_nvdla* req) {
    auto to_erase_id = lru_order.front();
    auto& entry = lines[to_erase_id];
    assert(entry.valid);
    if (entry.dirty) {
        write_back_dirty_bytes(entry, req, shared);   // [DIRTY_MASK] was: addDMAWriteReq(addr, whole line)
    }
    addr_map.erase(entry.map_it);
    entry.valid = 0;
    lru_order.splice(lru_order.end(), lru_order, entry.lru_it);
    return to_erase_id;
}


prefetchThrottleSet::prefetchThrottleSet(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _assoc):
        abstractSet(wrap, _lat, _line_size, _assoc),
        lines(_assoc, prefetchThrottleLineWithTag(_line_size)) {

}


bool prefetchThrottleSet::read_spm_line(uint64_t aligned_addr, std::vector<uint8_t>& data_out) {
    assert((aligned_addr & (uint64_t)(spm_line_size - 1)) == 0);

    auto addr_map_it = addr_map.find(aligned_addr);
    if (addr_map_it == addr_map.end()) {
        return false;
    }
    auto& entry = lines[addr_map_it->second];
    entry.valid = 0;
    auto& line = entry.spm_line;
    data_out.assign(line.begin(), line.end());
    addr_map.erase(addr_map_it);
    return true;
}


void prefetchThrottleSet::clear_and_write_back_dirty(Wrapper_nvdla* req) {
    for (auto& line: lines) {
        line.valid = 0;
        // don't clear lru_order and the vector itself because they may be used for data coming in afterward
    }
    addr_map.clear();
}


void prefetchThrottleSet::fill_spm_line(uint64_t aligned_addr, const uint8_t* data, Wrapper_nvdla* req) {
    assert((aligned_addr & (uint64_t)(spm_line_size - 1)) == 0);
    // [BUF_MODE] (the assert below is compiled out with -DNDEBUG) the throttle counts only the
    // prefetches in flight of one NVDLA, so with a shared buffer the set can be full here.
    // Drop the line instead of writing past the end of `lines`.
    if (addr_map.size() >= assoc)
        return;
    assert(addr_map.size() < assoc);

    // some bias data are found to be accessed twice. We simply ignore them.
    // since biases make up only a small proportion, this phenomenon has little
    // impact on overall performance
    if (addr_map.find(aligned_addr) != addr_map.end()) {
        return;
    }

    uint32_t to_write_vec_id = assoc;
    for (uint32_t i = 0; i < lines.size(); i++) {
        if (!lines[i].valid) {
            to_write_vec_id = i;
            break;
        }
    }
    if (to_write_vec_id >= assoc)   // [BUF_MODE] no invalid entry (see above): drop
        return;
    addr_map.emplace(aligned_addr, to_write_vec_id);
    auto& entry = lines[to_write_vec_id];
    entry.valid = 1;
#ifndef NO_DATA
    entry.spm_line.assign(data, data + spm_line_size);
#endif
}


embeddedBuffer::embeddedBuffer(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _line_num, uint32_t _assoc) :
        wrapper(wrap), spm_latency(_lat), spm_line_size(_line_size), spm_line_num(_line_num), assoc(_assoc),
        num_sets(_line_num / _assoc) {
    sets.reserve(_line_num / _assoc);
}


allBuffer::allBuffer(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _line_num, uint32_t _assoc) :
        embeddedBuffer(wrap, _lat, _line_size, _line_num, _assoc) {
    for (uint32_t set_id = 0; set_id < num_sets; set_id++) {
        sets.emplace_back(new allBufferSet(wrap, _lat, _line_size, _assoc));
    }
}


allBuffer::~allBuffer() {
    for (auto& set_ptr: sets) {
        delete set_ptr;
    }
}


bool allBuffer::read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out, uint8_t stream_id,
                                  Wrapper_nvdla* req) {
    uint32_t set_id = (axi_addr / spm_line_size) % num_sets;
    return sets[set_id]->read_spm_axi_line(axi_addr, data_out);
}


void allBuffer::write_spm_axi_line_with_mask(uint64_t axi_addr, const uint8_t* data, uint64_t mask, uint8_t stream,
                                             Wrapper_nvdla* req) {
    uint32_t set_id = (axi_addr / spm_line_size) % num_sets;
    sets[set_id]->write_spm_axi_line_with_mask(axi_addr, data, mask, req);
}


prefetchBuffer::prefetchBuffer(Wrapper_nvdla* wrap, uint32_t _lat, uint32_t _line_size, uint32_t _line_num, uint32_t _assoc) :
        embeddedBuffer(wrap, _lat, _line_size, _line_num, _assoc) {
    switch (wrap->buf_mode) {
        case BUF_MODE_PFT:
            for (uint32_t set_id = 0; set_id < num_sets; set_id++) {
                sets.emplace_back(new allBufferSet(wrap, _lat, _line_size, _assoc));
            }
            break;
        case BUF_MODE_PFT_CUTOFF:
            for (uint32_t set_id = 0; set_id < num_sets; set_id++) {
                sets.emplace_back(new prefetchThrottleSet(wrap, _lat, _line_size, _assoc));
            }
            break;
        case BUF_MODE_ALL:
        default:
            assert(false);
    }
}


prefetchBuffer::~prefetchBuffer() {
    for (auto& set_ptr: sets) {
        delete set_ptr;
    }
}


bool prefetchBuffer::read_spm_axi_line(uint64_t axi_addr, uint8_t* data_out, uint8_t stream_id,
                                       Wrapper_nvdla* req) {
    uint32_t set_id = (axi_addr / spm_line_size) % num_sets;
    uint64_t addr_base = axi_addr & ~(uint64_t)(spm_line_size - 1);
    // [BUF_MODE] read buffer of this NVDLA and stream
    const int nv = (req ? req->id_nvdla : wrapper->id_nvdla) % MAX_RB_NVDLA;
    auto& rb = read_buffers[nv][stream_id % MAX_RB_STREAMS];
    if (addr_base != rb.first) {
        bool found = sets[set_id]->read_spm_line(addr_base, rb.second);
        if (!found) {
            return false;
        }
        rb.first = addr_base;
    }
    // always in read buffer here
#ifndef NO_DATA
    uint64_t offset = axi_addr & (uint64_t)(spm_line_size - 1);
    std::vector<uint8_t> &entry_vector = rb.second;
    for (int i = 0; i < AXI_WIDTH / 8; i++)
        data_out[i] = entry_vector[offset + i];
#endif
    return true;
}


uint32_t prefetchBuffer::num_valid(uint64_t try_addr) {
    uint32_t set_id = (try_addr / spm_line_size) % num_sets;
    return sets[set_id]->size();
};
