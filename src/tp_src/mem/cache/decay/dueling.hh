/**
 * Copyright (c) 2019, 2020 Inria
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

#ifndef __BASE_DUELING_HH__
#define __BASE_DUELING_HH__

#include <cstddef>
#include <cstdint>

#include "base/sat_counter.hh"

namespace gem5
{

/**
 * A dueler is an entry that may or may not be accounted for sampling.
 * Whenever an action triggers sampling, the dueling monitor will check
 * if the dueler is a sample so that it can decide whether to perform
 * the sampling or not.
 *
 * Each sampling dueler belongs to a team, "True" or "False", for which
 * it "duels". For example, if a sample belongs to team "True" and it is
 * sampled, team "True" will score a point.
 *
 * @see DuelingMonitor
 */

namespace tp
{

class DecayDueler
{
  private:
    // uint64_t _isSample;
    bool _isSample;

    uint64_t _team;

  public:
    /** By default initializes entries as followers. */
    DecayDueler();

    virtual ~DecayDueler() = default;

    void setSample(uint64_t team);

    bool isSample(uint64_t& team) const;
};

/**
 * Duel between two sampled options to determine which is the winner. The
 * duel happens when a sample is taken: the saturating counter is increased
 * if the team is "True", or decreased if the team is "False". Whenever the
 * counter passes the flipping threshold the winning team changes.
 *
 * By default the threshold to change teams is the same on both ways, but
 * this may introduce unwanted flickering, so the threshold can be split
 * so that it takes longer to change the winning team again just after
 * changing it.
 *
 * Based on Set Dueling, proposed in "Adaptive Insertion Policies for High
 * Performance Caching".
 */
class DecayDuelingMonitor
{
  private:
    // There are always exactly two duelers. If this is changed the logic
    // must be revisited
    const int NUM_DUELERS = 3;

    /**
     * Unique identifier of this instance. It is a one bit mask used to
     * identify which Dueler refers to this duel. This is done so that
     * an entry can be dueled by many different policies simultaneously,
     * which may even be of different domains (e.g., an entry can duel for
     * 2 replacement policies, and 2 compression methods at the same time).
     */
    const uint64_t id;

    /**
     * [size in number of blocks]
     * Given a table containing X entries, a constituency is a region of
     * the table such that it contains X/constituencySize entries. Each
     * constituency contains one sample of each dueler.
     */
    const std::size_t constituencySize;

    /**
     * [size in number of blocks]
     * Number of entries that belong to each team within a constituency.
     */
    const std::size_t teamSize;

    const std::size_t numOfSets;
    const std::size_t numOfLeaderTeamSets;
    uint64_t standardLeaderTeamMisses;

    /**
     * If the winning team was "True", and the counter is decreased further
     * than this threshold, "False" will become the winning team.
     */
    const double lowThreshold;

    /**
     * If the winning team was "False", and the counter is increased further
     * than this threshold, "True" will become the winning team.
     */
    const double highThreshold;

    /**
     * Counter that determines which dueler is winning.
     * In the DIP paper they propose using a 10-11 bit saturating counter.
     */
    int selectors[4];
    // SatCounter32 selector;

    /**
     * Counts the number of entries have been initialized in the current
     * constituency.
     */
    int regionCounter;
    int constituencyCounter;

    /** The team that is currently winning. */
    int winner;

  public:
    /**
     * Number of times this class has been instantiated. It is used to assign
     * unique ids to each dueling monitor instance.
     */
    static unsigned numInstances;

    DecayDuelingMonitor(std::size_t total_sets,
        std::size_t leader_sets,
        std::size_t constituency_size,
        std::size_t team_size = 1,
        double low_threshold = 0.5,
        double high_threshold = 0.5);
    ~DecayDuelingMonitor() = default;

    /**
     * If given dueler is a sampling entry, sample it and check if the
     * winning team must be updated.
     *
     * @param dueler The selected entry.
     */
    void sample(const DecayDueler* dueler);

    /**
     * Check if the given dueler is a sample for this instance. If so, get its
     * team.
     *
     * @param dueler The selected entry.
     * @param team Team to which this sampling entry belongs (only 2 possible).
     * @return Whether this is a sampling entry.
     */
    bool isSample(const DecayDueler* dueler, bool& team) const;

    /**
     * Get the team that is currently winning the duel.
     *
     * @return Winning team.
     */
    int getWinner();

    /**
     * Initialize a dueler entry, deciding wether it is a sample or not.
     * We opt for a complement approach, which dedicates the first entries
     * of a constituency to a team, and the last entries to the other team.
     *
     * @param dueler The entry to be initialized.
     */
    void initEntry(DecayDueler* dueler);

    void incStdLTMisses() { standardLeaderTeamMisses++; }
};

} // namespcae tp

} // namespace gem5

#endif // __BASE_DUELING_HH__
