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

#ifndef __MEM_CACHE_SET_SAMPLING_POLICIES_BASE_HH__
#define __MEM_CACHE_SET_SAMPLING_POLICIES_BASE_HH__

// #include <memory>

// #include "base/compiler.hh"
// #include "mem/cache/replacement_policies/replaceable_entry.hh"
// #include "mem/packet.hh"
#include "mem/cache/cache_blk.hh"
#include "params/BaseSetSamplingPolicy.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

/**
 * Replacement candidates as chosen by the indexing policy.
 */
// typedef std::vector<ReplaceableEntry*> ReplacementCandidates;

namespace tp
{

namespace set_sampling_policy
{

class SetSampler
{
  private:
    bool _isSample;

  public:
    SetSampler();
    ~SetSampler(){};

    void setSample();
    void unsetSample();
    bool isSample() const;
};

/**
 * A common base class of cache replacement policy objects.
 */
class Base : public ClockedObject
{
  protected:
    /** The block size of the cache. */
    const unsigned blkSize;
    /** The size of the cache. */
    const unsigned size;
    /** System we are currently operating in. */
    System *system;
    /** The associativity of the cache. */
    unsigned assoc;
    /** The total number of cache blocks */
    size_t numBlks;
    /** The total number of cache sets */
    size_t totalSets;
    /** The number of cache sets dedicated for monitoring */
    size_t dedicatedSets;
    /** The number of cache blocks in each constituency
     *  Constituency: a group of blocks in which
     *  there is only one sampler set.
     */
    size_t constituencySize;

    uint32_t samplingFactor;

    /**
     * Counts the number of entries have been initialized in the current
     * constituency.
     */
    int regionCounter;
    int constituencyCounter;

  public:
    typedef BaseSetSamplingPolicyParams Params;
    Base(const Params &p)
    : ClockedObject(p),
      blkSize(p.block_size),
      size(p.size),
      system(p.system),
      assoc(p.assoc),
      samplingFactor(p.sampling_factor)
    {
      numBlks = size / blkSize;
      totalSets = numBlks / assoc;
    }
    virtual ~Base() = default;

    /**
     * Check if the given dueler is a sample for this instance. If so, get its
     * team.
     *
     * @param dueler The selected entry.
     * @param team Team to which this sampling entry belongs (only 2 possible).
     * @return Whether this is a sampling entry.
     */
    virtual bool isSample(const SetSampler* sampler) const = 0;

    /**
     * Initialize a dueler entry, deciding wether it is a sample or not.
     * We opt for a complement approach, which dedicates the first entries
     * of a constituency to a team, and the last entries to the other team.
     *
     * @param dueler The entry to be initialized.
     */
    virtual void initEntry(SetSampler* sampler) = 0;

    virtual void initEntry(CacheBlk* blk) = 0;
};

} // namespace set_selection_policy
} // namespace tp
} // namespace gem5

#endif // __MEM_CACHE_SET_SAMPLING_POLICIES_BASE_HH__
