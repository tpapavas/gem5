/*
 * Copyright (c) 2013,2016,2018-2019 ARM Limited
 * All rights reserved.
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
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
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
 */

/**
 * @file
 * Definitions of BaseTags.
 */

#include "mem/cache/tags/base.hh"

#include <cassert>

#include "base/types.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/request.hh"
#include "sim/core.hh"
#include "sim/sim_exit.hh"
#include "sim/system.hh"

namespace gem5
{
    // [RZ CODE]
    // static std::set<Addr> disabledBlockKeys;
    // static std::set<std::pair<int, int>> disabledSetandWay;
    // static const std::vector<std::pair<unsigned, unsigned>> disabledBlocks = {
    //         {0, 0},
    //         {0, 1},
    //         {0, 2},
    //         {2, 0},
    //         {2, 1},
    //         {4, 3},
    //         {6, 0},
    //         {3, 1},
    //         {4, 1},
    //         {4, 2},
    //         {6, 1},
    //         {1, 0},
    //         {3, 2},
    //         {3, 3},
    //         {5, 0},
    //         {5, 1},
    //         {5, 2},
    //         {7, 3},
    //         {1, 5},
    //         {1, 6},
    //         {1, 7},
    //         {3, 4},
    //         {3, 5},
    //         {3, 6},
    //         {3, 7},
    //         {5, 4},
    //         {5, 5},
    //         {1, 2},
    //         {1, 3},
    //         {3, 0},
    //         {5, 0},
    //         {5, 1},
    //         {7, 2},
    //         {7, 3},
    //         {8, 0},
    //         {8, 1},
    //         {8, 2},
    //         {8, 3},
    //         {9, 0},
    //         {9, 1},
    //         {9, 2},
    //         {10, 3},
    //         {10, 0},
    //         {10, 1},
    //         {10, 2},
    //         {10, 3},
    //         {11, 0},
    //         {11, 1},
    //         {12, 2},
    //         {12, 3},
    //         {13, 0},
    //         {13, 1},
    //         {14, 2},
    //         {14, 3},
    //         {15, 0},
    //         {15, 1},
    //         {15, 2},
    //         {15, 3},
    //         {16, 1},
    //         {16, 0},
    //         {16, 3},
    //         {17, 0},
    //         {17, 1},
    //         {17, 2},
    //         {17, 3},
    //         {15, 0},
    //         {18, 1},
    //         {11, 2},
    //         {13, 3},
    //         {19, 0},
    //         {19, 1},
    //         {21, 2},
    //         {21, 3},
    //         {23, 0},
    //         {23, 1},
    //         {23, 2},
    //         {23, 3},
    //         {29, 0},
    //         {29, 1},
    //         {30, 0},
    //         {30, 3},
    //         {31, 0},
    //     };
    // static const std::vector<std::pair<unsigned, unsigned>> disabledBlocks_10 =
    //   [] {
    //     std::vector<std::pair<unsigned, unsigned>> v;
    //     // 103 sets × 16 ways = 1648 blocks
    //     for (unsigned set = 0; set < 103; ++set) {
    //         for (unsigned way = 0; way < 14; ++way) {
    //             v.emplace_back(set, way);
    //         }
    //     }
    //     return v;
    // }();
    // static const std::vector<std::pair<unsigned, unsigned>> disabledBlocks_40 =
    //   [] {
    //     std::vector<std::pair<unsigned, unsigned>> v;
    //     // 500 sets × 16 ways = 8000 blocks
    //     for (unsigned set = 0; set < 500; ++set) {
    //         for (unsigned way = 0; way < 14; ++way) {
    //             v.emplace_back(set, way);
    //         }
    //     }
    //     return v;
    // }();

    // static const std::vector<std::pair<unsigned, unsigned>> disabledBlocks_80 =
    //   [] {
    //     std::vector<std::pair<unsigned, unsigned>> v;
    //     // 800 sets × 16 ways = 12800 blocks
    //     for (unsigned set = 0; set < 800; ++set) {
    //         for (unsigned way = 0; way < 14; ++way) {
    //             v.emplace_back(set, way);
    //         }
    //     }
    //     return v;
    // }();

