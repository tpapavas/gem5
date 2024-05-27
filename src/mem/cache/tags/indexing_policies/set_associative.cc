/*
 * Copyright (c) 2018 Inria
 * Copyright (c) 2012-2014,2017 ARM Limited
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
 * Copyright (c) 2003-2005,2014 The Regents of The University of Michigan
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
 * Definitions of a set associative indexing policy.
 */

#include "mem/cache/tags/indexing_policies/set_associative.hh"

#include "mem/cache/replacement_policies/replaceable_entry.hh"

//// MY CODE ////
#include "debug/TPIdle.hh"
#include "debug/TPFaulty.hh"
//// EOF MY CODE ////

namespace gem5
{

SetAssociative::SetAssociative(const Params &p)
    : BaseIndexingPolicy(p),
      faultySets(numSets) // MY CODE
{
    //// MY CODE ////
    // example of faulty sets
    //faultySets[1].push_back(2);
    //faultySets[3].push_back(0);
    //faultySets[5].push_back(3);
    //// EOF MY CODE ////
}

uint32_t
SetAssociative::extractSet(const Addr addr) const
{
    return (addr >> setShift) & setMask;
}

Addr
SetAssociative::regenerateAddr(const Addr tag, const ReplaceableEntry* entry)
                                                                        const
{
    return (tag << tagShift) | (entry->getSet() << setShift);
}

std::vector<ReplaceableEntry*>
SetAssociative::getPossibleEntries(const Addr addr) const
{
    //// MY CODE ////
    /*int set_index = extractSet(addr);
    int faulty_set_size = faultySets[set_index].size();

    // faulty sets are only for l1d caches and must have at least 1 element
    if (name().find("l1dcache") == std::string::npos || faulty_set_size == 0) {
        return sets[set_index];
    }

    std::vector<ReplaceableEntry*> set;
    int i = 0;

    for (const auto& entry : sets[set_index]) {
        if (i < faulty_set_size && entry->getWay() == faultySets[set_index][i]) {
            i++;
            //DPRINTF(TPFaulty, "%s\n", entry->print());
            continue; // ignore faulty blocks
        }
        set.push_back(entry);
    }
    //for (const auto& entry : set) {
    //    DPRINTF(TPFaulty, "%s\n", entry->print());
    //}
    //DPRINTF(TPFaulty, "\n\n");

    return set; // */
    //// MY CODE ////
    return sets[extractSet(addr)]; // old code
}

} // namespace gem5
