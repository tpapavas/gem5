from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject

class UCIeLink(SimObject):
    type = 'UCIeLink'
    cxx_header = 'dev/ucie/ucie_link.hh'
    cxx_class = 'gem5::UCIeLink'

    cpu_side = ResponsePort('Upstream device connection')
    mem_side = RequestPort('Downstream interconnect connection')
    config_file = Param.String('', 'Absolute YAML config path')
    delay_unit_ps = Param.Bool(True, 'Treat model raw delay units as ps (verify with model author)')
    apply_response_delay = Param.Bool(True, 'Also model UCIe delay on response path')
    queue_size = Param.Unsigned(16, 'Max packets buffered/in flight per direction')
    header_bits = Param.Unsigned(64, 'Header/flit overhead bits per packet (data-less packets cost only this)')

    # ---- task13: flit-accurate mode (MUG5 ASICON'23 / JETCAS'25) ----
    link_mode = Param.String('packet',
        "'packet' (task12 behaviour), 'flit256_std' (256B flit, 236B "
        "payload) or 'flit68' (68B flit, 64B payload), 'flit256_leandros' "
        "(task15: flit256_std packing, flit timing from the Leandros "
        "get_delay() model of config_file; lanes/gtps/datapath_bits/"
        "phy_model ignored)")
    flit_payload_bytes = Param.Unsigned(0,
        'Override payload bytes per flit (0 = format default). '
        'flit_payload_bytes=256 with flit256_std = paper analytic grid '
        '(overhead not occupying stream positions)')
    lanes = Param.Unsigned(16, 'PHY lanes')
    gtps = Param.Float(4.0, 'GT/s per lane')
    datapath_bits = Param.Unsigned(256, 'Adapter datapath width (bits/cycle)')
    tlp_header_bytes = Param.Unsigned(16, 'Protocol (TLP) header bytes per packet')
    phy_model = Param.String('leandros',
        "PHY latency added after flit accumulation: 'leandros' (constant "
        "part of get_delay), 'spec' (spec_adapter_ps+spec_phy_ps+NN "
        "channel delay), 'none' (0)")
    spec_adapter_ps = Param.Unsigned(1000, 'spec phy_model: D2D adapter + logical PHY latency (ps)')
    spec_phy_ps = Param.Unsigned(1000, 'spec phy_model: electrical PHY latency (ps)')
    ber = Param.Float(0.0, 'Bit error rate (per flit error prob 1-(1-BER)^flitBits)')
    ber_deterministic = Param.Bool(False,
        'One flit error every ceil(1/P_flit) transmissions (JETCAS style) instead of RNG')
    retry_buffer_flits = Param.Unsigned(64, 'Retry buffer depth in flits (<=255, 8-bit seq)')
    rng_seed = Param.UInt64(1, 'Seed for flit error RNG')
    csb_sync = Param.Bool(False,
        'Synchronous CSB register path: forward immediately, add the link '
        'round trip (req + resp, same model) to pkt->headerDelay so the '
        'CPU can stall for it')
