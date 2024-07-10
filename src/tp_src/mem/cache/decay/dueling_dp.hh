#ifndef __TP_CACHE_DECAY_MECHANISM_DUELING_DP_HH__
#define __TP_CACHE_DECAY_MECHANISM_DUELING_DP_HH__

#include <vector>

//#include "params/BASE.hh"

#include "sim/clocked_object.hh"
#include "tp_src/mem/cache/decay/base.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

class Base;

class DuelingDecayData : public GlobalDecayData
{
  protected:
    int globalCounter;

  public:
    DuelingDecayData() {};

    void setGlobal(int val) { globalCounter = val; }
    int getGlobal() { return globalCounter;  }

  friend Dueling;
};

class Dueling : public Base
{
  protected:
    std::vector<Base*> const duelingPolicies;
  public:
    Dueling();
};

} // decay policy

} // namespace tp

} // namespace gem5

#endif // __TP_CACHE_DECAY_MECHANISM_BASE_HH__
