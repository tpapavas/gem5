#ifndef __LEARNING_GEM5_TP_HELLO_OBJECT_HH__
#define __LEARNING_GEM5_TP_HELLO_OBJECT_HH__

#include "tp_src/learning_gem5/part2/tp_goodbye_object.hh"
#include "params/TPHelloObject.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class TPHelloObject : public SimObject {
	private:
		void processEvent();		
		
		EventFunctionWrapper event;

		TPGoodbyeObject* goodbye;

		const std::string myName;

		const Tick latency;

		int timesLeft;
	public:
    	TPHelloObject(const TPHelloObjectParams &p);

		void startup() override;
};

} // namespace gem5

#endif // __LEARNING_GEM5_TP_HELLO_OBJECT_HH__
