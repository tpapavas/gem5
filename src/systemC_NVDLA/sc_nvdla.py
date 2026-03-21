import m5
from m5.objects import *
from m5.params import *

# class ScNVDLAModule(SystemC_ScModule):
#     type = "ScNVDLAModule"
#     cxx_class = "gem5::ScNVDLAModule"
#     cxx_header = "systemC_NVDLA/sc_nvdla.hh"

#     tlm = TlmTargetSocket(32, "TLM target socket")
#     _csb_socket = TlmTargetSocket(32, "CSB  target socket")
#     _dbb_socket = TlmInitiatorSocket(
#       32, "DBB Initiator Socket (off-chip memory)"
#     )
#     _sram_socket= TlmInitiatorSocket(
#       32, "SRAM Initiator Socket (on-chip memory)"
#     )
#     system = Param.System(Parent.any, "system")


# class ScNVDLA(SimObject):
#     type = "ScNVDLA"
#     cxx_class = "gem5::ScNVDLA"
#     cxx_header = "systemC_NVDLA/sc_nvdla.hh"

#     system = Param.System(Parent.any, "system reference")\

#     module = Param.ScNVDLAModule("SystemC module")
#     accel_port = ResponsePort("CPU -> NVDLA trigger port")

#     #csb  = Param.Gem5ToTlmBridge32(Parent.any, "CSB bridge")
#     #sram = Param.TlmToGem5Bridge32(Parent.any, "SRAM bridge")
#     #dbb  = Param.TlmToGem5Bridge32(Parent.any, "DBB bridge")

# This class is a subclass of sc_module, and all the special magic which makes
# that work is handled in the base classes.
class TLM_Target(SystemC_ScModule):
    type = "TLM_Target"
    cxx_class = "Target"
    cxx_header = "systemC_NVDLA/sc_tlm_target.hh"
    tlm = TlmTargetSocket(32, "TLM target socket")
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
    tlm = TlmTargetSocket(32, "TLM NVDLA target socket")
    system = Param.System(Parent.any, "system")
