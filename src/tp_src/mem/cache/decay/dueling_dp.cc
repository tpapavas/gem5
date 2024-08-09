#include "tp_src/mem/cache/decay/dueling_dp.hh"

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "tp_src/mem/cache/decay/constant_dp.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

Dueling::Dueling()
    : Base()
{
    for (int i = 0; i < NUM_DUELERS; i++) {
        duelingPolicies.push_back(new tp::decay_policy::Constant());
    }

    duelerData = new DecayDueler();
    DPRINTF(TPCacheDecayDebug, "DuelingDP: constructor\n");
}

void
Dueling::updateDecay()
{
    for (int i = 0; i < NUM_DUELERS; i++) {
        duelingPolicies[i]->updateDecay();
    }
}

void
Dueling::handleHit(std::shared_ptr<tp::decay_policy::GlobalDecayData>&
    globDecayData)
{
}

void
Dueling::handleMiss(std::shared_ptr<tp::decay_policy::GlobalDecayData>&
    globDecayData)
{

}

void
Dueling::setDecay(int decay)
{
    // for (int i = 0; i < NUM_DUELERS; i++) {
        // duelingPolicies[i]->setDecay(decay);
    // }

    duelingPolicies[0]->setDecay(decay/2);
    duelingPolicies[1]->setDecay(decay*2);
    duelingPolicies[2]->setDecay(decay);
}

int
Dueling::getDecay()
{
    uint64_t thisTeam;

    if (duelerData->isSample(thisTeam)) {
        return duelingPolicies[thisTeam]->getDecay();
    }

    return duelingPolicies[2]->getDecay();
}

} // namespace decay_policy

} // namespace tp

} // namespace gem5
