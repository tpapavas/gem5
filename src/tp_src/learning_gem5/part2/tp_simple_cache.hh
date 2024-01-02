#ifndef __LEARNING_GEM5_PART2_TP_SIMPLE_CACHE_HH__
#define __LEARNING_GEM5_PART2_TP_SIMPLE_CACHE_HH__

#include <unordered_map>

#include "base/statistics.hh"

#include "mem/port.hh"
#include "params/TPSimpleCache.hh"
#include "sim/clocked_object.hh"

namespace gem5
{


class TPSimpleCache : public ClockedObject
{
	private:
		class CPUSidePort : public ResponsePort
		{
			private:
				int id;

				TPSimpleCache *owner;

				bool needRetry;

				PacketPtr blockedPacket;

			public:
				CPUSidePort(const std::string& name, int id, TPSimpleCache *owner) : 
					ResponsePort(name), id(id), owner(owner), needRetry(false), blockedPacket(nullptr) 
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
				TPSimpleCache *owner;
				
				PacketPtr blockedPacket;
						
			public:
				MemSidePort(const std::string& name, TPSimpleCache *owner) : 
					RequestPort(name), owner(owner), blockedPacket(nullptr) 
				{ }

				void sendPacket(PacketPtr pkt);
				
			protected:
				bool recvTimingResp(PacketPtr pkt) override;
				void recvReqRetry() override;
				void recvRangeChange() override;
		};

		bool handleRequest(PacketPtr pkt, int port_id);

		bool handleResponse(PacketPtr pkt);

		void sendResponse(PacketPtr pkt);

		void handleFunctional(PacketPtr pkt);

		void accessTiming(PacketPtr pkt);

		bool accessFunctional(PacketPtr pkt);

		void insert(PacketPtr pkt);

		AddrRangeList getAddrRanges() const;

		void sendRangeChange();

		const Cycles latency;

		const unsigned blockSize;

		const unsigned capacity;

		std::vector<CPUSidePort> cpuPorts;

		MemSidePort memPort;
 
 		bool blocked;

		PacketPtr originalPacket;

		int waitingPortId;

		std::unordered_map<Addr, uint8_t*> cacheStore;
	
		Tick missTime;

	protected:
		struct SimpleCacheStats : public statistics::Group
		{
			SimpleCacheStats(statistics::Group *parent);
			statistics::Scalar hits;
			statistics::Scalar misses;
			statistics::Histogram missLatency;
			statistics::Formula hitRatio;
		} stats;

	public:
		TPSimpleCache(const TPSimpleCacheParams &params);

		Port &getPort(const std::string &if_name, PortID idx=InvalidPortID) override;
};

}
#endif
