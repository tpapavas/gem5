#include "tp_src/events/cache/decay_event_handler.hh"

#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "mem/cache/base.hh"

namespace gem5
{
namespace tp
{

DecayEventHandler::DecayEventHandler(const DecayEventHandlerParams &params) :
    TimingEventHandler(params),
    decayPeriod(period),
    decayCounter(decayPeriod),
    localDecayCounter(params.local_decay_counter-1),
    isOn(params.is_on),
    powerOffRemainingEvent(
            [this]{processPowerOffRemainingEvent();},
            "powerOffRemainingEvent"),
    powerOffRemainingPeriod(
        this->cyclesToTicks(Cycles(params.post_decay_period))
    ),
    timesRemainingFired(0),
    timesRemainingLimit(decayPeriod / powerOffRemainingPeriod - 1)
{
    DPRINTF(TPCacheDecay,
        "Created the DecayEventHandler object with the name %s\n",
        name());
}

void
DecayEventHandler::setCache(BaseCache *_cache)
{
    cache = _cache;
    cache->setLocalDecayCounter(localDecayCounter);
}

void
DecayEventHandler::processEvent()
{
    if (!isOn) {
        schedule(event, curTick() + decayPeriod);
            return;
    }

    timesFired++;
    timesRemainingFired = 0;

    DPRINTF(TPCacheDecayDebug, "Processing the decay event! #%d fired\n",
        timesFired);
    if (!cache->updateDecayAndPowerOff()) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    }

    if (tillSimEnd || timesFired < numOfFires) {
        schedule(event, curTick() + decayPeriod);
    } else {
        DPRINTF(TPCacheDecay, "Done firing!\n");
    }
}

void
DecayEventHandler::processPowerOffRemainingEvent()
{
    timesRemainingFired++;

    DPRINTF(TPCacheDecayDebug, "process remaining blks\n");

    if (!cache->powerOffRemainingBlks() &&
        timesRemainingFired < timesRemainingLimit) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    }
}

void
DecayEventHandler::enable()
{
    isOn = true;
}

} // namespace tp
} // namespace gem5