    BaseTags::BaseTags(const Params &p)
        : ClockedObject(p), blkSize(p.block_size), blkMask(blkSize - 1),
          // cache2DisableName(p.cache2DisableName),
          size(p.size), lookupLatency(p.tag_latency),
          system(p.system), indexingPolicy(p.indexing_policy),
          warmupBound((p.warmup_percentage / 100.0) * (p.size / p.block_size)),
          warmedUp(false), numBlocks(p.size / p.block_size),
          dataBlks(new uint8_t[p.size]), // Allocate data storage in one big chunk
          stats(*this)
    {
        registerExitCallback([this]()
                             { cleanupRefs(); });
    }

    ReplaceableEntry *
    BaseTags::findBlockBySetAndWay(int set, int way) const
    {
        return indexingPolicy->getEntry(set, way);
    }

    // [RZ CODE]
    // uint64_t
    // BaseTags::calculateIndexFromKey(Addr addr) const
    // {
    //     int assoc = indexingPolicy->getAssociativity();
    //     uint32_t block_offset_bits = log2(blkSize);
    //     uint32_t numSets = size / (blkSize * assoc);
    //     // Number of bits to represent the block offset
    //
    //     // Shift the key by the block offset bits to get the portion
    //     // of the address that corresponds to the index
    //     uint64_t index = (addr >> block_offset_bits) & (numSets - 1);
    //     // The mask is used to ensure that the index fits
    //     // within the number of sets
    //     return index;
    // }
    // void recordDisabledBlock(Addr key) {
    //     disabledBlockKeys.insert(key);
    // }
    // void recordDisabledSetAndWay(int set, int way) {
    //     // Insert the set and way as a pair into the set
    //     disabledSetandWay.insert(std::make_pair(set, way));
    // }
    // bool isKeyDisabled(int set, int way) {
    //     if (disabledSetandWay.find(std::make_pair(set, way))
    //            != disabledSetandWay.end()) {
    //         std::cout << "We found a disabled set and way: " << set  << "\n";
    //         return true;
    //     }
    //     return false;
    // }
    // bool BaseTags::isDisabled(int set, int way) const
    // {
    //     // Check if the block at the given set and way should be disabled
    //     for (const auto& pair : disabledBlocks_10) {
    //         if (pair.first == set && pair.second == way) {
    //             return true;
    //         }
    //     }
    //     return false;
    // }
    //
    // std::vector<ReplaceableEntry*>
    // BaseTags::Entries(Addr addr) const
    // {
    //     // Get the possible entries that may contain the given address
    //     return indexingPolicy->getPossibleEntries(addr);
    // }

    // CacheBlk*
    // BaseTags::findBlock(Addr addr, bool is_secure) const
    // {
    //     // Extract block tag
    //     Addr tag = extractTag(addr);
    //     std::string name = this->name();
    //     // Find possible entries that may contain the given address
    //     const std::vector<ReplaceableEntry*> entries = Entries(addr);
    //     bool disabled_smthg = false;
    //     for (int way = 0; way < entries.size(); ++way) {
    //         CacheBlk* blk = static_cast<CacheBlk*>(entries[way]);
    //         CacheBlk* blk1 =
    //             static_cast<CacheBlk*>(entries[entries.size() - way - 1]);
    //         uint64_t set = calculateIndexFromKey(addr);
    //         if (!blk) {
    //             continue;
    //         }
    //         if (curTick() < 1191257905980 && curTick() > 1191257904860){

    //             std::cout << "[BaseTags::findBlock] tick = "
    //                       << curTick() << "\n";
    //         }
    //         // if (this->name() == "board.cache_hierarchy.l1dcaches.tags") {
    //         //     //std::cout << "[BaseTags::findBlock] cache2DisableName =  "
    //         //     //          << cache2DisableName << "\n";
    //         //     //assert(false);
    //         //     //Use isDisabled to check if the block should be disabled

