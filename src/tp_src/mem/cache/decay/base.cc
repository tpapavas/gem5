#include "tp_src/mem/cache/decay/base.hh"

#include "debug/TPCacheDecay.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

Base::Base()
{
}

// void
// Base::updateDecay()
// {
// }

// void
// Base::handleHit(IATACdata *iatac)
// {
// }

// void
// Base::handleMiss(IATACdata *iatac)
// {
// }

std::string
Base::print() const
{
    // return csprintf("thits: %d elapsed: %d counter: %d ",
    //     _thits, _elapsed, _counter);
}

void
GlobalDecayData::printGlobals()
{
    // printf("Acumcounter:\t");
    // for (int i = 0; i < _MAX_ACCESS; i++) {
    //     printf("%6d ", _acumcounter[i]);
    // }
    // printf("\n");
}

GlobalDecayData::GlobalDecayData()
{
}

// void
// globalDecayData::updateMaxGlobals(int p)
// {
// }

// void
// globalDecayData::checkAcumOverflow(int p)
// {
// }

// void
// globalDecayData::setGlobal(int val)
// {
//     for (int i = 0; i < _MAX_ACCESS; i++) {
//         setGlobal(i, val);
//     }
// }

} // namespace decay_policy

} // namespace tp

} // namespace gem5
