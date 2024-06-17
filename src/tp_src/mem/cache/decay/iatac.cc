#include "tp_src/mem/cache/decay/iatac.hh"

#include "debug/TPCacheDecay.hh"

namespace gem5
{

namespace tp
{

// bool IATAC::isOn() {
//     return _mechOn;
// }

// void IATAC::setOn() {
//     _mechOn = true;
// }

// bool IATAC::_mechOn = false;
// bool IATAC::_first_iatac_obj = true;

// int IATAC::_acumcounter[_MAX_ACCESS];
// int IATAC::_globalDecay[_MAX_ACCESS];
// int IATAC::_maxGlobalDecay[_MAX_ACCESS];

IATAC::IATAC()
    : BaseDecay()
{
    // if (_first_iatac_obj) {
    //     _setupGlobalStructs();
    // }
}

void
IATAC::updateDecay()
{
    if (_elapsed < 8192 && !_accessOverflow) {
        _elapsed++;
    }
    // DPRINTF(TPCacheDecay,
    //      "updateDecay; decay: %d elapsed: %d\n", _decay, _elapsed);
}

void
IATAC::handleHit(IATACdata *iatac)
{
    // DPRINTF(TPCacheDecay,
    //      "On hit; counter: %d elapsed: %d\n", _counter, _elapsed);
    if (!_letOverflow && _counter >= _MAX_ACCESS - 1) {
        _accessOverflow = true;
        _wrongBit = true;
        return;
    }

    _counter++;

    if (_elapsed > _thits) {
        _thits = _elapsed;
    }

    if (!_onoff) {
        // must not be capable of being decayed until replacement.
        _wrongBit = true;
    }

    _decay = iatac->_maxGlobalDecay[_counter+1];
}

void
IATAC::handleMiss(IATACdata *iatac)
{
    if (!_accessOverflow) {
        // DPRINTF(TPCacheDecay, "on miss\n");
        // DPRINTF(TPCacheDecay, "counter: %d, thits: %d\n",
        //           _counter, _thits);
        if (_thits > iatac->_globalDecay[_counter]) {
            DPRINTF(TPCacheDecay, "thits greater than global\n");
            iatac->_globalDecay[_counter] =
                iatac->_globalDecay[_counter] << 1; // x2
            // iatac->setGlobal(_counter, iatac->_globalDecay[_counter] << 1);
            iatac->updateMaxGlobals(_counter);
        } else if (_thits * 2 < iatac->_globalDecay[_counter]) {
            DPRINTF(TPCacheDecay, "thits smaller than global/2\n");
            iatac->_globalDecay[_counter] = iatac->_globalDecay[_counter] > 1 ?
                iatac->_globalDecay[_counter] >> 1 : 1; // /2
            // iatac->setGlobal(_counter, iatac->_globalDecay[_counter] >> 1);
            iatac->updateMaxGlobals(_counter);
        }

        iatac->_acumcounter[_counter]++;
        iatac->checkAcumOverflow(_counter);

        _decay = iatac->_maxGlobalDecay[_counter+1];
    }

    _onoff = true;
    _wrongBit = false;
    _counter = 1;
    _thits = 0;
    _elapsed = 0;

    _accessOverflow = false;
}

// void
// IATAC::_setupGlobalStructs()
// {
//     for (int i = 0; i < _MAX_ACCESS; i++) {
//         _acumcounter[i] = 0;
//         _globalDecay[i] = 8192;
//         _maxGlobalDecay[i] = 8192;
//     }

//     _first_iatac_obj = false;
// }

std::string
IATAC::print() const
{
    return csprintf("thits: %d elapsed: %d counter: %d ",
        _thits, _elapsed, _counter);
}

void
IATACdata::printGlobals()
{
    printf("Acumcounter:\t");
    for (int i = 0; i < _MAX_ACCESS; i++) {
        printf("%6d ", _acumcounter[i]);
    }
    printf("\n");

    printf("Globaldecay:\t");
    for (int i = 0; i < _MAX_ACCESS; i++) {
        printf("%6d ", _globalDecay[i]);
    }
    printf("\n");

    printf("Maxglobaldecay:\t");
    for (int i = 0; i < _MAX_ACCESS; i++) {
        printf("%6d ", _maxGlobalDecay[i]);
    }
    printf("\n");
}

IATACdata::IATACdata()
{
    for (int i = 0; i < _MAX_ACCESS; i++) {
        _acumcounter[i] = 0;
        _globalDecay[i] = 1;
        _maxGlobalDecay[i] = 1;

        _isMax[i] = false;
    }
}

void
IATACdata::updateMaxGlobals(int p)
{
    // DPRINTF(TPCacheDecay, "on updateGlobals\n");
    for (int i = p; i > 0; i--) {
        if (_acumcounter[i] > _MIN_COUNT
            && _globalDecay[i] >= _maxGlobalDecay[i]) {
            _maxGlobalDecay[i-1] = _globalDecay[i];
            _isMax[i] = true; // it is max for at least some P.
        } else {
            _maxGlobalDecay[i-1] = _maxGlobalDecay[i];
        }
    }
}

void
IATACdata::checkAcumOverflow(int p)
{
    if (_acumcounter[p] >= _MAX_COUNT) {
        for (int i = 0; i < _MAX_ACCESS; i++) {
            _acumcounter[i] = _acumcounter[i] >> 1;
        }
        updateMaxGlobals(_MAX_ACCESS-1); // update all maxima.
    }
}

void
IATACdata::setGlobal(int val)
{
    for (int i = 0; i < _MAX_ACCESS; i++) {
        setGlobal(i, val);
    }
}

} // namespace tp

} // namespace gem5