    //         //     if (isDisabled(set, way)) {
    //         //         // Check if the block is dcache
    //         //         //DPRINTF(CacheTags, "Cache2DisableName is = %d \n",
    //         //         //   cache2DisableName);
    //         //         std::cout << " way size is "
    //         //                   << entries.size() << std::endl;
    //         //         // recordDisabledBlock(addr);
    //         //         recordDisabledSetAndWay(set, way);
    //         //         // Print disabled block keys for debugging
    //         //         std::cout << "Disabled Sets and Ways: " << std::endl;
    //         //         for (const auto& disabledKey : disabledSetandWay) {
    //         //             std::cout << "this is disabled set and way  ("
    //         //                       << disabledKey.first << ", "
    //         //                       << disabledKey.second << ")" << std::endl;
    //         //         }
    //         //         std::cout << "\n" << std::endl;
    //                 // // Skip this block (not working because of snoop filter)
    //                 // return nullptr;
    //                 // if (blk->isValid()) {
    //                 //     // If the block is valid, invalidate it
    //                 //     std::cout << "Invalidating block at set: "
    //                 //               << set << ", way: " << way << std::endl;
    //                 //     blk->invalidate();
    //                 //      disabled_smthg = true;
    //                 // // }
    //                 // // continue; // Skip this block because its invalide
    //                 // size_t block_size = blkSize;
    //                 // memset(blk->data, 0, block_size);
    //         //     }
    //         // }
    //         // if (disabled_smthg) {
    //         //     doesnt work it doesnt like return block without check
    //         //     return blk;
    //         // }
    //         if (blk->matchTag(tag, is_secure)) {
    //             if ((set == 0 || set == 2 || set == 4 || set == 6)
    //                   && disabled_smthg) {
    //                 std::cout << "Return block at set: " << set
    //                           << ", way: " << way << std::endl;
    //             }
    //             return blk;
    //         }
    //     }
    //     // Did not find block
    //     return nullptr;
    // }

    CacheBlk *
    BaseTags::findBlock(Addr addr, bool is_secure) const
    {
        // Extract block tag
        Addr tag = extractTag(addr);

        // Find possible entries that may contain the given address
        const std::vector<ReplaceableEntry *> entries =
            indexingPolicy->getPossibleEntries(addr);

        // Search for block
        for (const auto &location : entries)
        {
            CacheBlk *blk = static_cast<CacheBlk *>(location);
            // [RZ CODE]
            // uint32_t set = blk->getSet();
            // uint32_t way = blk->getWay();
            if (blk->matchTag(tag, is_secure))
            {
                // [RZ CODE]
                // if (this->name()
                //        == "system.littleCluster.accel_0_pr_cache.tags") {
                //     if (isDisabled(set, way)) {
                //         // If the block is disabled, skip it
                //         blk->setFaultyBit(true); // Set up faulty bit
                //         std::cout << "Making faulty block at findblock at "
                //                   << "set RIZOS41: " << set
                //                   << ", way: " << way << "\n" << std::endl;
                //     }else {
                //         blk->setFaultyBit(false); // Set down faulty bit
                //     }
                // }
                // if (blk->isFaulty()) {
                //     if (rand() % 2 == 0){
                //         for (int i = 0; i < 2; ++i) {
                //          // unsigned assoc = indexingPolicy->getAssociativity();
                //     //         size_t block_idx = set * assoc + way;
                //             // Invalidate first byte of the block's storage and
                //             // point blk->data to it
                //             std::cout << "Before zero: "
                //                       << unsigned(blk->data[i]) << "\n";
                //             blk->data[i] = rand();
                //             blk->data[i+2] = rand();
                //             // blk->data[1] = 0;
                //             std::cout << "After zero: "
                //                       << unsigned(blk->data[i]) << "\n";
                //         }
                //     }
                // }
                return blk;
            }
        }

        // Did not find block
        return nullptr;
    }

