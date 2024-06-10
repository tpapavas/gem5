#ifndef __TP_DECAY_MECHANISM_IATAC_HH__
#define __TP_DECAY_MECHANISM_IATAC_HH__

//#include "params/IATAC.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

namespace tp
{

class IATAC;

class IATACdata
{
  public:
    IATACdata();

    bool isOn() { return _on; }
    void setOn(bool onoff) { _on = onoff;}

    void updateMaxGlobals(int);

    void checkAcumOverflow(int);

    void setGlobal(int val);
    void setGlobal(int i, int val) { _globalDecay[i] = val; }

    //// extra code ////
    int getInitLocalDecay() { return _initLocalDecay; }
    void setInitLocalDecay(int init_local_decay)
    {
      _initLocalDecay = init_local_decay;
    }

    bool doLetOverflow() { return _letOverflow; }
    void setLetOverflow(bool letOverflow)
    {
      _letOverflow = letOverflow;
    }

    bool doResetCounterOnHit() { return _resetCounterOnHit; }
    void setResetCounterOnHit(bool reset_counter_on_hit)
    {
      _resetCounterOnHit = reset_counter_on_hit;
    }
    //// eof extra code ////

    void printGlobals();

  private:
    static const int _MAX_ACCESS = 32;

    static const int _MIN_COUNT = 8;
    static const int _MAX_COUNT = 1024;

    int _acumcounter[_MAX_ACCESS];
    int _globalDecay[_MAX_ACCESS];
    int _maxGlobalDecay[_MAX_ACCESS];

    bool _isMax[_MAX_ACCESS];

    bool _on;

    //// extra code ////
    int _initLocalDecay = 8192;

    bool _letOverflow = false;

    bool _resetCounterOnHit = false;
    //// eof extra code ////

  friend IATAC;
};

class IATAC
{
  public:
    IATAC();

    void handleHit(IATACdata*);
    void handleMiss(IATACdata*);

    bool decayElapsed() { return (_elapsed >= _decay) && _onoff; }
    // static void printGlobals();

    void updateDecay();
    int getDecay() { return _decay; }

    int getElapsed() { return _elapsed; }

    int getCounter() { return _counter; }

    bool getWrong() { return _wrongBit; }

    void setPower(bool onoff) { _onoff = onoff; }
    bool isPoweredOff() const { return !_onoff; }

    //// extra code ////
    void setDecay(int decay_interval) { _decay = decay_interval; }

    void setLetOverflow(bool let_overflow) { _letOverflow =let_overflow; }

    bool doResetCounterOnDecayedHit() { return _resetCounterOnHit; }
    void setResetCounterOnHit(bool reset_counter_on_hit)
    {
      _resetCounterOnHit = reset_counter_on_hit;
    }

    void resetDecayCounter() { _counter = 1; }
    //// eof extra code ////

    std::string print() const;

  private:
    static const int _MAX_ACCESS = 32;

    // static int _acumcounter[];
    // static int _globalDecay[];
    // static int _maxGlobalDecay[];

    // static bool _first_iatac_obj;

    /** If the block was prematurely turned off. */
    bool _wrongBit = false;

    /** if the block is on or off */
    bool _onoff = true;

    /** decay interval that has to elapse. */
    int _decay = 8192;

    /** max interaccess interval. */
    int _thits = 0;

    /** ticks elapsed since last access. */
    int _elapsed = 0;

    /** access counter */
    int _counter = 1;

    bool _accessOverflow = false;

    //// extra code ////
    bool _letOverflow = false;

    bool _resetCounterOnHit = false;
    //// eof extra code ////
    // void _setupGlobalStructs();
};

} // namespace tp

} // namespace gem5

#endif // __TP_DECAY_MECHANISM_IATAC_HH__
