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

    void setGlobal(int i, int val) { _globalDecay[i] = val; }

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
    bool isPoweredOff() { return !_onoff; }

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

    // void _setupGlobalStructs();
};

} // namespace tp

} // namespace gem5

#endif // __TP_DECAY_MECHANISM_IATAC_HH__
