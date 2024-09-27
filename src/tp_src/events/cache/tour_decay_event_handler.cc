#include "tp_src/events/cache/tour_decay_event_handler.hh"

#include <iostream>

#include "debug/TPCacheDecay.hh"
#include "debug/TPCacheDecayDebug.hh"
#include "debug/TPDecayPolicies.hh"
#include "mem/cache/base.hh"
#include "tour_decay_event_handler.hh"

namespace gem5
{
namespace tp
{

TourDecayEventHandler::TourDecayEventHandler(
        const TourDecayEventHandlerParams &params) :
    DecayEventHandler(params),
    dedicatedSets(params.dedicated_sets),
    dThres(params.d_threshold),
    uThres(params.u_threshold),
    scaleFactor(params.s_factor)
{
    DPRINTF(TPCacheDecay,
        "Created the DecayEventHandler object with the name %s\n"
        "TOUR_WINDOW_LIMIT: %" PRIu64"",
        name(), TOUR_WINDOW_LIMIT);

    eventType = gem5::tp::EventType::DECAY_TOUR;
}

void TourDecayEventHandler::setCache(BaseCache *_cache)
{
    DecayEventHandler::setCache(_cache);
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
