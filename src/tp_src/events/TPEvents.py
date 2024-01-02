from m5.params import *
from m5.objects.ClockedObject import ClockedObject

class TimingEventHandler(ClockedObject):
	type = 'TimingEventHandler'
	cxx_header = "tp_src/events/timing_event_handler.hh"
	cxx_class = "gem5::tp::TimingEventHandler"

	period = Param.Cycles("Time between firing events")
	number_of_fires = Param.Int(1, "Number of times to fire the event")
	run_till_sim_end = Param.Bool(False, "Whether the event will run until simulation finishes")
	start_delay = Param.Int(0, "Time before activating the event")
