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

#ifndef __MEM_CACHE_SET_SAMPLING_POLICIES_UNIFORM_SSP_HH__
#define __MEM_CACHE_SET_SAMPLING_POLICIES_UNIFORM_SSP_HH__

// #include <memory>

// #include "base/compiler.hh"
// #include "mem/cache/replacement_policies/replaceable_entry.hh"
// #include "mem/packet.hh"
#include "params/UniformSSP.hh"
#include "tp_src/mem/cache/set_sampling_policies/base.hh"

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

/**
 * A common base class of cache replacement policy objects.
 */
class UniformSSP : public Base
{
  public:
    typedef UniformSSPParams Params;
    UniformSSP(const Params &p);
    virtual ~UniformSSP() = default;

    /**
     * Check if the given dueler is a sample for this instance. If so, get its
     * team.
     *
     * @param sampler The selected entry.
     * @param team Team to which this sampling entry belongs (only 2 possible).
     * @return Whether this is a sampling entry.
     */
    virtual bool isSample(const SetSampler* sampler) const override {};

    /**
     * Initialize a dueler entry, deciding wether it is a sample or not.
     * We opt for a complement approach, which dedicates the first entries
     * of a constituency to a team, and the last entries to the other team.
     *
     * @param sampler The entry to be initialized.
     */
    virtual void initEntry(SetSampler* sampler) override;
    virtual void initEntry(CacheBlk* blk) override;
};

} // namespace set_selection_policy
} // namespace tp
} // namespace gem5

#endif // __MEM_CACHE_SET_SAMPLING_POLICIES_UNIFORM_SSP_HH__
