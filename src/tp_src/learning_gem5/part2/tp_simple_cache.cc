#include "tp_src/learning_gem5/part2/tp_simple_cache.hh"

#include "base/compiler.hh"
#include "base/random.hh"
#include "debug/TPSimpleCache.hh"
#include "sim/system.hh"

namespace gem5
{

TPSimpleCache::TPSimpleCache(const TPSimpleCacheParams &params) :
	ClockedObject(params),
	latency(params.latency),
	blockSize(params.system->cacheLineSize()),
	capacity(params.size / blockSize),
	memPort(params.name + ".mem_side", this),
	blocked(false), originalPacket(nullptr), waitingPortId(-1), stats(this)
{
	for (int i = 0; i < params.port_cpu_side_connection_count; ++i) {
		cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
	}
}

Port &
TPSimpleCache::getPort(const std::string &if_name, PortID idx)
{
	if (if_name == "mem_side") {
		panic_if(idx != InvalidPortID, "Mem side of simple cache not a vector port");
		return memPort;
	} else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
		return cpuPorts[idx];
	} else {
		return ClockedObject::getPort(if_name, idx);
	}
}

AddrRangeList
TPSimpleCache::CPUSidePort::getAddrRanges() const
{
	return owner->getAddrRanges();
}

void
TPSimpleCache::CPUSidePort::recvFunctional(PacketPtr pkt)
{
	return owner->handleFunctional(pkt);
}

bool
TPSimpleCache::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
	DPRINTF(TPSimpleCache, "Got request %s\n", pkt->print());

	if (blockedPacket || needRetry) {
		DPRINTF(TPSimpleCache, "Request blocked\n");
		needRetry = true;
		return false;
	}
	if (!owner->handleRequest(pkt, id)) {
		DPRINTF(TPSimpleCache, "Request failed\n");
		needRetry = true;
		return false;
	} else {
		DPRINTF(TPSimpleCache, "Request succeeded\n");
		return true;
	}
}

void
TPSimpleCache::CPUSidePort::sendPacket(PacketPtr pkt)
{
	panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

	if (!sendTimingResp(pkt)) {
		blockedPacket = pkt;
	}
}

void
TPSimpleCache::CPUSidePort::recvRespRetry()
{
	assert(blockedPacket != nullptr);

	PacketPtr pkt = blockedPacket;
	blockedPacket = nullptr;

	DPRINTF(TPSimpleCache, "Retrying response pkt %s\n", pkt->print());

	sendPacket(pkt);

	trySendRetry();
}

void
TPSimpleCache::CPUSidePort::trySendRetry()
{
	if (needRetry && blockedPacket == nullptr) {
		needRetry = false;
		DPRINTF(TPSimpleCache, "Sending retry req for %d\n", id);
		sendRetryReq();
	}
}

void
TPSimpleCache::MemSidePort::recvRangeChange()
{
	owner->sendRangeChange();
}

void
TPSimpleCache::MemSidePort::sendPacket(PacketPtr pkt)
{
	panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");
	if (!sendTimingReq(pkt)) {
		blockedPacket = pkt;
	}
}

void
TPSimpleCache::MemSidePort::recvReqRetry()
{
	assert(blockedPacket != nullptr);

	PacketPtr pkt = blockedPacket;
	blockedPacket = nullptr;

	sendPacket(pkt);
}

bool
TPSimpleCache::MemSidePort::recvTimingResp(PacketPtr pkt)
{
	return owner->handleResponse(pkt);
}

void
TPSimpleCache::handleFunctional(PacketPtr pkt)
{
	if (accessFunctional(pkt)) {
		pkt->makeResponse();
	} else {
		memPort.sendFunctional(pkt);
	}
}

AddrRangeList
TPSimpleCache::getAddrRanges() const
{
	DPRINTF(TPSimpleCache, "Sending new ranges\n");
	return memPort.getAddrRanges();
}

void
TPSimpleCache::sendRangeChange()
{
	for (auto& port : cpuPorts) {
		port.sendRangeChange();
	}
}

bool
TPSimpleCache::handleRequest(PacketPtr pkt, int port_id)
{
	if (blocked) {
		return false;
	}
	DPRINTF(TPSimpleCache, "Got request for addr %#x\n", pkt->getAddr());
	blocked = true;

	assert(waitingPortId == -1);
	waitingPortId = port_id;

	schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt) ;}, name() + ".accessEvent", true), clockEdge(latency));

	return true;
}

