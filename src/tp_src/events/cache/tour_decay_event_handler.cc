#include "tp_src/events/cache/tour_decay_event_handler.hh"

#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "debug/TPDecayPolicies.hh"
#include "mem/cache/base.hh"
#include "tour_decay_event_handler.hh"
#include "tp_src/mem/cache/decay/dueling.hh"

namespace gem5
{
namespace tp
{

TourDecayEventHandler::TourDecayEventHandler(
        const TourDecayEventHandlerParams &params) :
    DecayEventHandler(params),
    dedicatedSets(params.dedicated_sets),
    tournamentWindow(0),
    dThres(params.d_threshold),
    uThres(params.u_threshold),
    scaleFactor(params.s_factor),
    duelingType(params.dueling_type)
{
    TW_CYCLES = Cycles(params.window_size * 9 * 128000);
    TOUR_WINDOW_LIMIT = TW_CYCLES / ticksToCycles(decayPeriod);
    DPRINTF(TPCacheDecay,
        "Created the DecayEventHandler object with the name %s\n"
        "TOUR_WINDOW_LIMIT: %" PRIu64"",
        name(), TOUR_WINDOW_LIMIT);

    eventType = gem5::tp::EventType::DECAY_TOUR;
}

void TourDecayEventHandler::setCache(BaseCache *_cache)
{
    DecayEventHandler::setCache(_cache);
    cache->getDecayDuelingMonitor()->setDuelingType(
        static_cast<DecayDuelingMonitor::DuelingType>(duelingType));
}

void
TourDecayEventHandler::processEvent()
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

void TourDecayEventHandler::retreiveParams(
        int &param1, int &param2, float &param3, float &param4)
{
    param1 = dedicatedSets;
    param2 = scaleFactor;
    param3 = dThres;
    param4 = uThres;
}

} // namespace tp
} // namespace gem5
