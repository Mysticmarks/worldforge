#include "MetaQuery.h"

#include "Metaserver.h"

#include "ServerInfo.h"
#include "Log.h"
#include "EventService.h"
#include "Exceptions.h"

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/RootEntity.h>

#include <algorithm>

#include <cassert>
#include <cstring>
#include <memory>
#include <utility>

#ifdef _WIN32

#ifndef snprintf
#define snprintf _snprintf
#endif

#endif // _WIN32

using namespace Atlas::Objects::Operation;
using Atlas::Objects::smart_dynamic_cast;
using Atlas::Objects::Root;
using Atlas::Objects::Entity::RootEntity;

namespace Eris {

char* pack_uint32(uint32_t data, char* buffer, unsigned int& size);

char* unpack_uint32(uint32_t& dest, char* buffer);


// meta-server protocol commands	
const uint32_t CKEEP_ALIVE = 2,
		HANDSHAKE = 3,
		CLIENTSHAKE = 5,
		LIST_REQ = 7,
		LIST_RESP = 8,
		PROTO_ERANGE = 9,
		LAST = 10;

// special  command value to track LIST_RESP processing
const uint32_t LIST_RESP2 = 999;

struct MetaDecoder : Atlas::Objects::ObjectsDecoder {
	Meta& m_meta;

	MetaDecoder(Meta& meta, const Atlas::Objects::Factories& factories) :
			ObjectsDecoder(factories), m_meta(meta) {
	}