bool
TPSimpleCache::handleResponse(PacketPtr pkt)
{
	assert(blocked);
	DPRINTF(TPSimpleCache, "Got response for addr %#x\n", pkt->getAddr());
	insert(pkt);
	
	stats.missLatency.sample(curTick() - missTime);

	if (originalPacket != nullptr) {
		[[maybe_unused]] bool hit = accessFunctional(originalPacket);
		panic_if(!hit, "Should always hit after inserting");
		originalPacket->makeResponse();
		delete pkt;
		pkt = originalPacket;
		originalPacket = nullptr;
	}

	sendResponse(pkt);

	return true;
}

void
TPSimpleCache::sendResponse(PacketPtr pkt)
{
	assert(blocked);

	DPRINTF(TPSimpleCache, "Sending resp for addr %#x\n", pkt->getAddr());

	int port = waitingPortId;

	blocked = false;
	waitingPortId = -1;

	cpuPorts[port].sendPacket(pkt);

	for (auto& port : cpuPorts) {
		port.trySendRetry();
	}
}

void
TPSimpleCache::accessTiming(PacketPtr pkt)
{
	bool hit = accessFunctional(pkt);

	DPRINTF(TPSimpleCache, "$s for packet: %s\n", hit ? "hit" : "miss", pkt->print());

	if (hit) {
		stats.hits++; // update stats
		pkt->makeResponse();
		sendResponse(pkt);
	} else {
		stats.misses++; // update stats
		missTime = curTick();
		Addr addr = pkt->getAddr();
		Addr block_addr = pkt->getBlockAddr(blockSize);
		unsigned size = pkt->getSize();
		if (addr == block_addr && size == blockSize) {
			DPRINTF(TPSimpleCache, "forwarding packet\n");
			memPort.sendPacket(pkt);
		} else {
			DPRINTF(TPSimpleCache, "Upgrading packet to block size\n");
			panic_if(addr - block_addr + size > blockSize, "Cannot handle accesses that span multiple cache lines");

			assert(pkt->needsResponse());
			MemCmd cmd;
			if (pkt->isWrite() || pkt->isRead()) {
				cmd = MemCmd::ReadReq;
			} else {
				panic("Unknown packet type in upgrade size");
			}

			PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
			new_pkt->allocate();

			assert(new_pkt->getAddr() == new_pkt->getBlockAddr(blockSize));

			originalPacket = pkt;

			DPRINTF(TPSimpleCache, "forwording packet\n");
			memPort.sendPacket(new_pkt);
		}
	}
}

bool
TPSimpleCache::accessFunctional(PacketPtr pkt)
{
	Addr block_addr = pkt->getBlockAddr(blockSize);
	auto it = cacheStore.find(block_addr);
	if (it != cacheStore.end()) {
		if (pkt->isWrite()) {
			pkt->writeDataToBlock(it->second, blockSize);
		} else if (pkt->isRead()) {
			pkt->setDataFromBlock(it->second, blockSize);
		} else {
			panic("Unknown packet type!");
		}
		return true;
	}

	return false;
}

void
TPSimpleCache::insert(PacketPtr pkt)
{
	if (cacheStore.size() >= capacity) {
		int bucket, bucket_size;
		do {
			bucket = random_mt.random(0, (int)cacheStore.bucket_count() - 1);
		} while ( (bucket_size = cacheStore.bucket_size(bucket)) == 0 );
		auto block = std::next(cacheStore.begin(bucket), random_mt.random(0, bucket_size - 1));
		DPRINTF(TPSimpleCache, "Removing addr %#x\n", block->first);

		RequestPtr req = std::make_shared<Request>(block->first, blockSize, 0, 0);
		PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
		new_pkt->dataDynamic(block->second);
		
		DPRINTF(TPSimpleCache, "Writing packet back %s\n", pkt->print());
		memPort.sendTimingReq(new_pkt);

		cacheStore.erase(block->first);
	}
	uint8_t *data = new uint8_t[blockSize];
	cacheStore[pkt->getAddr()] = data;
}

TPSimpleCache::SimpleCacheStats::SimpleCacheStats(statistics::Group *parent) : 
	statistics::Group(parent),
	ADD_STAT(hits, statistics::units::Count::get(), "Number of hits"),
	ADD_STAT(misses, statistics::units::Count::get(), "Number of misses"),
	ADD_STAT(missLatency, statistics::units::Tick::get(), "Ticks for misses to the cache"),
	ADD_STAT(hitRatio, statistics::units::Ratio::get(), "The ratio of hits to the total accesses to the cache", hits / (hits + misses))
{
	missLatency.init(16); // number of buckets
}

}
