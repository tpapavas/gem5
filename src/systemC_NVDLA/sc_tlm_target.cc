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

#include <tlm_utils/simple_target_socket.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include "base/trace.hh"
#include "params/TLM_Target.hh"
#include "sc_tlm_target.hh"

//#include "systemc/ext/systemc"
#include "systemc.h"
#include "systemc/ext/tlm"

using namespace std;
using namespace sc_core;
using namespace gem5;

void Target::b_transport(tlm::tlm_generic_payload& trans,
                            sc_time& delay)
{
    std::cout << "[NVDLA -> DRAM] addr=0x"
              << std::hex << trans.get_address()
              << " cmd=" << (trans.is_read() ? "READ" : "WRITE")
              << std::endl;

    if(!iSocket.get_interface()){
        std::cout << "[Error] no interface isocket";
    }
    iSocket->b_transport(trans, delay);
    std::cout << "after tSocket->b_transport(trans, delay);\n ";


    //uint32_t data = *reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    //std::cout << "[NVDLA -> DRAM] data are : " << std::hex << data << "\n";
    //std::cout << " isocket size are " << iSocket.size() << "\n";
    //iSocket->b_transport(trans, delay);

}



Target *
gem5::TLM_TargetParams::create() const
{
    Target *target = new Target(name.c_str());
    return target;
}

gem5::Port
&Target::gem5_getPort(const std::string &if_name, int idx)
{
    if (if_name == "tlm_i"){
        std::cout << " [gem5::Port &Target::gem5_getPort] i_wrapper\n";
        return i_wrapper;
    }

    return wrapper;

}
