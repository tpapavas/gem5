/*
 * Copyright 2022 Fraunhofer IESE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __SYSTEC_TLM_INITIATOR_EXAMPLE__
#define __SYSTEC_TLM_INITIATOR_EXAMPLE__

#include <tlm_utils/simple_initiator_socket.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include "NV_nvdla.h"
#include "base/trace.hh"
#include "systemc/ext/core/sc_module_name.hh"
#include "systemc/tlm_port_wrapper.hh"

//#include "systemc/ext/systemc"
#include "systemc.h"

#include "systemc/ext/tlm"

using namespace std;
using namespace sc_core;
using namespace gem5;

#define GLB_BASE 0x0
#define GEC_BASE 0x1000
#define MCIF_BASE 0x2000
#define CVIF_BASE 0x3000
#define BDMA_BASE 0x4000
#define CDMA_BASE 0x5000
#define CSC_BASE 0x6000
#define CMAC_A_BASE 0x7000
#define CMAC_B_BASE 0x8000
#define CACC_BASE 0x9000
#define SDP_RDMA_BASE 0xa000
#define SDP_BASE 0xb000
#define PDP_RDMA_BASE 0xc000
#define PDP_BASE 0xd000
#define CDP_RDMA_BASE 0xe000
#define CDP_BASE 0xf000
#define RBK_BASE 0x10000

SC_MODULE(Initiator)
{
public:
  tlm_utils::simple_initiator_socket<Initiator> tSocket;
  sc_gem5::TlmInitiatorWrapper<32> wrapper;

private:
  // unsigned char mem[512];
  //unsigned char *mem;

public:
  SC_HAS_PROCESS(Initiator);
  Initiator(sc_module_name name) : sc_module(name),
                                   tSocket("tSocket"),
                                   wrapper(tSocket, std::string(name) + ".tlm",
                                   InvalidPortID)
  {
    //mem = (unsigned char *)malloc(16 * 1024 * 1024);

    std::cout << "TLM Initiator Online" << std::endl;
  }

  gem5::Port &gem5_getPort(const std::string &if_name, int idx = -1) override;

  void write_reg(uint32_t addr, unsigned char *data)
  {
    tlm::tlm_generic_payload trans;

    trans.set_command(tlm::TLM_WRITE_COMMAND);
    trans.set_address(addr);
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_data_ptr(data);

    sc_time delay = SC_ZERO_TIME;

    tSocket->b_transport(trans, delay);
  }

  uint32_t read_reg(uint32_t addr)
  {
    tlm::tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;

    uint32_t data = 0;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);

    tSocket->b_transport(trans, delay);
    std::cout << "read_reg value are : " << std::hex << data << "\n";

    return data;
  }

  /*
  uint32_t read_reg_fw(uint32_t addr)
  {
    tlm::tlm_generic_payload trans;
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    uint32_t data = 0;

    trans.set_command(tlm::TLM_READ_COMMAND);
    trans.set_address(addr);
    trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

    std::cout << "[TLM READ SEND] addr=0x"
              << std::hex << addr
              << " time=" << sc_core::sc_time_stamp()
              << std::endl;

    tlm::tlm_sync_enum status = tSocket->nb_transport_fw(trans, phase, delay);

    if (status == tlm::TLM_COMPLETED)
    {
      std::cout << "[TLM READ COMPLETED] data=0x"
                << std::hex << data
                << std::endl;
    }

    if (trans.is_response_error())
    {
      SC_REPORT_ERROR("TLM-2", "Response error from nb_transport_fw");
    }

    std::cout << "read_reg value are : " << std::hex << data << std::endl;

    return data;
  }
    */
};

#endif // __SYSTEC_TLM_INITIATOR_EXAMPLE__
