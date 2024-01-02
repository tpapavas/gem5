#include "tp_src/learning_gem5/part2/tp_simple_memobj.hh"

#include "debug/TPSimpleMemobj.hh"

namespace gem5
{

TPSimpleMemobj::TPSimpleMemobj(const TPSimpleMemobjParams &params) :
	SimObject(params),
	instPort(params.name + ".inst_port", this),
	dataPort(params.name + ".data_port", this),
	memPort(params.name + ".mem_side", this),
	blocked(false)
{
}

Port &
TPSimpleMemobj::getPort(const std::string &if_name, PortID idx)
{
	panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

	if (if_name == "mem_side") {
		return memPort;
	} else if (if_name == "inst_port") {
		return instPort;
	} else if (if_name == "data_port") {
		return dataPort;
	} else {
		return SimObject::getPort(if_name, idx);
	}
}

AddrRangeList
TPSimpleMemobj::CPUSidePort::getAddrRanges() const
{
	return owner->getAddrRanges();
}

void
TPSimpleMemobj::CPUSidePort::recvFunctional(PacketPtr pkt)
{
	return owner->handleFunctional(pkt);
}

bool
TPSimpleMemobj::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
	if (!owner->handleRequest(pkt)) {
		needRetry = true;
		return false;
	} else {
		return true;
	}
}

void
TPSimpleMemobj::CPUSidePort::sendPacket(PacketPtr pkt)
{
	panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

	if (!sendTimingResp(pkt)) {
		blockedPacket = pkt;
	}
}

void
TPSimpleMemobj::CPUSidePort::recvRespRetry()
{
	assert(blockedPacket != nullptr);

	PacketPtr pkt = blockedPacket;
	blockedPacket = nullptr;

	sendPacket(pkt);
}

void
TPSimpleMemobj::CPUSidePort::trySendRetry()
{
	if (needRetry && blockedPacket == nullptr) {
		needRetry = false;
		DPRINTF(TPSimpleMemobj, "Sending retry req for %d\n", id);
		sendRetryReq();
	}
}

void
TPSimpleMemobj::MemSidePort::recvRangeChange()
{
	owner->sendRangeChange();
}

void
TPSimpleMemobj::MemSidePort::sendPacket(PacketPtr pkt)
{
	panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");
	if (!sendTimingReq(pkt)) {
		blockedPacket = pkt;
	}
}

void
TPSimpleMemobj::MemSidePort::recvReqRetry()
{
	assert(blockedPacket != nullptr);

	PacketPtr pkt = blockedPacket;
	blockedPacket = nullptr;

	sendPacket(pkt);
}

bool
TPSimpleMemobj::MemSidePort::recvTimingResp(PacketPtr pkt)
{
	return owner->handleResponse(pkt);
}

void
TPSimpleMemobj::handleFunctional(PacketPtr pkt)
{
	memPort.sendFunctional(pkt);
}

AddrRangeList
TPSimpleMemobj::getAddrRanges() const
{
	DPRINTF(TPSimpleMemobj, "Sending new ranges\n");
	return memPort.getAddrRanges();
}

void
TPSimpleMemobj::sendRangeChange()
{
	instPort.sendRangeChange();
	dataPort.sendRangeChange();
}

bool
TPSimpleMemobj::handleRequest(PacketPtr pkt)
{
	if (blocked) {
		return false;
	}
	DPRINTF(TPSimpleMemobj, "Got request for addr %#x\n", pkt->getAddr());
	blocked = true;
	memPort.sendPacket(pkt);
	return true;
}

bool
TPSimpleMemobj::handleResponse(PacketPtr pkt)
{
	assert(blocked);
	DPRINTF(TPSimpleMemobj, "Got response for addr %#x\n", pkt->getAddr());
	blocked = false;

	if (pkt->req->isInstFetch()) {
		instPort.sendPacket(pkt);
	} else {
		dataPort.sendPacket(pkt);
	}

	instPort.trySendRetry();
	dataPort.trySendRetry();

	return true;
}

}
