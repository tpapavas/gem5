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
    timesRemainingFired(0),
    timesRemainingLimit(decayPeriod / powerOffRemainingPeriod - 1)
{
    DPRINTF(TPCacheDecay,
        "Created the IATACDecayEventHandler object with the name %s\n",
        name());
}

void
IATACDecayEventHandler::setCache(BaseCache *_cache)
{
    cache = _cache;
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
    }

    if (tillSimEnd || timesFired < numOfFires) {
        schedule(event, curTick() + decayPeriod);
    } else {
        DPRINTF(TPCacheDecay, "Done firing!\n");
    }
}

void
IATACDecayEventHandler::processPowerOffRemainingEvent()
{
    timesRemainingFired++;

    DPRINTF(TPCacheDecayDebug, "process remaining blks\n");

    if (!cache->powerOffRemainingBlks() &&
        timesRemainingFired < timesRemainingLimit) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    }
}

void
IATACDecayEventHandler::enable()
{
    isOn = true;
    cache->setDecayOn(true);
}

} // namespace tp
} // namespace gem5
