from m5.params import *
from m5.SimObject import *
from m5.objects.TPEvents import TimingEventHandler


class FlushEventHandler(TimingEventHandler):
    type = "FlushEventHandler"
    cxx_header = "tp_src/events/cache/flush_event_handler.hh"
    cxx_class = "gem5::tp::FlushEventHandler"

    @cxxMethod
    def enable():
        pass

    writeback_on_flush = Param.Bool("whether to do writeback on flush")

    is_on = Param.Bool(True, "Whether the event is doing its thing")


class DecayEventHandler(TimingEventHandler):
    type = "DecayEventHandler"
    cxx_header = "tp_src/events/cache/decay_event_handler.hh"
    cxx_class = "gem5::tp::DecayEventHandler"

    @cxxMethod
    def enable():
        pass

    decay_period = Param.Cycles(
        500, "the period (in cycles) of a decay window."
    )

    post_decay_period = Param.Cycles(
        100,
        "the period (in cycles) for trying to decay blocks "
        "not decayed on time.",
    )

    local_decay_counter = Param.Cycles(16, "the number of decay windows.")

    is_on = Param.Bool(True, "Whether the event is doing its thing")


class IATACDecayEventHandler(TimingEventHandler):
    type = "IATACDecayEventHandler"
    cxx_header = "tp_src/events/cache/iatac_decay_event_handler.hh"
    cxx_class = "gem5::tp::IATACDecayEventHandler"

    @cxxMethod
    def enable():
        pass

    decay_period = Param.Cycles(
        1000, "the period (in cycles) of a decay window."
    )

    post_decay_period = Param.Cycles(
        100,
        "the period (in cycles) for trying to decay blocks "
        "not decayed on time.",
    )

    is_on = Param.Bool(True, "Whether the event is doing its thing")

    init_global_counter = Param.Int(1, "Initial value for global counters.")

    init_local_counter = Param.Int(8192, "Initial value for local counter.")

    let_overflow = Param.Bool(
        False, "Whether we let counters increment after MAX_ACCESSES reached"
    )

    reset_on_decay_hit = Param.Bool(
        False,
        "Whether we reset local counter of a decayed block when it gets hit.",
    )
