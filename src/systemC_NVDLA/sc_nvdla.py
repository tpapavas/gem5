import m5
from m5.objects import *
from m5.params import *
from m5.objects.SystemC import SystemC_ScModule
from m5.objects.Tlm import TlmTargetSocket, TlmInitiatorSocket

# This class is a subclass of sc_module, and all the special magic which makes
# that work is handled in the base classes.
class TLM_Target(SystemC_ScModule):
    type = "TLM_Target"
    cxx_class = "Target"
    cxx_header = "systemC_NVDLA/sc_tlm_target.hh"
    tlm = TlmTargetSocket(32, "TLM target socket")
    tlm_i = TlmInitiatorSocket(32, "TLM initiator socket")
    system = Param.System(Parent.any, "system")


class TLM_Initiator(SystemC_ScModule):
    type = "TLM_Initiator"
    cxx_class = "Initiator"
    cxx_header = "systemC_NVDLA/sc_tlm_initiator.hh"
    tlm = TlmInitiatorSocket(32, "TLM initiator socket")
    system = Param.System(Parent.any, "system")


class TLM_ScNvDlaSE(SystemC_ScModule):
    type = "TLM_ScNvDlaSE"
    cxx_class = "ScNvDlaSE"
    cxx_header = "systemC_NVDLA/sc_nvdla_se.hh"

    cpu_freq = Param.Float(1.0, "CPU frequency in GHz")

    freq_ratio = Param.Float(1.0, "CPU/NVDLA frequency ratio")

    csb_target = TlmTargetSocket(32, "TLM NVDLA target socket")

    dbb_init = TlmInitiatorSocket(32, "Off chip memory ")
    sram_init = TlmInitiatorSocket(32, " Dedicated On-chip Cache ")

    dbb_target = TlmTargetSocket(32, "Off chip memory ")
    sram_target = TlmTargetSocket(32, " Dedicated On-chip Cache ")

    system = Param.System(Parent.any, "system")
