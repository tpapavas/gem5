#include "tp_src/events/cache/decay_event_handler.hh"

#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "debug/TPDecayPolicies.hh"
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
    calcDecayEvent(
            [this]{processCalcDecayEvent();},
            "calcDecayEvent"),
    calcDecayPeriod(
        this->cyclesToTicks(Cycles(16384))
    ),
    timesRemainingFired(0),
    timesRemainingLimit(INT_MAX),
    tournamentWindow(0)  // extra code
    // timesRemainingLimit(decayPeriod / powerOffRemainingPeriod - 1),
{
    TW_CYCLES = Cycles(params.window_size * 9 * 128000);
    TOUR_WINDOW_LIMIT = TW_CYCLES / ticksToCycles(decayPeriod);
    DPRINTF(TPCacheDecay,
        "Created the DecayEventHandler object with the name %s\n"
        "TOUR_WINDOW_LIMIT: %" PRIu64"",
        name(), TOUR_WINDOW_LIMIT);
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
    if (!cache->updateDecayAndPowerOff(decayPeriod,
            tournamentWindow, TOUR_WINDOW_LIMIT)) {
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

    if (tournamentWindow % TOUR_WINDOW_LIMIT == 0) {
        tournamentWindow = 0;

        TOUR_WINDOW_LIMIT = TW_CYCLES / ticksToCycles(decayPeriod);
        DPRINTF(TPDecayPolicies, "TOUR_WINDOW_LIMIT: %" PRIu64"\n",
            TOUR_WINDOW_LIMIT);
    }
}

void
DecayEventHandler::processPowerOffRemainingEvent()
{
    timesRemainingFired++;

    DPRINTF(TPCacheDecayDebug, "process remaining blks\n");

    bool lastTime = timesRemainingFired >= timesRemainingLimit;

    if (!cache->powerOffRemainingBlks(decayPeriod, tournamentWindow, lastTime)
        && timesRemainingFired < timesRemainingLimit) {
        schedule(powerOffRemainingEvent, curTick() + powerOffRemainingPeriod);
    } else {
        schedule(event, curTick() + decayPeriod);
    }
}

void
DecayEventHandler::processCalcDecayEvent()
{
    cache->calcDecayPercentage();
    //if (!onDecayEvent) {
        schedule(calcDecayEvent, curTick() + calcDecayPeriod);
    //}
}

void
DecayEventHandler::enable()
{
    isOn = true;
    cache->setDecayOn(true);
}

} // namespace tp
} // namespace gem5
