# Copyright (c) 2012-2016,2019-2020 ARM Limited
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
#
# Copyright (c) 2005-2007 The Regents of The University of Michigan
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from m5.params import *
from m5.proxy import *
from m5.util.fdthelper import *

from m5.objects.Device import BasicPioDevice


class NvDlaDevice(BasicPioDevice):
    type = "NvDlaDevice"
    cxx_header = "dev/arm/nvdla_device.hh"
    cxx_class = "gem5::NvDlaDevice"

    pio_size = Param.Addr(0x8, "Size of address range")
    interrupt = Param.ArmInterruptPin("Interrupt to use for this device")

    # new glue stuff
    trace_mode = Param.Bool(False, "Use register trace")

    # rtlObject stuff
    enableRTLObject = Param.Bool(True, "Enable RTL Object")

    enableWaveform = Param.Bool(False, "Enable Trace Waveform")

    system = Param.System(Parent.any, "System this accelerator belongs to")

    # rtlNVDLA stuff
    cpu_side = ResponsePort("CPU side port, receives requests")
    cmd_cpu_side = ResponsePort("CPU side port, receives cmd requests")
    mem_side = RequestPort("Memory side port, sends requests")
    sram_port = RequestPort("High Speed port to SRAM, sends requests")
    dram_port = RequestPort("Regular Speed to DRAM, sends requests")
    dma_port = RequestPort("DMA port to DRAM")

    freq_ratio = Param.Float(
        1, "=(frequency of LITTLE CPU) / (frequency of NVDLA)"
    )

    buffer_mode = Param.UInt32(
        0,
        "How to use pr/sh cache/embedded-SPM. all(0): cache all; "
        "pft(1): prefetch-buffer-only; pft-cut(2): prefetch buffer with throttling",
    )

    dma_enable = Param.UInt32(0, "Whether to use DMA in testing")

    use_shared_spm = Param.Bool(
        False, "Whether to use shared spm among NVDLAs"
    )

    spm_size = Param.MemorySize("64kB", "The size of the embedded SPM")

    spm_latency = Param.UInt32(
        2, "Latency for NVDLA private scratchpad memory"
    )

    spm_line_size = Param.UInt32(
        1024, "The minimal granularity to copy data from memory to SPM"
    )

    prefetch_enable = Param.UInt32(
        0,
        "Whether to issue software prefetch when inflight read queue is under-fed",
    )

    pft_threshold = Param.UInt32(
        16,
        "the threshold of current inflight memory requests to launch software prefetch",
    )

    assoc = Param.String("full", "The associativity of the embedded buffer")

    id_nvdla = Param.UInt64(0, "id of the NVDLA")

    maxReq = Param.UInt64(4, "Max Request inflight for NVDLA")

    base_addr_dram = Param.UInt64(0xA0000000, "")

    base_addr_sram = Param.UInt64(0xB0000000, "")

    enableTimingAXI = Param.Bool(False, "Enable Timing mode in AXI")

    use_fake_mem = Param.Bool(False, "Whether to use fake memory to simulate")

    print_path = Param.String("", "The path to store output logs of NVDLA")


class NvDlaDeviceSE(BasicPioDevice):
    type = "NvDlaDeviceSE"
    cxx_header = "dev/arm/nvdla_device_se.hh"
    cxx_class = "gem5::NvDlaDeviceSE"

    pio_size = Param.Addr(0x8, "Size of address range")

    # new glue stuff
    trace_mode = Param.Bool(False, "Use register trace")

    # rtlObject stuff
    enableRTLObject = Param.Bool(True, "Enable RTL Object")

    enableWaveform = Param.Bool(False, "Enable Trace Waveform")

    system = Param.System(Parent.any, "System this accelerator belongs to")
    cpu = Param.BaseCPU(Parent.any, "System this accelerator belongs to")

    # rtlNVDLA stuff
    cpu_side = ResponsePort("CPU side port, receives requests")
    cmd_cpu_side = ResponsePort("CPU side port, receives cmd requests")
    mem_side = RequestPort("Memory side port, sends requests")
    sram_port = RequestPort("High Speed port to SRAM, sends requests")
    dram_port = RequestPort("Regular Speed to DRAM, sends requests")
    dma_port = RequestPort("DMA port to DRAM")

    freq_ratio = Param.Float(
        1, "=(frequency of LITTLE CPU) / (frequency of NVDLA)"
    )

    buffer_mode = Param.UInt32(
        0,
        "How to use pr/sh cache/embedded-SPM. all(0): cache all; "
        "pft(1): prefetch-buffer-only; pft-cut(2): prefetch buffer with throttling",
    )

    dma_enable = Param.UInt32(0, "Whether to use DMA in testing")

    use_shared_spm = Param.Bool(
        False, "Whether to use shared spm among NVDLAs"
    )

    spm_size = Param.MemorySize("64kB", "The size of the embedded SPM")

    spm_latency = Param.UInt32(
        2, "Latency for NVDLA private scratchpad memory"
    )

    spm_line_size = Param.UInt32(
        1024, "The minimal granularity to copy data from memory to SPM"
    )

    prefetch_enable = Param.UInt32(
        0,
        "Whether to issue software prefetch when inflight read queue is under-fed",
    )

    pft_threshold = Param.UInt32(
        16,
        "the threshold of current inflight memory requests to launch software prefetch",
    )

    assoc = Param.String("full", "The associativity of the embedded buffer")

    id_nvdla = Param.UInt64(0, "id of the NVDLA")

    maxReq = Param.UInt64(4, "Max Request inflight for NVDLA")

    base_addr_dram = Param.UInt64(0xA0000000, "")

    base_addr_sram = Param.UInt64(0xB0000000, "")

    enableTimingAXI = Param.Bool(False, "Enable Timing mode in AXI")

    use_fake_mem = Param.Bool(False, "Whether to use fake memory to simulate")

    print_path = Param.String("", "The path to store output logs of NVDLA")