    void
    BaseTags::insertBlock(const PacketPtr pkt, CacheBlk *blk)
    {
        // [RZ CODE]
        // uint32_t set = blk->getSet();
        // uint32_t way = blk->getWay();
        //
        // if (this->name() == "system.littleCluster.accel_0_pr_cache.tags") {
        //     [RZ CODE]
        //     for (uint32_t i = 0; i < entries.size(); ++i) {
        //         if (entries[i] == blk) {
        //             way = i;
        //             break;
        //         }
        //     }
        //     Assert that we found the block.
        //     This indicates a problem if it fails.
        //     assert(way != -1);
        //     Check if the block should be disabled
        //     if (isDisabled(set, way)) {
        //         // If the block is disabled, we should not insert it
        //         std::cout << "Making faulty block an insert at set RIZOS: "
        //                   << set
        //                   << ", way: " << way << "\n" << std::endl;
        //         blk->setFaultyBit(true); // Set up faulty bit
        //         // std::cout << "Skipping insertion of disabled block at set: "
        //         //           << set
        //         //           << ", way: " << way << std::endl;
        //     }
        //     else {
        //         blk->setFaultyBit(false); // Set down faulty bit
        //     }
        //     if (blk->isFaulty()) {
        //         // If the block is faulty, we should not insert it
        //         std::cout << "Skipping insertion of faulty block at set RIZOS: "
        //                   << set
        //                   << ", way: " << way << std::endl;
        //         return ;
        //     }
        // }

        assert(!blk->isValid());

        // Previous block, if existed, has been removed, and now we have
        // to insert the new one

        // Deal with what we are bringing in
        RequestorID requestor_id = pkt->req->requestorId();
        assert(requestor_id < system->maxRequestors());
        stats.occupancies[requestor_id]++;

        // [RZ CODE]
        // // Lets try to find the way and set of given block to skip it
        // Addr addr = pkt->getAddr();
        // const std::vector<ReplaceableEntry*> entries = Entries(addr);
        // assert(!entries.empty());

        // Insert block with tag, src requestor id and task id
        blk->insert(extractTag(pkt->getAddr()), pkt->isSecure(), requestor_id,
                    pkt->req->taskId());

        // Check if cache warm up is done
        if (!warmedUp && stats.tagsInUse.value() >= warmupBound)
        {
            warmedUp = true;
            stats.warmupTick = curTick();
        }

        // We only need to write into one tag and one data block.
        stats.tagAccesses += 1;
        stats.dataAccesses += 1;
    }

    void
    BaseTags::moveBlock(CacheBlk *src_blk, CacheBlk *dest_blk)
    {
        assert(!dest_blk->isValid());
        assert(src_blk->isValid());

        // Move src's contents to dest's
        *dest_blk = std::move(*src_blk);

        assert(dest_blk->isValid());
        assert(!src_blk->isValid());
    }

    Addr
    BaseTags::extractTag(const Addr addr) const
    {
        return indexingPolicy->extractTag(addr);
    }

    void
    BaseTags::cleanupRefsVisitor(CacheBlk &blk)
    {
        if (blk.isValid())
        {
            stats.totalRefs += blk.getRefCount();
            ++stats.sampledRefs;
        }
    }

    void
    BaseTags::cleanupRefs()
    {
        forEachBlk([this](CacheBlk &blk)
                   { cleanupRefsVisitor(blk); });
    }

    void
    BaseTags::computeStatsVisitor(CacheBlk &blk)
    {
        if (blk.isValid())
        {
            const uint32_t task_id = blk.getTaskId();
            assert(task_id < context_switch_task_id::NumTaskId);
            stats.occupanciesTaskId[task_id]++;
            Tick age = blk.getAge();

            int age_index;
            if (age / sim_clock::as_int::us < 10)
            { // <10us
                age_index = 0;
            }
            else if (age / sim_clock::as_int::us < 100)
            { // <100us
                age_index = 1;
            }
            else if (age / sim_clock::as_int::ms < 1)
            { // <1ms
                age_index = 2;
            }
            else if (age / sim_clock::as_int::ms < 10)
            { // <10ms
                age_index = 3;
            }
            else
                age_index = 4; // >10ms

            stats.ageTaskId[task_id][age_index]++;
        }
    }

