#ifndef __TP_CACHE_DECAY_MECHANISM_DUELING_DP_HH__
#define __TP_CACHE_DECAY_MECHANISM_DUELING_DP_HH__

#include <vector>

//#include "params/BASE.hh"

#include "debug/TPCacheDecayDebug.hh"
#include "sim/clocked_object.hh"
#include "tp_src/mem/cache/decay/base.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

class DuelingDecayData;

class Dueling : public Base
{
  protected:
    const int NUM_DUELERS = 3;

    std::vector<Base*> duelingPolicies;

    DecayDueler* duelerData;

  public:
    Dueling();

    virtual void handleHit(std::shared_ptr<GlobalDecayData>&) override;
    virtual void handleMiss(std::shared_ptr<GlobalDecayData>&) override;

    virtual void updateDecay() override;
    virtual void setDecay(int decay) override;
    virtual int getDecay() override;

    DecayDueler* getDecayDueler() override {
      DPRINTF(TPCacheDecayDebug, "DuelingDP: getDecayDueler\n");
      return duelerData;
    }
};

class DuelingDecayData : public GlobalDecayData
{
  protected:
    int _globalCounter;

    DecayDuelingMonitor* decayDuelingMonitor;

  public:
    DuelingDecayData() {};

    virtual void setGlobal(int val) override { _globalCounter = val; }
    virtual int getGlobal() override { return _globalCounter;  }

    virtual Dueling* instantiateDecay() override {
      return new Dueling();
    }

    void setDecayDuelingMonitor(DecayDuelingMonitor* monitor) {
      decayDuelingMonitor = monitor;
    }


    virtual void updateMaxGlobals(int) override {};
    virtual void checkAcumOverflow(int) override {};

  friend Dueling;
};

} // decay policy

} // namespace tp

} // namespace gem5

#endif // __TP_CACHE_DECAY_MECHANISM_BASE_HH__