	void objectArrived(Root obj) override {
		m_meta.objectArrived(std::move(obj));
	}
};

Meta::Meta(boost::asio::io_context& io_service,
                   EventService& eventService,
                   std::string metaServer,
                   unsigned int maxQueries,
                   unsigned short metaServerPort) :
                m_factories(new Atlas::Objects::Factories()),
                m_io_service(io_service),
                m_event_service(eventService),
                m_decoder(new MetaDecoder(*this, *m_factories)),
                m_status(INVALID),
                m_metaHost(std::move(metaServer)),
                m_metaPort(std::to_string(metaServerPort)),
                m_maxActiveQueries(maxQueries),
                m_nextQuery(0),
                m_resolver(io_service),
                m_socket(io_service),
		m_metaTimer(io_service),
		m_receive_stream(&m_receive_buffer),
		m_send_buffer(new boost::asio::streambuf()),
		m_send_stream(m_send_buffer.get()),
		m_data{},
		m_dataPtr(nullptr),
		m_bytesToRecv(0),
		m_totalServers(0),
		m_packed(0),
		m_recvCmd(false),
		m_gotCmd(0) {
	unsigned int max_half_open = FD_SETSIZE;
	if (m_maxActiveQueries > (max_half_open - 2)) {
		m_maxActiveQueries = max_half_open - 2;
	}
}

Meta::~Meta() {
	disconnect();
}

/*
void Meta::queryServer(const std::string &ip)
{
    m_status = QUERYING;
    
    if (m_activeQueries.size() < m_maxActiveQueries)
    {
        MetaQuery *q =  new MetaQuery(this, ip);
        if (q->isComplete())
        {
            // indicated a failure occurred, so we'll kill it now and say no more
            delete q;
        } else
            m_activeQueries.insert(q);
    }
}
*/

void Meta::queryServerByIndex(size_t index) {
	if (m_status == INVALID) {
		logger->error("called queryServerByIndex with invalid server list");
		return;
	}

	if (index >= m_gameServers.size()) {
		logger->error("called queryServerByIndex with bad server index {}", index);
		return;
	}

	if (m_gameServers[index].status == ServerInfo::QUERYING) {
		logger->warn("called queryServerByIndex on server already being queried");
		return;
	}

	internalQuery(index);
}

void Meta::refresh() {
	if (!m_activeQueries.empty()) {
		logger->warn("called meta::refresh() while doing another query, ignoring");
		return;
	}

	if (m_status == VALID) {
		// save the current list in case we fail
		m_lastValidList = m_gameServers;
	}

	m_gameServers.clear();
	m_nextQuery = 0;
	disconnect();
	connect();
}

void Meta::cancel() {
	m_activeQueries.clear();

	disconnect();

	// revert to the last valid list if possible
	if (!m_lastValidList.empty()) {
		m_gameServers = m_lastValidList;
		m_status = VALID;
	} else {
		m_status = INVALID;
		m_gameServers.clear();
	}
	m_nextQuery = m_gameServers.size();
}

const ServerInfo& Meta::getInfoForServer(size_t index) const {
	if (index >= m_gameServers.size()) {
		logger->error("passed out-of-range index {} to getInfoForServer", index);
		throw BaseException("Out of bounds exception when getting server info.");
	} else {
		return m_gameServers[index];
	}
}

size_t Meta::getGameServerCount() const {
	return m_gameServers.size();
}

void Meta::connect() {
        m_resolver.async_resolve(m_metaHost, m_metaPort,
                                                         [&](const boost::system::error_code& ec, boost::asio::ip::udp::resolver::results_type iterator) {
								 if (!ec && !iterator.empty()) {
									 this->connect(*iterator.begin());
								 } else {
									 this->disconnect();
								 }
							 });
}

void Meta::connect(const boost::asio::ip::udp::endpoint& endpoint) {
	m_socket.open(boost::asio::ip::udp::v4());
	m_socket.async_connect(endpoint, [&](boost::system::error_code ec) {
		if (!ec) {
			do_read();

			// build the initial 'ping' and send
			unsigned int dsz = 0;
			pack_uint32(CKEEP_ALIVE, m_data.data(), dsz);
			this->m_send_stream << std::string(m_data.data(), dsz) << std::flush;
			this->write();
			this->setupRecvCmd();

			this->m_status = GETTING_LIST;
			this->startTimeout();
		} else {
			this->doFailure("Couldn't open connection to metaserver " + this->m_metaHost);
		}
	});
}

void Meta::disconnect() {
	if (m_socket.is_open()) {
		m_socket.close();
	}
	m_metaTimer.cancel();
}

void Meta::startTimeout() {
	m_metaTimer.cancel();
	m_metaTimer.expires_after(std::chrono::seconds(8));
	m_metaTimer.async_wait([&](boost::system::error_code ec) {
		if (!ec) {
			this->metaTimeout();
		}
	});
}


void Meta::do_read() {
	if (m_socket.is_open()) {
		m_socket.async_receive(m_receive_buffer.prepare(DATA_BUFFER_SIZE),
							   [this](boost::system::error_code ec, std::size_t length) {
								   if (!ec) {
									   m_receive_buffer.commit(length);
									   if (length > 0) {
										   this->gotData();
									   }
									   this->write();
									   this->do_read();
								   } else {
									   if (ec != boost::asio::error::operation_aborted) {
										   this->doFailure(std::string("Connection to the meta-server failed: ") + ec.message());
									   }
								   }
							   });
	}
}

void Meta::write() {
	if (m_socket.is_open()) {
		if (m_send_buffer->size() != 0) {
			std::shared_ptr<boost::asio::streambuf> send_buffer(std::move(m_send_buffer));
			m_send_buffer = std::make_unique<boost::asio::streambuf>();
			m_send_stream.rdbuf(m_send_buffer.get());
			m_socket.async_send(send_buffer->data(),
								[&, send_buffer](boost::system::error_code ec, std::size_t length) {
									if (!ec) {
										send_buffer->consume(length);
									} else {
										if (ec != boost::asio::error::operation_aborted) {
											this->doFailure(std::string("Connection to the meta-server failed: ") + ec.message());
										}
									}
								});
		}
	}
}

void Meta::gotData() {
	recv();
}

void Meta::deleteQuery(MetaQuery* query) {
	auto I = std::find_if(m_activeQueries.begin(), m_activeQueries.end(), [&](const std::unique_ptr<MetaQuery>& entry) { return entry.get() == query; });

	if (I != m_activeQueries.end()) {
		auto containedQuery = I->release();
		m_activeQueries.erase(I);

		//Delay destruction.
		m_event_service.runOnMainThread([containedQuery]() {
			delete containedQuery;
		});

		if (m_activeQueries.empty() && m_nextQuery == m_gameServers.size()) {
			m_status = VALID;
			// we're all done, emit the signal
			AllQueriesDone.emit();
		}
	} else {
		logger->error("Tried to delete meta server query which wasn't "
					  "among the active queries. This indicates an error "
					  "with the flow in Metaserver.");
	}
}

void Meta::recv() {
	if (m_bytesToRecv == 0) {
		logger->error("No bytes to receive when calling recv().");
		return;
	}

	m_receive_stream.peek();
	std::streambuf* iobuf = m_receive_stream.rdbuf();
	std::streamsize len = std::min(m_bytesToRecv, iobuf->in_avail());
	if (len > 0) {
		iobuf->sgetn(m_dataPtr, len);
		m_bytesToRecv -= len;
		m_dataPtr += len;
	}
//	do {
//		int d = m_stream.get();
//		*(m_dataPtr++) = static_cast<char>(d);
//		m_bytesToRecv--;
//	} while (iobuf->in_avail() && m_bytesToRecv);

	if (m_bytesToRecv > 0) {
		logger->error("Fragment data received by Meta::recv");
		return; // can't do anything till we get more data
	}

	if (m_recvCmd) {
		uint32_t op;
		unpack_uint32(op, m_data.data());
		recvCmd(op);
	} else {
		processCmd();
	}

	// try and read more
	if (m_bytesToRecv && m_receive_stream.rdbuf()->in_avail())
		recv();
}

void Meta::recvCmd(uint32_t op) {
	switch (op) {
		case HANDSHAKE:
			setupRecvData(1, HANDSHAKE);
			break;

		case PROTO_ERANGE:
			doFailure("Got list range error from Metaserver");
			break;

		case LIST_RESP:
			setupRecvData(2, LIST_RESP);
			break;

		default:
			doFailure("Unknown Meta server command");
			break;
	}
}

void Meta::processCmd() {
	if (m_status != GETTING_LIST) {
		logger->error("Command received when not expecting any. It will be ignored. The command was: {}", m_gotCmd);
		return;
	}

	switch (m_gotCmd) {
		case HANDSHAKE: {
			uint32_t stamp;
			unpack_uint32(stamp, m_data.data());

			unsigned int dsz = 0;
			m_dataPtr = pack_uint32(CLIENTSHAKE, m_data.data(), dsz);
			pack_uint32(stamp, m_dataPtr, dsz);

			m_send_stream << std::string(m_data.data(), dsz) << std::flush;
			write();

			m_metaTimer.cancel();
			// send the initial list request
			listReq(0);
		}
			break;

		case LIST_RESP: {
			//uint32_t m_totalServers, m_packed;
			uint32_t total_servers;
			m_dataPtr = unpack_uint32(total_servers, m_data.data());
			if (!m_gameServers.empty()) {
				if (total_servers != m_totalServers) {
					logger->warn("Server total in new packet has changed. {}:{}", total_servers, m_totalServers);
				}
			} else {
				m_totalServers = total_servers;
			}
			unpack_uint32(m_packed, m_dataPtr);
			// FIXME This assumes that the data received so far is all the servers, which
			// in the case of fragmented server list it is not. Currently this code is generally
			// of the size of packet receieved. As there should only ever be one packet incoming
			// we should be able to make assumptions based on the amount of data in the buffer.
			// The buffer should also contain a complete packet if it contains any, so retrieving
			// data one byte at a time is less efficient than it might be.
			setupRecvData(m_packed, LIST_RESP2);

			// If this is the first response, allocate the space
			if (m_gameServers.empty()) {

				assert(m_nextQuery == 0);
				m_gameServers.reserve(m_totalServers);
			}
		}
			break;

		case LIST_RESP2: {
			m_dataPtr = m_data.data();
			while (m_packed--) {
				uint32_t ip;
				m_dataPtr = unpack_uint32(ip, m_dataPtr);
				auto ipAsString = boost::asio::ip::address_v4(ip).to_string();

				// FIXME  - decide whether a reverse name lookup is necessary here or not
				m_gameServers.emplace_back(ServerInfo{ipAsString});
			}

			if (m_gameServers.size() < m_totalServers) {
				// request some more
				listReq((unsigned int) m_gameServers.size());
			} else {
				// allow progress bars to setup, etc, etc
				CompletedServerList.emit(m_totalServers);
				m_status = QUERYING;
				// all done, clean up
				disconnect();
			}
			query();

		}
			break;

		default:
			std::stringstream ss;
			ss << "Unknown Meta server command: " << m_gotCmd;
			doFailure(ss.str());
			break;
	}
}

void Meta::listReq(unsigned int base) {
	unsigned int dsz = 0;
	char* _dataPtr = pack_uint32(LIST_REQ, m_data.data(), dsz);
	pack_uint32(base, _dataPtr, dsz);

	m_send_stream << std::string(m_data.data(), dsz) << std::flush;
	write();
	setupRecvCmd();

	startTimeout();
}

void Meta::setupRecvCmd() {
	m_recvCmd = true;
	m_bytesToRecv = sizeof(uint32_t);
	m_dataPtr = m_data.data();
}

void Meta::setupRecvData(int words, uint32_t got) {
	m_recvCmd = false;
	m_bytesToRecv = words * sizeof(uint32_t);
	m_dataPtr = m_data.data();
	m_gotCmd = got;
}

/* pack the data into the specified buffer, update the buffer size, and return
the new buffer insert pointer */

char* pack_uint32(uint32_t data, char* buffer, unsigned int& size) {
	uint32_t netorder;

	netorder = htonl(data);
	memcpy(buffer, &netorder, sizeof(uint32_t));
	size += sizeof(uint32_t);
	return buffer + sizeof(uint32_t);
}

/* unpack one data from the buffer, and return the next extract pointer */

char* unpack_uint32(uint32_t& dest, char* buffer) {
	uint32_t netorder;

	memcpy(&netorder, buffer, sizeof(uint32_t));
	dest = ntohl(netorder);
	return buffer + sizeof(uint32_t);
}

void Meta::internalQuery(size_t index) {
	assert(index < m_gameServers.size());

	ServerInfo& sv = m_gameServers[index];
	auto q = std::make_unique<MetaQuery>(m_io_service, *m_decoder, *this, sv.host, index);
	if (q->getStatus() != BaseConnection::CONNECTING &&
		q->getStatus() != BaseConnection::NEGOTIATE) {
		// indicates a failure occurred, so we'll kill it now and say no more
		sv.status = ServerInfo::INVALID;
	} else {
		m_activeQueries.emplace_back(std::move(q));
		sv.status = ServerInfo::QUERYING;
	}
}

void Meta::objectArrived(Root obj) {
	Info info = smart_dynamic_cast<Info>(obj);
	if (!info.isValid()) {
		logger->error("Meta::objectArrived, failed to convert object to INFO op");
		return;
	}

// work out which query this is
	auto refno = info->getRefno();
	QuerySet::iterator Q;

	for (Q = m_activeQueries.begin(); Q != m_activeQueries.end(); ++Q)
		if ((*Q)->getQueryNo() == refno) break;

	if (Q == m_activeQueries.end()) {
		logger->error("Couldn't locate query for meta-query reply");
	} else {
		(*Q)->setComplete();

		RootEntity svr = smart_dynamic_cast<RootEntity>(info->getArgs().front());
		if (!svr.isValid()) {
			logger->error("Query INFO argument object is broken");
		} else {
			if ((*Q)->getServerIndex() >= m_gameServers.size()) {
				logger->error("Got server info with out of bounds index.");
			} else {
				ServerInfo& sv = m_gameServers[(*Q)->getServerIndex()];

				sv.processServer(svr);
				sv.ping = (int) (*Q)->getElapsed();

				// emit the signal
				ReceivedServerInfo.emit(sv);
			}
		}
		deleteQuery(Q->get());
	}
	query();
}

void Meta::doFailure(const std::string& msg) {
	Failure.emit(msg);
	cancel();
}

void Meta::dispatch() {

}

void Meta::metaTimeout() {
	// cancel calls disconnect, which will kill upfront without this
	m_metaTimer.cancel();

	// might want different behaviour in the future, I suppose
	doFailure("Connection to the meta-server timed out");
}

void Meta::queryFailure(MetaQuery* q, const std::string&) {
	// we do NOT emit a failure signal here (because that would probably cause the
	// host app to pop up a dialog or something) since query failures are likely to
	// be very frequent.
	m_gameServers[q->getServerIndex()].status = ServerInfo::INVALID;
	q->setComplete();
	deleteQuery(q);
	query();
}

void Meta::query() {
	while ((m_activeQueries.size() < m_maxActiveQueries) && (m_nextQuery < m_gameServers.size())) {
		internalQuery(m_nextQuery++);
	}
}

void Meta::queryTimeout(MetaQuery* q) {
	m_gameServers[q->getServerIndex()].status = ServerInfo::TIMEOUT;
	deleteQuery(q);
	query();
}

} // of Eris namespace

