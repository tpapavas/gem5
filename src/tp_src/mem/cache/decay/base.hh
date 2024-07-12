#ifndef __TP_CACHE_DECAY_MECHANISM_BASE_HH__
#define __TP_CACHE_DECAY_MECHANISM_BASE_HH__

#include <memory>
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

    int getCounter() { return _counter; }

    void setPower(bool onoff) { _onoff = onoff; }
    bool isPoweredOff() const { return !_onoff; }

    virtual std::string print() const;

    virtual DecayDueler* getDecayDueler() { return nullptr; }

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

  protected:

    bool _on;

  friend Base;
};

} // decay policy

} // namespace tp

} // namespace gem5

#endif // __TP_CACHE_DECAY_MECHANISM_BASE_HH__
