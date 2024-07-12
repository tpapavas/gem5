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
    timesRemainingLimit(INT_MAX),
    tournamentWindow(0)  // extra code
    // timesRemainingLimit(decayPeriod / powerOffRemainingPeriod - 1)
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

    tournamentWindow++;

    timesFired++;
    timesRemainingFired = 0;

    DPRINTF(TPCacheDecayDebug, "Processing the decay event! #%d fired\n",
        timesFired);
    if (!cache->updateDecayAndPowerOff(decayPeriod, tournamentWindow)) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    } else if (tillSimEnd || timesFired < numOfFires) {
        schedule(event, curTick() + decayPeriod);
    } else {
        DPRINTF(TPCacheDecay, "Done firing!\n");
    }

    if (tournamentWindow == TOUR_WINDOW_LIMIT) {
        tournamentWindow = 0;
    }
}

void
DecayEventHandler::processPowerOffRemainingEvent()
{
    timesRemainingFired++;

    DPRINTF(TPCacheDecayDebug, "process remaining blks\n");

    if (!cache->powerOffRemainingBlks(decayPeriod, tournamentWindow) &&
        timesRemainingFired < timesRemainingLimit) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    } else {
        schedule(event, curTick() + decayPeriod);
    }
}

void
DecayEventHandler::enable()
{
    isOn = true;
}

} // namespace tp
} // namespace gem5
