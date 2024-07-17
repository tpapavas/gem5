#ifndef __TP_CACHE_DECAY_MECHANISM_BASE_HH__
#define __TP_CACHE_DECAY_MECHANISM_BASE_HH__

#include <memory>

#include "base/cprintf.hh"
#include "base/trace.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "tp_src/mem/cache/decay/dueling.hh"

namespace gem5
{

namespace tp
{

namespace decay_policy
{

class GlobalDecayData;

class Base
{
  public:
    Base();

    virtual void handleHit(std::shared_ptr<GlobalDecayData>&) = 0;
    virtual void handleMiss(std::shared_ptr<GlobalDecayData>&) = 0;

    virtual void updateDecay() = 0;

    virtual int getDecay() { return _decay; }
    virtual void setDecay(int decay) { _decay = decay; }

    virtual void resetCounter()
    {
      DPRINTF(TPCacheDecayDebug, "BaseDP: resetCounter\n");
      _counter = 1;
    }
    virtual bool isDecayable()
    {
      DPRINTF(TPCacheDecayDebug, "BaseDP: isDecayable\n");
      return true;
    }
    virtual bool decayElapsed()
    {
      DPRINTF(TPCacheDecayDebug, "BaseDP: decayElapsed\n");
      return _decay < 0;
    }

    int getCounter() { return _counter; }

    void setPower(bool onoff) { _onoff = onoff; }
    bool isPoweredOff() const { return !_onoff; }

    virtual std::string print() const;

    virtual DecayDueler* getDecayDueler() {
      DPRINTF(TPCacheDecayDebug, "BaseDP: getDecayDueler\n");
      return nullptr;
    }

  protected:
    /** if the block is on or off */
    bool _onoff = true;

    /** decay interval that has to elapse. */
    int _decay = 8192;

    /** access counter */
    int _counter = 1;
};

class GlobalDecayData
{
  public:
    GlobalDecayData();

    bool isOn() { return _on; }
    void setOn(bool onoff) { _on = onoff; }

    virtual void updateMaxGlobals(int) = 0;

    virtual void checkAcumOverflow(int) = 0;

    virtual void setGlobal(int val) = 0;
    virtual int getGlobal() = 0;

    virtual void printGlobals();

    virtual Base* instantiateDecay() = 0;

    virtual void sample(int &globalDecayCounter) {}

        //// needed only for IATAC ////
    // virtual int getInitLocalDecay()
    // {
    //   DPRINTF(TPCacheDecayDebug, "BaseDP: getInitLocalDecay\n");
    //   return -1;
    // }
    // virtual void setInitLocalDecay(int init_local_decay)
    // {
    //   DPRINTF(TPCacheDecayDebug, "BaseDP: setInitLocalDecay\n");
    // }
    // virtual bool doLetOverflow()
    // {
    //   DPRINTF(TPCacheDecayDebug, "BaseDP: doLetOverflow\n");
    //   return -1;
    // }
    // virtual void setLetOverflow(bool letOverflow)
    // {
    //   DPRINTF(TPCacheDecayDebug, "BaseDP: setLetOverflow\n");
    // }
    // virtual bool doResetCounterOnHit()
    // {
    //   DPRINTF(TPCacheDecayDebug, "BaseDP: doResetCounterOnHit\n");
    //   return -1;
    // }
    // virtual void setResetCounterOnHit(bool reset_counter_on_hit)
    // {
    //   DPRINTF(TPCacheDecayDebug, "BaseDP: setResetCounterOnHit\n");
    // }

  protected:

    bool _on;

  friend Base;
};

} // decay policy

} // namespace tp

} // namespace gem5

#endif // __TP_CACHE_DECAY_MECHANISM_BASE_HH__
