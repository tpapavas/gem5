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

#include <cmath>
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
  public:
    enum DuelingType
      {
        PLAIN,
        JUMP,
        E_JUMP,
        E_JUMP_C,
        UD_S,
        UD_S_SIMPLE,
        EN_AWARE
      };

  protected:
    //// tour-var code ////
    //// eof tour-var code ////

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
    std::size_t numOfLTBlks;
    uint64_t standardLeaderTeamMisses[4];
    double toffRatios[4];
    std::size_t udLimit;

    Cycles wInCycles;

    double ltLeakage;
    double LSetsToSetsRatio;

    const double memDynamic = 7.2; // nj per read (access)
    const double cacheBlkLeakagePow = 0.000033; // nj per ns

    /**
     * Threshold for downscaling.
     */
    const double lowThreshold;

    /**
     * Threshold for upscaling.
     */
    const double highThreshold;

    double lowLimit, highLimit;

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

    uint64_t globCounter;

    /** The team that is currently winning. */
    int winner;

    int sFactor;

    //// tour-var code ////
    DuelingType duelingType = DuelingType::PLAIN;
    //// eof tour-var code ////

    double _a, _b, _c;

    virtual bool downscaleCondition() { return false; }
    virtual bool upscaleCondition() { return false; }
    virtual bool jumpscaleCondition(int im) { return false; }

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
        double low_threshold = 0.01,
        double high_threshold = 0.02,
        int s_factor = 4,
        Tick clock_ticks = 0,
        Cycles w_cycles = Cycles(0),
        DuelingType dueling_type = DuelingType::PLAIN);
    ~DecayDuelingMonitor() = default;

    /**
     * If given dueler is a sampling entry, sample it and check if the
     * winning team must be updated.
     *
     * @param dueler The selected entry.
     */
    virtual bool sample(const DecayDueler* dueler);

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
    virtual int getWinner();
    virtual int getEnergyWinner();

    /**
     * Initialize a dueler entry, deciding wether it is a sample or not.
     * We opt for a complement approach, which dedicates the first entries
     * of a constituency to a team, and the last entries to the other team.
     *
     * @param dueler The entry to be initialized.
     */
    virtual void initEntry(DecayDueler* dueler);

    void incLTMisses(int ltId) { standardLeaderTeamMisses[ltId]++; }

    int getScaleFactor() { return sFactor; }

    const int *getSelectors() { return selectors; }
    const uint64_t *getLTMisses()  { return standardLeaderTeamMisses; }
    int getNumOfDuelers() { return NUM_DUELERS; }
    void setDuelingType(DuelingType dueling_type) {
      duelingType = dueling_type;
    }

    void updateGlobalCounter() { globCounter++; }
    void resetGlobalCounter() { globCounter = 0; }

    void incTOff(uint64_t leaderTeam) {
      toffRatios[leaderTeam] += 1.0;
    }
    void resetTOffs() {
      for (int i = 0; i < NUM_DUELERS; i++) {
        toffRatios[i] = 0;
      }
    }

    void calcTOffs() {
      for (int i = 0; i < NUM_DUELERS; i++) {
        toffRatios[i] /= (globCounter * numOfLTBlks);
      }
    }
};

class TourPlain : public DecayDuelingMonitor
{
  protected:
    virtual bool downscaleCondition() override {
      return selectors[0] <= lowLimit * selectors[2];
    }
    virtual bool upscaleCondition() override {
      return selectors[1] <= highLimit * selectors[2];
    }

  public:
    TourPlain(std::size_t sets, std::size_t l_sets, std::size_t c_size,
      std::size_t t_size = 1, double l_thres = 0.01, double h_thres = 0.02,
      int sf = 4, Tick clk_ticks = 0, Cycles w_cycles = Cycles(0),
      DuelingType dueling_type = DuelingType::PLAIN):
    DecayDuelingMonitor(sets, l_sets, c_size, t_size, l_thres, h_thres, sf,
      clk_ticks, w_cycles, dueling_type) {};
};

class TourJump : public TourPlain
{
  protected:
    virtual bool jumpscaleCondition(int idealMisses) override {
      return ((winner == 2)
               && (selectors[1] > 30 && idealMisses > 0)
               && (selectors[1] >= 0.1 * idealMisses));
    }

  public:
    TourJump(std::size_t sets, std::size_t l_sets, std::size_t c_size,
      std::size_t t_size = 1, double l_thres = 0.01, double h_thres = 0.02,
      int sf = 4, Tick clk_ticks = 0, Cycles w_cycles = Cycles(0),
      DuelingType dueling_type = DuelingType::JUMP):
    TourPlain(sets, l_sets, c_size, t_size, l_thres, h_thres, sf,
      clk_ticks, w_cycles, dueling_type) {};
};

class TourEJump : public TourJump
{
  protected:
    virtual bool jumpscaleCondition(int idealMisses) override {
      int maxSleepMisses =
            std::max(selectors[0], std::max(selectors[1], selectors[2]));
      return ((maxSleepMisses > 30 && idealMisses > 0)
                && (maxSleepMisses >= 0.1 * idealMisses));
    }

