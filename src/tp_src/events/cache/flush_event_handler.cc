#include "tp_src/events/cache/flush_event_handler.hh"
#include "mem/cache/base.hh"

#include "debug/TPHello.hh"

#include <iostream>

namespace gem5 
{
namespace tp 
{

FlushEventHandler::FlushEventHandler(const FlushEventHandlerParams &params) :
    TimingEventHandler(params),
	writebackOnFlush(params.writeback_on_flush),
	isOn(params.is_on)
{
	DPRINTF(TPHello, "Created the FlushEventHandler object with the name %s\n", name());
}

void
FlushEventHandler::setCache(BaseCache *_cache)
{
	cache = _cache;
}

void 
FlushEventHandler::processEvent() 
{
	if (!isOn) {
		schedule(event, curTick() + period);	
		return;
	}

	timesFired++;
	DPRINTF(TPHello, "Hello world! Processing the flush event! #%d fired\n", timesFired);
	cache->flush(writebackOnFlush);
	if (tillSimEnd || timesFired < numOfFires) {
		schedule(event, curTick() + period);	
	} else {
		DPRINTF(TPHello, "Done firing!\n");
	}
}

void
FlushEventHandler::enable()
{
	isOn = true;
}

} // namespace tp
} // namespace gem5
