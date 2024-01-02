#ifndef __LEARNING_GEM5_PART2_SIMPLE_MEMOBJ_HH__
#define __LEARNING_GEM5_PART2_SIMPLE_MEMOBJ_HH__

#include "mem/port.hh"
#include "params/TPSimpleMemobj.hh"
#include "sim/sim_object.hh"

namespace gem5
{


class TPSimpleMemobj : public SimObject
{
	private:
		class CPUSidePort : public ResponsePort
		{
			private:
				TPSimpleMemobj *owner;

				bool needRetry;

				PacketPtr blockedPacket;

			public:
				CPUSidePort(const std::string& name, TPSimpleMemobj *owner) : 
					ResponsePort(name), owner(owner) 
				{ }

				void sendPacket(PacketPtr pkt);

				AddrRangeList getAddrRanges() const override;
		
				void trySendRetry();
			protected:
				Tick recvAtomic(PacketPtr pkt) override { panic("recvAtomic unimpl."); }
				void recvFunctional(PacketPtr pkt) override;
				bool recvTimingReq(PacketPtr pkt) override;
				void recvRespRetry() override;

		};

		class MemSidePort: public RequestPort
		{
			private:
				TPSimpleMemobj *owner;
				
				PacketPtr blockedPacket;
						
			public:
				MemSidePort(const std::string& name, TPSimpleMemobj *owner) : 
					RequestPort(name), owner(owner) 
				{ }

				void sendPacket(PacketPtr pkt);
				
			protected:
				bool recvTimingResp(PacketPtr pkt) override;
				void recvReqRetry() override;
				void recvRangeChange() override;
		};

		bool handleRequest(PacketPtr pkt);

		bool handleResponse(PacketPtr pkt);

		void handleFunctional(PacketPtr pkt);

		AddrRangeList getAddrRanges() const;

		void sendRangeChange();

		CPUSidePort instPort;
		CPUSidePort dataPort;

		MemSidePort memPort;
 
 		bool blocked;
	public:
		TPSimpleMemobj(const TPSimpleMemobjParams &params);

		Port &getPort(const std::string &if_name, PortID idx=InvalidPortID) override;
};

}
#endif
