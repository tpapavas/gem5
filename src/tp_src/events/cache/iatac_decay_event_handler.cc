#include "tp_src/events/cache/iatac_decay_event_handler.hh"

#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "mem/cache/base.hh"

namespace gem5
{
namespace tp
{

IATACDecayEventHandler::IATACDecayEventHandler(
        const IATACDecayEventHandlerParams &params) :
    TimingEventHandler(params),
    decayPeriod(period),
    isOn(params.is_on),
    powerOffRemainingEvent(
            [this]{processPowerOffRemainingEvent();},
            "powerOffRemainingEvent"),
    powerOffRemainingPeriod(
        this->cyclesToTicks(Cycles(params.post_decay_period))
    ),
    calcDecayEvent(
            [this]{processCalcDecayEvent();},
            "calcDecayEvent"),
    calcDecayPeriod(
        this->cyclesToTicks(Cycles(16384))
    ),
    timesRemainingFired(0),
    timesRemainingLimit(INT_MAX),
    globalCounter(params.init_global_counter),
    initDecay(params.init_local_counter),
    letOverflow(params.let_overflow),
    resetCounterOnHit(params.reset_on_decay_hit)
    // timesRemainingLimit(decayPeriod / powerOffRemainingPeriod - 1),
{
    DPRINTF(TPCacheDecay,
        "Created the IATACDecayEventHandler object with the name %s\n",
        name());
}

void
IATACDecayEventHandler::setCache(BaseCache *_cache)
{
    cache = _cache;

    //// extra code ////
    // cache->getIATACdata()->setGlobal(globalCounter);
    // cache->setIATACInitDecay(initDecay);

    cache->setIATACdata(globalCounter, initDecay,
        letOverflow, resetCounterOnHit);
    //// eof extra code ////
}

void
IATACDecayEventHandler::processEvent()
{
    if (!isOn) {
        schedule(event, curTick() + decayPeriod);
            return;
    }

    timesFired++;
    timesRemainingFired = 0;

    DPRINTF(TPCacheDecayDebug, "Processing the decay event! #%d fired\n",
        timesFired);

    if (!cache->iatacUpdateDecay()) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    } else if (tillSimEnd || timesFired < numOfFires) {
        schedule(event, curTick() + decayPeriod);
    } else {
        DPRINTF(TPCacheDecay, "Done firing!\n");
        return;
    }

    if (!calcDecayEvent.scheduled()) {
        schedule(calcDecayEvent, curTick() + calcDecayPeriod);
    }
}

void
IATACDecayEventHandler::processPowerOffRemainingEvent()
{
    timesRemainingFired++;

    DPRINTF(TPCacheDecayDebug, "process remaining blks\n");

    bool lastTime = timesRemainingFired >= timesRemainingLimit;

    if (!cache->iatacPowerOffRemainingBlks(lastTime) &&
        timesRemainingFired < timesRemainingLimit) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    } else {
        schedule(event, curTick() + decayPeriod);
    }
}

void
IATACDecayEventHandler::processCalcDecayEvent()
{
    cache->calcIATACDecayPercentage();
    schedule(calcDecayEvent, curTick() + calcDecayPeriod);
}

void
IATACDecayEventHandler::enable()
{
    isOn = true;
    cache->setDecayOn(true);
}

} // namespace tp
} // namespace gem5
