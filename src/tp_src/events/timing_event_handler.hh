#ifndef __LEARNING_GEM5_TIMING_EVENT_HANDLER_HH__
#define __LEARNING_GEM5_TIMING_EVENT_HANDLER_HH__

#include "params/TimingEventHandler.hh"
#include "sim/clocked_object.hh"

namespace gem5
{
namespace tp 
{

class TimingEventHandler : public ClockedObject 
{
	protected:
		EventFunctionWrapper event;

		const Tick period;

		int numOfFires, timesFired;
	
		bool tillSimEnd;

		int startDelay;

		virtual void processEvent();	
	public:
    	TimingEventHandler(const TimingEventHandlerParams &p);

		void startup() override;
};

} // namespace tp

} // namespace gem5

#endif // __LEARNING_GEM5_TIMING_EVENT_HANDLER_HH__
