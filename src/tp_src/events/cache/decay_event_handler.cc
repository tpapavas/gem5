#include "tp_src/events/cache/decay_event_handler.hh"
#include "mem/cache/base.hh"

#include "debug/TPCacheDecay.hh"

#include <iostream>

namespace gem5 
{
namespace tp 
{

DecayEventHandler::DecayEventHandler(const DecayEventHandlerParams &params) :
    TimingEventHandler(params),
    decayPeriod(period),
    decayCounter(decayPeriod),
    localDecayCounter(params.local_decay_counter-1),
    isOn(params.is_on)
{
	DPRINTF(TPCacheDecay, "Created the DecayEventHandler object with the name %s\n", name());
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
	//DPRINTF(TPCacheDecay, "Processing the decay event! #%d fired\n", timesFired);
	// do decay stuff
    //cache->decreaseDecayCounter();
    //decayCounter--;
    //if (decayCounter < 0) {
        //cache->resetDecayCounter();
      //  decayCounter = decayPeriod;
        
        // check every block if dead && dirty
    cache->updateDecayAndPowerOff();
    //}
    // end of decay stuff
	
    if (tillSimEnd || timesFired < numOfFires) {
		schedule(event, curTick() + decayPeriod);	
	} else {
		DPRINTF(TPCacheDecay, "Done firing!\n");
	}
}

void
DecayEventHandler::enable()
{
    isOn = true;
}

} // namespace tp
} // namespace gem5
