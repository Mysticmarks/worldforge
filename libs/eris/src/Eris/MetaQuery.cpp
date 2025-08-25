#include "MetaQuery.h"

#include "Metaserver.h"
#include "Connection.h"

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Encoder.h>

#include <cassert>

using namespace Atlas::Objects::Operation;
using WFMath::TimeStamp;

namespace Eris {

MetaQuery::MetaQuery(boost::asio::io_context& io_service,
					 Atlas::Bridge& bridge,
					 Meta& meta,
					 const std::string& host,
					 size_t sindex) :
		BaseConnection(io_service, "eris-metaquery", host),
		_meta(meta),
		_queryNo(0),
		m_serverIndex(sindex),
		m_complete(false),
		m_completeTimer(io_service) {
	_bridge = &bridge;
	BaseConnection::connectRemote(host, 6767);
}

// clean up is all done by the Base Connection
MetaQuery::~MetaQuery() = default;

void MetaQuery::onConnect() {
	// servers must responed to a fully anonymous GET operation
	// with pertinent info
	Get gt;
	gt->setSerialno(getNewSerialno());

	// send code from Connection
	_socket->getEncoder().streamObjectsMessage(gt);
	_socket->write();

	_stamp = TimeStamp::now();

	// save our serial-no (so we can identify replies)
	_queryNo = gt->getSerialno();

	m_completeTimer.expires_after(std::chrono::seconds(10));
	m_completeTimer.async_wait([&](boost::system::error_code ec) {
		if (!ec) {
			this->onQueryTimeout();
		}
	});

}

void MetaQuery::dispatch() {
	_meta.dispatch();
}

std::int64_t MetaQuery::getElapsed() {
        return (TimeStamp::now() - _stamp).milliseconds();
}

void MetaQuery::handleFailure(const std::string& msg) {
	_meta.queryFailure(this, msg);
}

void MetaQuery::handleTimeout(const std::string&) {
	_meta.queryTimeout(this);
}

void MetaQuery::onQueryTimeout() {
	_meta.queryTimeout(this);
}

void MetaQuery::setComplete() {
	assert(!m_complete);
	m_complete = true;
	m_completeTimer.cancel();
}

} // of namsespace
