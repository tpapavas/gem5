#ifndef __LEARNING_GEM5_FLUSH_EVENT_HANDLER_HH__
#define __LEARNING_GEM5_FLUSH_EVENT_HANDLER_HH__

#include "params/FlushEventHandler.hh"
#include "sim/sim_object.hh"
#include "mem/cache/base.hh" 
#include "tp_src/events/timing_event_handler.hh"

namespace gem5
{
namespace tp 
{

class FlushEventHandler : public TimingEventHandler 
{
	private:
		void processEvent() override;		
		
		bool writebackOnFlush;

		BaseCache *cache;

		bool isOn;
	public:
    	FlushEventHandler(const FlushEventHandlerParams &p);

		void setCache(BaseCache *_cache);

		void enable();
};

} // namespace tp

} // namespace gem5

#endif // __LEARNING_GEM5_FLUSH_EVENT_HANDLER_HH__
