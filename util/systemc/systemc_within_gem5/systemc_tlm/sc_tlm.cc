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

#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include <iomanip>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include "base/trace.hh"

#include "systemc/ext/systemc"
#include "systemc/ext/tlm"

#include "NV_nvdla.h"

#define N 1024

using namespace std;
using namespace sc_core;
using namespace gem5;

SC_MODULE(Initiator)
{
public:
    tlm_utils::simple_initiator_socket<Initiator> iSocket;

protected:
    int data[16];

public:
    SC_CTOR(Initiator) : iSocket("iSocket")
    {
        SC_THREAD(process);

        for (int i = 0; i < 16; i++)
        {
            data[i] = 0;
        }
    }

    void write_reg(uint32_t addr, uint32_t data)
    {
        tlm::tlm_generic_payload trans;

        trans.set_command(tlm::TLM_WRITE_COMMAND);
        trans.set_address(addr);
        trans.set_data_length(4);
        trans.set_streaming_width(4);
        trans.set_data_ptr(reinterpret_cast<unsigned char *>(&data));

        sc_time delay = sc_time(10, SC_NS);

        iSocket->b_transport(trans, delay);
    }

protected:
    void process()
    {
        wait(100, SC_NS);

        std::cout << "Starting minimal convolution\n";

        /* interrupt setup */
        //write_reg(0x9004, 0x1);

        //std::cout << "CDMA started\n";

    }
};

SC_MODULE(Target)
{
public:
    tlm_utils::simple_target_socket<Target> tSocket;

private:
    unsigned char mem[512];

public:
    SC_HAS_PROCESS(Target);
    Target(sc_module_name name, unsigned int bufferSize = 8) : sc_module(name),
                                                            tSocket("tSocket")
    {
        tSocket.register_b_transport(this, &Target::b_transport);
    }

    virtual void b_transport(tlm::tlm_generic_payload & trans,
                             sc_time & delay)
    {
        tlm::tlm_command cmd = trans.get_command();
        uint64_t addr = trans.get_address();

        if (cmd == tlm::TLM_READ_COMMAND)
            std::cout << "[DBB READ] addr=" << addr << std::endl;

        if (cmd == tlm::TLM_WRITE_COMMAND)
            std::cout << "[DBB WRITE] addr=" << addr << std::endl;
        executeTransaction(trans);
    }

    // Common to b_transport and nb_transport
    void executeTransaction(tlm::tlm_generic_payload & trans)
    {
        tlm::tlm_command cmd = trans.get_command();
        sc_dt::uint64 adr = trans.get_address();
        unsigned char *ptr = trans.get_data_ptr();
        unsigned int len = trans.get_data_length();
        unsigned char *byt = trans.get_byte_enable_ptr();
        unsigned int wid = trans.get_streaming_width();

        if (trans.get_address() >= 512)
        {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
            return;
        }
        if (byt != 0)
        {
            trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
            return;
        }
        if (len > 4 || wid < len)
        {
            trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
            return;
        }

        if (cmd == tlm::TLM_READ_COMMAND)
        {
            memcpy(&mem[trans.get_address()], // destination
                   trans.get_data_ptr(),      // source
                   trans.get_data_length());  // size
        }
        else if (cmd == tlm::TLM_WRITE_COMMAND)
        {
            memcpy(trans.get_data_ptr(),      // destination
                   &mem[trans.get_address()], // source
                   trans.get_data_length());  // size
        }

        cout << "\033[1;32m("
             << name()
             << ")@" << setfill(' ') << setw(12) << sc_time_stamp()
             << ": " << setw(12) << (cmd ? "Exec. Write " : "Exec. Read ")
             << "Addr = " << setfill('0') << setw(8) << dec << adr
             << " Data = " << "0x" << setfill('0') << setw(8) << hex
             << *reinterpret_cast<int *>(ptr)
             << "\033[0m" << endl;

        trans.set_response_status(tlm::TLM_OK_RESPONSE);
    }
};

template <unsigned int I, unsigned int T>
SC_MODULE(Interconnect)
{
public:
    tlm_utils::simple_target_socket_tagged<Interconnect> tSocket[T];
    tlm_utils::simple_initiator_socket_tagged<Interconnect> iSocket[I];

    SC_CTOR(Interconnect)
    {
        for (unsigned int i = 0; i < T; i++)
        {
            tSocket[i].register_b_transport(this,
                                            &Interconnect::b_transport,
                                            i);
        }
    }

private:
    int routeFW(int inPort, tlm::tlm_generic_payload &trans)
    {
        int outPort = 0;

        // Memory map implementation:
        if (trans.get_address() < 512)
        {
            outPort = 0;
        }
        else if (trans.get_address() >= 512 && trans.get_address() < 1024)
        {
            // Correct Address:
            trans.set_address(trans.get_address() - 512);
            outPort = 1;
        }
        else
        {
            trans.set_response_status(tlm::TLM_ADDRESS_ERROR_RESPONSE);
        }

        return outPort;
    }

    virtual void b_transport(int id,
                             tlm::tlm_generic_payload &trans,
                             sc_time &delay)
    {
        sc_assert(id < T);
        int outPort = routeFW(id, trans);
        iSocket[outPort]->b_transport(trans, delay);
    }
};

SC_MODULE(nvdlaTOP)
{
public:
    Initiator cpu;
    Target mem_dbb;
    Target mem_sram;

    sc_core::sc_signal<bool> irq_signal;
    SC_CTOR(nvdlaTOP)
        : cpu("CPU"),
          mem_dbb("DBB_MEM"),
          mem_sram("SRAM_MEM")
    {

        auto nvdla = new scsim::cmod::NV_nvdla("nvdla_test");

        // CSB
        cpu.iSocket.bind(nvdla->nvdla_host_master_if);

        // DBB memory
        nvdla->nvdla_core2dbb_axi4.bind(mem_dbb.tSocket);

        // SRAM memory
        nvdla->nvdla_core2cvsram_axi4.bind(mem_sram.tSocket);

        // IRQ binding (IMPORTANT)
        nvdla->nvdla_intr(irq_signal);
        std::cout << "[TOP] NVDLA connected\n";
    }
};

int sc_main(int __attribute__((unused)) sc_argc,
            char __attribute__((unused)) * sc_argv[])
{
    nvdlaTOP top("top");

    sc_start();
    top.cpu.write_reg(0x955555,0x111);
    top.irq_signal.write(true);
    return 0;
}
/*
int
sc_main (int __attribute__((unused)) sc_argc,
             char __attribute__((unused)) *sc_argv[])
{

    Initiator * cpu1   = new Initiator("C1");
    Initiator * cpu2   = new Initiator("C2");

    Target * memory1   = new Target("M1");
    Target * memory2   = new Target("M2");

    Interconnect<2,2> * bus = new Interconnect<2,2>("B1");

    cpu1->iSocket.bind(bus->tSocket[0]);
    cpu2->iSocket.bind(bus->tSocket[1]);
    bus->iSocket[0].bind(memory1->tSocket);
    bus->iSocket[1].bind(memory2->tSocket);

    sc_core::sc_start();

    return 0;
}
*/
