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

#include "tp_src/mem/cache/decay/dueling.hh"

#include "base/bitfield.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "debug/TPDecayPolicies.hh"

namespace gem5
{

namespace tp
{

unsigned DecayDuelingMonitor::numInstances = 0;

DecayDueler::DecayDueler()
  : _isSample(false), _team(0)
{
}

void
DecayDueler::setSample(uint64_t team)
{
    _isSample = true;
    _team = team;
}

bool
DecayDueler::isSample(uint64_t& team) const
{
    team = _team;
    return _isSample;
}

DecayDuelingMonitor::DecayDuelingMonitor(std::size_t total_sets,
    std::size_t leader_sets,
    std::size_t constituency_size,
    std::size_t team_size, double low_threshold,
    double high_threshold, int s_factor)
  : id(1 << numInstances), numOfSets(total_sets),
    numOfLeaderTeamSets(leader_sets),
    constituencySize(constituency_size),
    teamSize(team_size), lowThreshold(low_threshold),
    highThreshold(high_threshold), sFactor(s_factor),
    regionCounter(0),
    constituencyCounter(0), // new code
    winner(2),
    standardLeaderTeamMisses(0)
{
    fatal_if(constituencySize < (NUM_DUELERS * teamSize),
        "There must be at least team size entries per team in a constituency");
    fatal_if(numInstances > 63, "Too many Dueling instances");
    fatal_if((lowThreshold <= 0.0) || (highThreshold >= 1.0),
        "The low threshold must be within the range ]0.0, 1.0[");
    fatal_if((highThreshold <= 0.0) || (highThreshold >= 1.0),
        "The high threshold must be within the range ]0.0, 1.0[");
    fatal_if(lowThreshold > highThreshold,
        "The low threshold must be below the high threshold");
    numInstances++;

    // Start selector around its middle value
    // selector.saturate();
    // selector >>= 1;
    // if (selector.calcSaturation() < lowThreshold) {
    //     winner = false;
    // }

    for (int i = 0; i < 4; i++) {
        selectors[i] = 0;
    }
}

bool
DecayDuelingMonitor::sample(const DecayDueler* dueler)
{
    if (dueler) {
        u_int64_t duelerTeam;

        if (dueler->isSample(duelerTeam)) {
            selectors[duelerTeam]++;

            int idealMisses = standardLeaderTeamMisses - selectors[2];
            DPRINTF(TPDecayPolicies, "DDM: Selectors (d/2, d, 2d): "
                "(%d, %d, %d)",
                selectors[0], selectors[2], selectors[1]);
            DPRINTF(TPDecayPolicies, " (%d, %d, %d)\n",
                selectors[0] + idealMisses,
                selectors[2] + idealMisses,
                selectors[1] + idealMisses);

            int maxSleepMisses =
                std::max(selectors[0], std::max(selectors[1], selectors[2]));
            if ((maxSleepMisses > 30 && idealMisses > 0)
                && maxSleepMisses >= 0.1 * idealMisses) {
                // dim: decay-induced misses
                // if dim(2d) >= 10% increase to dim(d), go to 4x decay.
                return false;
            }
        }
    }
    // bool team;
    // if (dueler->isSample(id, team)) {
    //     if (team) {
    //         selector++;

    //         if (selector.calcSaturation() >= highThreshold) {
    //             winner = true;
    //         }
    //     } else {
    //         selector--;

    //         if (selector.calcSaturation() < lowThreshold) {
    //             winner = false;
    //         }
    //     }
    // }

    return true;
}

bool
DecayDuelingMonitor::isSample(const DecayDueler* dueler, bool& team) const
{
    // return dueler->isSample(id, team);

    return true;
}

int
DecayDuelingMonitor::getWinner()
{
    winner = 2;
    int minMisses = 10000000;

    int idealMisses = standardLeaderTeamMisses - selectors[2];
    /**
     * Variation
     * 1) go down if (misses(d/2) - misses(d)) <= 1% * misses(d)
     */
    // int halfDecayMissesIncrease = selectors[0] - selectors[2];
    // int doubleDecayMissesDecrease = selectors[2] - selectors[1];


////////////// THRESHOLD MECHANISM ///////////////////////////////////////
    float lowLimit = 1.0 + lowThreshold;
    float highLimit = 1.0 - highThreshold;
// /*
    // if (halfDecayMissesIncrease >= 0 &&
    //         halfDecayMissesIncrease <= 0.01 * selectors[2]) {
    if (selectors[0] <= lowLimit * selectors[2]) {
        winner = 0;
    // } else if (doubleDecayMissesDecrease >= 0 &&
    //         doubleDecayMissesDecrease >= 0.02 * selectors[2]) {
    } else if (selectors[1] <= highLimit * selectors[2]) {
        winner = 1;
    } else {
        // for (int i = 0; i < NUM_DUELERS; i++) {
        //     if (selectors[i] <= minMisses) {
        //         minMisses = selectors[i];
        //         winner = i;
        //     }
        //     selectors[i] = 0;
        // }
        winner = 2;
    }

    int maxSleepMisses =
        std::max(selectors[0], std::max(selectors[1], selectors[2]));
    if ((maxSleepMisses > 0 && idealMisses > 0)
        && maxSleepMisses >= 0.1 * idealMisses) {
        // dim: decay-induced misses
        // if dim(2d) >= 10% increase to dim(d), go to 4x decay.
        winner = 3;
    }
    // */
//////////////// EOF THRESHOLD MECHANISM /////////////////////////////////

/////////////// NEW THRESHOLD MECHANISM //////////////////////////////////
    // count only non decay induced misses
/*    standardLeaderTeamMisses -= selectors[2];

    if ((selectors[0] + standardLeaderTeamMisses)
            <= 1.005 * (selectors[2] + standardLeaderTeamMisses)) {
        winner = 0;
    } else if ((selectors[1] + standardLeaderTeamMisses)
            <= 0.98 * (selectors[2] + standardLeaderTeamMisses)) {
        winner = 1;
    } else {
        winner = 2;
    } */
////////////// EOF NEW THRESHOLD MECHANISM ///////////////////////////////

    // reset counters
    for (int i = 0; i < NUM_DUELERS; i++) {
        selectors[i] = 0;
    }
    standardLeaderTeamMisses = 0;

    return winner;
}

void
DecayDuelingMonitor::initEntry(DecayDueler* dueler)
{
    //  #1 team: entries where constituency == set
    //  #2 team: entries where constituency == ~set
    //  #3 team: entries where
    //      for even constituencies, (constituency+2) == set
    //     for odd constituencies, (constituency-2) == ~set
    //
    DPRINTF(TPCacheDecayDebug, "inside initEntry #1\n");
    int teamSizeShifted =
        (teamSize*(constituencyCounter+1)-1) % constituencySize + 1;
    int prevTeamSizeShifted = teamSizeShifted - teamSize;

    //
    DPRINTF(TPCacheDecayDebug, "inside initEntry #2\n");
    int lead3TeamSizeShifted =
        (teamSize*(constituencyCounter+3)-1) % constituencySize + 1;
    int lead3PrevTeamSizeShifted = lead3TeamSizeShifted - teamSize;

    assert(dueler);
    if (regionCounter >= prevTeamSizeShifted &&
            regionCounter < teamSizeShifted) {
        dueler->setSample(0);
        DPRINTF(TPCacheDecayDebug, "constituency: %d, set: %d, team: 0\n",
            constituencyCounter, regionCounter/teamSize);
    } else if (regionCounter >= (constituencySize - teamSizeShifted) &&
            regionCounter < (constituencySize - prevTeamSizeShifted)) {
        dueler->setSample(1);
        DPRINTF(TPCacheDecayDebug, "constituency: %d, set: %d, team: 1\n",
            constituencyCounter, regionCounter/teamSize);
    } else if (constituencyCounter % 2 == 0) {
        if ((regionCounter >= lead3PrevTeamSizeShifted &&
                regionCounter < lead3TeamSizeShifted)) {
            dueler->setSample(2);
            DPRINTF(TPCacheDecayDebug, "constituency: %d, set: %d, team: 2\n",
                constituencyCounter, regionCounter/teamSize);
        }
    } else {
        if (regionCounter >= (constituencySize - lead3TeamSizeShifted) &&
                regionCounter <
                    (constituencySize - lead3PrevTeamSizeShifted)) {
            dueler->setSample(2);
            DPRINTF(TPCacheDecayDebug, "constituency: %d, set: %d, team: 2\n",
                constituencyCounter, regionCounter/teamSize);
        }
    }

    DPRINTF(TPCacheDecayDebug, "inside initEntry #3\n");

    // Check if we changed constituencies
    if (++regionCounter >= constituencySize) {
        regionCounter = 0;
        constituencyCounter++;
    }
}

int
DecayAMCMonitor::getWinner()
{
    int winner = 2;
    // separate ideal from sleep misses
    standardLeaderTeamMisses -= selectors[2];
    if (selectors[2] < (standardLeaderTeamMisses * 0.5 * pf)) {
        winner = 0; // halve the decay interval
    } else if (selectors[2] > (standardLeaderTeamMisses * 1.5 * pf)) {
        winner = 1; // double the decay interval
    } else {
        winner = 2; // keep the same decay interval
    }

    // reset counters
    for (int i = 0; i < NUM_DUELERS; i++) {
        selectors[i] = 0;
    }
    standardLeaderTeamMisses = 0;

    return winner;
}

void
DecayAMCMonitor::initEntry(DecayDueler* dueler)
{
    dueler->setSample(2);
}

} // namespace tp

} // namespace gem5
