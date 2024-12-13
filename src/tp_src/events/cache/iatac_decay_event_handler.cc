#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "mem/cache/base.hh"
#include "tp_src/events/cache/decay_event_handler.hh"

namespace gem5
{
namespace tp
{

IATACDecayEventHandler::IATACDecayEventHandler(
        const IATACDecayEventHandlerParams &params) :
    DecayEventHandler(params),
    globalCounter(params.init_global_counter),
    initDecay(params.init_local_counter),
    letOverflow(params.let_overflow),
    resetCounterOnHit(params.reset_on_decay_hit)
    // timesRemainingLimit(decayPeriod / powerOffRemainingPeriod - 1),
    // timesRemainingLimit(INT_MAX),
{
    DPRINTF(TPCacheDecay,
        "Created the IATACDecayEventHandler object with the name %s\n",
        name());

    eventType = tp::EventType::DECAY_IATAC;
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

} // namespace tp
} // namespace gem5
