from m5.params import *
from m5.SimObject import *
from m5.objects.TPEvents import TimingEventHandler

class FlushEventHandler(TimingEventHandler):
    type = 'FlushEventHandler'
    cxx_header = "tp_src/events/cache/flush_event_handler.hh"
    cxx_class = "gem5::tp::FlushEventHandler"

    @cxxMethod
    def enable():
        pass

    writeback_on_flush = Param.Bool("whether to do writeback on flush")

    is_on = Param.Bool(True, "Whether the event is doing its thing")

class DecayEventHandler(TimingEventHandler):
    type = 'DecayEventHandler'
    cxx_header = "tp_src/events/cache/decay_event_handler.hh"
    cxx_class = "gem5::tp::DecayEventHandler"

    @cxxMethod
    def enable():
        pass

    decay_period = Param.Cycles(500, "the period (in cycles) of a decay window.")
    
    local_decay_counter = Param.Cycles(16, "the number of decay windows.")
    
    is_on = Param.Bool(True, "Whether the event is doing its thing")