  public:
    TourEJump(std::size_t sets, std::size_t l_sets, std::size_t c_size,
      std::size_t t_size = 1, double l_thres = 0.01, double h_thres = 0.02,
      int sf = 4, Tick clk_ticks = 0, Cycles w_cycles = Cycles(0),
      DuelingType dueling_type = DuelingType::E_JUMP):
    TourJump(sets, l_sets, c_size, t_size, l_thres, h_thres, sf,
      clk_ticks, w_cycles, dueling_type) {};
};

class TourEJumpC : public TourEJump
{
  protected:
    virtual bool upscaleCondition() override {
      return selectors[1] < highLimit * selectors[2];
    }
    virtual bool jumpscaleCondition(int idealMisses) override {
      int maxSleepMisses = std::max(selectors[1], selectors[2]);
      return ((maxSleepMisses > 30 && idealMisses > 0)
               && (maxSleepMisses >= 0.1 * idealMisses));
    }

  public:
    TourEJumpC(std::size_t sets, std::size_t l_sets, std::size_t c_size,
      std::size_t t_size = 1, double l_thres = 0.01, double h_thres = 0.02,
      int sf = 4, Tick clk_ticks = 0, Cycles w_cycles = Cycles(0),
      DuelingType dueling_type = DuelingType::E_JUMP_C):
    TourEJump(sets, l_sets, c_size, t_size, l_thres, h_thres, sf,
      clk_ticks, w_cycles, dueling_type) {};
};

class TourUD_S : public DecayDuelingMonitor
{
  protected:
    virtual bool downscaleCondition() override {
      return (_a*(selectors[0]-selectors[2]) + pow(selectors[0], _b) / _c)
                < udLimit;
    }
    virtual bool upscaleCondition() override {
      return (_a*(selectors[2]-selectors[1]) + pow(selectors[2], _b) / _c)
                > udLimit;
    }
    virtual bool jumpscaleCondition(int idealMisses) override {
      int maxSleepMisses = std::max(selectors[1], selectors[2]);
      return ((maxSleepMisses > 30 && idealMisses > 0)
               && (maxSleepMisses >= 0.1 * idealMisses));
    }

  public:
    TourUD_S(std::size_t sets, std::size_t l_sets, std::size_t c_size,
      std::size_t t_size = 1, double l_thres = 0.01, double h_thres = 0.02,
      int sf = 4, Tick clk_ticks = 0, Cycles w_cycles = Cycles(0),
      DuelingType dueling_type = DuelingType::UD_S,
      double a = 2, double b = 2, double c = 2):
    DecayDuelingMonitor(sets, l_sets, c_size, t_size, l_thres, h_thres, sf,
      clk_ticks, w_cycles, dueling_type)
    {
      _a = a; _b = b; _c = c;
      std::size_t maxDIMs = 320 * LSetsToSetsRatio;
      // udLimit = 2*maxDIMs + (maxDIMs * maxDIMs)/2;
      udLimit = _a*maxDIMs + pow(maxDIMs, _b)/_c;
      // udLimit = 200;
      printf("LIM: %ld\n", udLimit);
    };
};

class TourUD_S_Simple : public TourUD_S
{
  public:
    TourUD_S_Simple(std::size_t sets, std::size_t l_sets, std::size_t c_size,
      std::size_t t_size = 1, double l_thres = 0.01, double h_thres = 0.02,
      int sf = 4, Tick clk_ticks = 0, Cycles w_cycles = Cycles(0),
      DuelingType dueling_type = DuelingType::UD_S_SIMPLE):
    TourUD_S(sets, l_sets, c_size, t_size, l_thres, h_thres, sf,
      clk_ticks, w_cycles, dueling_type, 0, 1, 1) {};
};

class TourEnAware : public DecayDuelingMonitor
{
  // protected:
  //   virtual bool downscaleCondition() override {
  //     return ;
  //   }
  //   virtual bool upscaleCondition() override {
  //     return ;
  //   }

  public:
    TourEnAware(std::size_t sets, std::size_t l_sets, std::size_t c_size,
      std::size_t t_size = 1, double l_thres = 0.01, double h_thres = 0.02,
      int sf = 4, Tick clk_ticks = 0, Cycles w_cycles = Cycles(0),
      DuelingType dueling_type = DuelingType::EN_AWARE):
    DecayDuelingMonitor(sets, l_sets, c_size, t_size, l_thres, h_thres, sf,
      clk_ticks, w_cycles, dueling_type) {};
};

class DecayAMCMonitor : public DecayDuelingMonitor
{
  protected:
    double pf; // performance factor
  public:
    DecayAMCMonitor(std::size_t total_sets,
        std::size_t leader_sets,
        std::size_t constituency_size,
        std::size_t team_size = 1,
        double low_threshold = 0.5,
        double high_threshold = 0.5,
        DuelingType dueling_type = DuelingType::PLAIN)
    : DecayDuelingMonitor(total_sets,
        leader_sets,
        constituency_size,
        team_size,
        low_threshold,
        high_threshold, -1, Tick(0), Cycles(0),
        dueling_type),
      pf(0.5) {}
    ~DecayAMCMonitor() = default;

    // virtual void sample(const DecayDueler* dueler) override;

    virtual int getWinner() override;

    virtual void initEntry(DecayDueler* dueler) override;
};

} // namespcae tp

} // namespace gem5

#endif // __BASE_DUELING_HH__
