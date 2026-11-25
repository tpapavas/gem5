/*
 * Copyright (c) 2022 Guillem Lopez Paradis
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Guillem Lopez Paradis
 */

#ifndef __DEV_ARM_NVDLA_DEVICE_HH__
#define __DEV_ARM_NVDLA_DEVICE_HH__

#include "dev/arm/base_gic.hh"
#include "dev/nvdla_device.hh"
#include "params/ArmNvDlaDevice.hh"

namespace gem5
{

/**
 * Arm FS NVDLA: the ISA-independent NvDlaDevice signalling its
 * interrupts through an ArmInterruptPin (GIC).
 */
class ArmNvDlaDevice : public NvDlaDevice
{
  public:
    ArmNvDlaDevice(const ArmNvDlaDeviceParams &params);

  protected:
    void raiseInterrupt() override { interrupt->raise(); }
    void clearInterrupt() override { interrupt->clear(); }

  private:
    ArmInterruptPin *const interrupt;
};

} // namespace gem5

#endif // __DEV_ARM_NVDLA_DEVICE_HH__