    void
    BaseTags::computeStats()
    {
        for (unsigned i = 0; i < context_switch_task_id::NumTaskId; ++i)
        {
            stats.occupanciesTaskId[i] = 0;
            for (unsigned j = 0; j < 5; ++j)
            {
                stats.ageTaskId[i][j] = 0;
            }
        }

        forEachBlk([this](CacheBlk &blk)
                   { computeStatsVisitor(blk); });
    }

    std::string
    BaseTags::print()
    {
        std::string str;

        auto print_blk = [&str](CacheBlk &blk)
        {
            if (blk.isValid())
                str += csprintf("\tBlock: %s\n", blk.print());
        };
        forEachBlk(print_blk);

        if (str.empty())
            str = "no valid tags\n";

        return str;
    }

    BaseTags::BaseTagStats::BaseTagStats(BaseTags &_tags)
        : statistics::Group(&_tags),
          tags(_tags),

          ADD_STAT(tagsInUse, statistics::units::Rate<statistics::units::Tick, statistics::units::Count>::get(),
                   "Average ticks per tags in use"),
          ADD_STAT(totalRefs, statistics::units::Count::get(),
                   "Total number of references to valid blocks."),
          ADD_STAT(sampledRefs, statistics::units::Count::get(),
                   "Sample count of references to valid blocks."),
          ADD_STAT(avgRefs, statistics::units::Rate<statistics::units::Count, statistics::units::Count>::get(),
                   "Average number of references to valid blocks."),
          ADD_STAT(warmupTick, statistics::units::Tick::get(),
                   "The tick when the warmup percentage was hit."),
          ADD_STAT(occupancies, statistics::units::Rate<statistics::units::Count, statistics::units::Tick>::get(),
                   "Average occupied blocks per tick, per requestor"),
          ADD_STAT(avgOccs, statistics::units::Rate<statistics::units::Ratio, statistics::units::Tick>::get(),
                   "Average percentage of cache occupancy"),
          ADD_STAT(occupanciesTaskId, statistics::units::Count::get(),
                   "Occupied blocks per task id"),
          ADD_STAT(ageTaskId, statistics::units::Count::get(),
                   "Occupied blocks per task id, per block age"),
          ADD_STAT(ratioOccsTaskId, statistics::units::Ratio::get(),
                   "Ratio of occupied blocks and all blocks, per task id"),
          ADD_STAT(tagAccesses, statistics::units::Count::get(),
                   "Number of tag accesses"),
          ADD_STAT(dataAccesses, statistics::units::Count::get(),
                   "Number of data accesses")
    {
    }

    void
    BaseTags::BaseTagStats::regStats()
    {
        using namespace statistics;

        statistics::Group::regStats();

        System *system = tags.system;

        avgRefs = totalRefs / sampledRefs;

        occupancies
            .init(system->maxRequestors())
            .flags(nozero | nonan);
        for (int i = 0; i < system->maxRequestors(); i++)
        {
            occupancies.subname(i, system->getRequestorName(i));
        }

        avgOccs.flags(nozero | total);
        for (int i = 0; i < system->maxRequestors(); i++)
        {
            avgOccs.subname(i, system->getRequestorName(i));
        }

        avgOccs = occupancies / statistics::constant(tags.numBlocks);

        occupanciesTaskId
            .init(context_switch_task_id::NumTaskId)
            .flags(nozero | nonan);

        ageTaskId
            .init(context_switch_task_id::NumTaskId, 5)
            .flags(nozero | nonan);

        ratioOccsTaskId.flags(nozero);

        ratioOccsTaskId = occupanciesTaskId / statistics::constant(tags.numBlocks);
    }

    void
    BaseTags::BaseTagStats::preDumpStats()
    {
        statistics::Group::preDumpStats();

        tags.computeStats();
    }

} // namespace gem5
