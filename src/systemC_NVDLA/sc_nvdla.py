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

    nvdla_id = Param.UInt32(0, "NVDLA instance id")

    csb_target = TlmTargetSocket(32, "TLM NVDLA target socket")

    dbb_init = TlmInitiatorSocket(32, "Off chip memory ")
    sram_init = TlmInitiatorSocket(32, " Dedicated On-chip Cache ")

    dbb_target = TlmTargetSocket(32, "Off chip memory ")
    sram_target = TlmTargetSocket(32, " Dedicated On-chip Cache ")

    system = Param.System(Parent.any, "system")

    # Embedded buffer (SPM) and weight prefetcher, as the Verilator NVDLA
    # (rtlNVDLA / NvDlaDeviceSE); see sc_embedded_buffer.hh
    dma_enable = Param.Bool(
        False, "Put the embedded buffer between the DBB port and memory"
    )
    spm_size = Param.MemorySize("64kB", "The size of the embedded buffer")
    spm_line_size = Param.UInt32(
        1024, "The minimal granularity to copy data from memory to the buffer"
    )
    spm_latency = Param.UInt32(
        2, "Latency (NVDLA cycles) of a DBB read beat served by the buffer"
    )
    assoc = Param.String(
        "full", "The associativity of the embedded buffer (full or ways)"
    )
    use_shared_spm = Param.Bool(
        False, "One embedded buffer shared by all NVDLAs"
    )
    buffer_mode = Param.UInt32(
        0,
        "How to use the embedded buffer. all(0): cache all; "
        "pft(1): prefetch-buffer-only; pft-cut(2): prefetch buffer with "
        "throttling",
    )
    prefetch_enable = Param.Bool(
        False, "Prefetch the weight tensors of the programmed CDMA ops"
    )
    pft_threshold = Param.UInt32(
        16,
        "Kept for the Verilator options; not used by the SystemC model "
        "(its prefetcher fetches one line per demand DBB transaction)",
    )
    mem_fill_chunk = Param.UInt32(
        64,
        "Line fills and write-backs go to memory in chunks of this size "
        "(gem5 cache line)",
    )
