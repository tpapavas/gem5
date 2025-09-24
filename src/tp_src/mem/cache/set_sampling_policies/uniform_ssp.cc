#include "tp_src/mem/cache/set_sampling_policies/uniform_ssp.hh"

#include "debug/TPSetSamplingPolicyDebug.hh"

namespace gem5
{

namespace tp
{

namespace set_sampling_policy
{

UniformSSP::UniformSSP(const Params &p)
  :
    Base(p)
{
    samplingFactor = p.sampling_factor;

    dedicatedSets = totalSets / samplingFactor;
    constituencySize = numBlks / dedicatedSets;
    fatal_if(constituencySize < assoc,
        "There must be at least |assoc| entries (blks) in a constituency");
    // fatal_if(numInstances > 63, "Too many Dueling instances");
    // numInstances++;

    // LSetsToSetsRatio = double(numOfLeaderTeamSets) / numOfSets;

    // double clkFreq = 1000.0 / clock_ticks;  // GHz
    // size_t numBlks = numOfSets * teamSize;
    // numOfLTBlks = numBlks * LSetsToSetsRatio;

    // float cacheLeakage = cacheBlkLeakagePow/clkFreq*numBlks; // nj per cycle
    // ltLeakage = cacheLeakage * LSetsToSetsRatio; // nj per cycle

    // DPRINTF(TPDecayPoliciesStats, "Leader Team leakage: %.4lf\n"
    //     "Window (cycles): %d\n", ltLeakage, wInCycles);
}

void
UniformSSP::initEntry(SetSampler* sampler)
{
    unsigned teamSize = assoc;
    //  sample team: entries from 1st set of each constituency
    //  not-sample team: all other sets in each constituency

    assert(sampler);
    if (regionCounter >= 0 && regionCounter < teamSize) {
        sampler->setSample();
        DPRINTF(TPSetSamplingPolicyDebug,
            "constituency: %d, set: %d, team: SAMPLE\n",
            constituencyCounter, regionCounter/teamSize);
    } else {
      sampler->unsetSample();
          DPRINTF(TPSetSamplingPolicyDebug,
            "constituency: %d, set: %d, team: NOT SAMPLE\n",
            constituencyCounter, regionCounter/teamSize);
    }

    // Check if we changed constituencies
    if (++regionCounter >= constituencySize) {
        regionCounter = 0;
        constituencyCounter++;
    }
}

void
UniformSSP::initEntry(CacheBlk* blk)
{
    unsigned setOffset = blk->getSet() % samplingFactor;
    //  sample team: entries from 1st set of each constituency
    //  not-sample team: all other sets in each constituency

    assert(sampler);
    if (setOffset == 0) {
        blk->getSetSampler()->setSample();
        DPRINTF(TPSetSamplingPolicyDebug,
            "constituency: %d, set: %d, team: SAMPLE\n",
            constituencyCounter, regionCounter/teamSize);
    } else {
      sblk->getSetSampler()->unsetSample();
          DPRINTF(TPSetSamplingPolicyDebug,
            "constituency: %d, set: %d, team: NOT SAMPLE\n",
            constituencyCounter, regionCounter/teamSize);
    }
}

}  // namespace set_sampling_policy

}  // namespace tp

}  // namespace gem5
