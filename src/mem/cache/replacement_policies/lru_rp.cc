/**
 * Copyright (c) 2018-2020 Inria
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

#include "mem/cache/replacement_policies/lru_rp.hh"

#include <cassert>
#include <memory>

#include "debug/CacheFaulty.hh"
#include "mem/cache/cache_blk.hh"
#include "params/LRURP.hh"
#include "sim/cur_tick.hh"

namespace gem5
{

namespace replacement_policy
{

LRU::LRU(const Params &p)
  : Base(p)
{
}

void
LRU::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
{
    // Reset last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = Tick(0);
}

void
LRU::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Update last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}

void
LRU::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Set last touch timestamp
    std::static_pointer_cast<LRUReplData>(
        replacement_data)->lastTouchTick = curTick();
}

ReplaceableEntry*
LRU::getVictim(const ReplacementCandidates& candidates) const
{
    // There must be at least one replacement candidate
    assert(candidates.size() > 0);

    // Visit all candidates to find victim
    ReplaceableEntry* victim = nullptr; // candidates[0];
    // ReplaceableEntry* victim = candidates[0];
    for (const auto& candidate : candidates) {
        //// FAULTY-BLKS CODE ////

        CacheBlk* blk = static_cast<CacheBlk*>(candidate);

        // If the candidate block is faulty, skip it and go
        // to the next candidate
        if (blk->isDisabled()) {
            continue;
        }

        if (victim == nullptr) {
            victim = candidate;
        }

        //// EOF FAULTY-BLKS CODE ////

        // Update victim entry if necessary
        if (std::static_pointer_cast<LRUReplData>(
                    candidate->replacementData)->lastTouchTick <
                std::static_pointer_cast<LRUReplData>(
                    victim->replacementData)->lastTouchTick) {
            victim = candidate;
        }
    }

    //// FAULTY-BLKS CODE ////
    // In case where all ways of a set are faulty, nullptr is returned.
    // This is handled like a miss.

    // We should get a victim (as long as there is at least on non-faulty blk)
    if (victim == nullptr) {
        DPRINTF(CacheFaulty,
            "In set %d there was no victim. Set has %sfaulty blocks.\n",
            candidates[0]->getSet(),
            static_cast<CacheBlk*>(candidates[0])->getFaulty(0) ? "" : "not "
        );
        DPRINTF(CacheFaulty, "\tWay 0: %s faulty\n",
            static_cast<CacheBlk*>(candidates[0])->getFaulty(0) ? "" : "not");
        DPRINTF(CacheFaulty, "\tWay 1: %s faulty\n",
            static_cast<CacheBlk*>(candidates[1])->getFaulty(0) ? "" : "not");
        DPRINTF(CacheFaulty, "\tWay 2: %s faulty\n",
            static_cast<CacheBlk*>(candidates[2])->getFaulty(0) ? "" : "not");
        DPRINTF(CacheFaulty, "\tWay 3: %s faulty\n",
            static_cast<CacheBlk*>(candidates[3])->getFaulty(0) ? "" : "not");
    }
    //// EOF FAULTY-BLKS CODE ////

    return victim;
}

std::shared_ptr<ReplacementData>
LRU::instantiateEntry()
{
    return std::shared_ptr<ReplacementData>(new LRUReplData());
}

} // namespace replacement_policy
} // namespace gem5
