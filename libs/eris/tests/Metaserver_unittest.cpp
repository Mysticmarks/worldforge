// Eris Online RPG Protocol Library
// Copyright (C) 2007 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

// $Id$

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include <Eris/Metaserver.h>
#include <Eris/EventService.h>
#include <Eris/WaitFreeQueue.h>

#include <Atlas/Objects/Factories.h>
#include <iostream>
#include <thread>
#include <array>
#include <atomic>
#include <boost/asio.hpp>
#include <arpa/inet.h>

#include <cassert>

using Eris::Meta;

static const std::string TEST_INVALID_IP("327.0.0.1");

class MockMetaserver {
public:
        explicit MockMetaserver(bool sendInvalid = false)
                : m_socket(m_io, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
                  m_sendInvalid(sendInvalid),
                  m_running(true),
                  m_thread([this] { serve(); }) {}

        ~MockMetaserver() {
                m_running = false;
                m_socket.close();
                if (m_thread.joinable()) {
                        m_thread.join();
                }
        }

        unsigned short port() const { return m_socket.local_endpoint().port(); }

private:
        void serve() {
                using boost::asio::ip::udp;
                udp::endpoint remote;
                std::array<char, 16> data{};
                while (m_running) {
                        boost::system::error_code ec;
                        m_socket.receive_from(boost::asio::buffer(data), remote, 0, ec);
                        if (ec) {
                                break;
                        }
                        if (m_sendInvalid) {
                                uint32_t cmd = htonl(9999);
                                m_socket.send_to(boost::asio::buffer(&cmd, sizeof(cmd)), remote, 0, ec);
                                break;
                        }
                        uint32_t handshake[2] = {htonl(3), htonl(1)}; // HANDSHAKE + stamp
                        m_socket.send_to(boost::asio::buffer(handshake), remote, 0, ec);
                        if (ec) break;
                        // clientshake
                        m_socket.receive_from(boost::asio::buffer(data), remote, 0, ec);
                        if (ec) break;
                        // list request
                        m_socket.receive_from(boost::asio::buffer(data), remote, 0, ec);
                        if (ec) break;
                        uint32_t listResp[3] = {htonl(8), htonl(0), htonl(0)}; // LIST_RESP, 0 servers
                        m_socket.send_to(boost::asio::buffer(listResp), remote, 0, ec);
                }
        }

        boost::asio::io_context m_io;
        boost::asio::ip::udp::socket m_socket;
        bool m_sendInvalid;
        std::atomic<bool> m_running;
        std::thread m_thread;
};

//static bool test_failure_flag;

//static void test_fail(const std::string & msg)
//{
//    std::cerr << "Got failure: " << msg << std::endl;
//    test_failure_flag = true;
//}

int main() {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);

        {
                MockMetaserver server;
                Meta* m = new Meta(io_service, event_service,
                                   boost::asio::ip::address_v4::loopback().to_string(), 20, server.port());
                assert(m->getGameServerCount() == 0);
                delete m;
        }

        bool test_failure_flag;
        auto test_fail = [&](const std::string &) { test_failure_flag = true; };

        // Test refreshing with normal configuration
        {
                MockMetaserver server;
                Meta m(io_service, event_service,
                        boost::asio::ip::address_v4::loopback().to_string(), 20, server.port());
                test_failure_flag = false;
                m.Failure.connect(test_fail);
                m.refresh();
                io_service.run_for(std::chrono::milliseconds(50));
                io_service.restart();
                assert(!test_failure_flag);
                assert(m.getStatus() == Meta::GETTING_LIST);
        }

        // Test refreshing with non-parsable IP fails
        {
                Meta m(io_service, event_service, TEST_INVALID_IP, 20);
                test_failure_flag = false;
                m.Failure.connect(test_fail);
                m.refresh();
                io_service.run_for(std::chrono::milliseconds(50));
                io_service.restart();
                assert(test_failure_flag);
                assert(m.getStatus() == Meta::INVALID);
        }

        // Test refreshing with normal configuration, refreshing twice
        {
                MockMetaserver server;
                Meta m(io_service, event_service,
                        boost::asio::ip::address_v4::loopback().to_string(), 20, server.port());
                test_failure_flag = false;
                m.Failure.connect(test_fail);
                m.refresh();
                io_service.run_for(std::chrono::milliseconds(50));
                io_service.restart();
                assert(!test_failure_flag);
                assert(m.getStatus() == Meta::GETTING_LIST);

                m.refresh();
                io_service.run_for(std::chrono::milliseconds(50));
                io_service.restart();
                assert(!test_failure_flag);
                assert(m.getStatus() == Meta::GETTING_LIST);
        }

        // Test mock server sending invalid response
        {
                MockMetaserver server(true);
                Meta m(io_service, event_service,
                        boost::asio::ip::address_v4::loopback().to_string(), 20, server.port());
                test_failure_flag = false;
                m.Failure.connect(test_fail);
                m.refresh();
                io_service.run_for(std::chrono::milliseconds(50));
                io_service.restart();
                assert(test_failure_flag);
                assert(m.getStatus() == Meta::INVALID);
        }

        return 0;
}

// stubs

#include <Eris/Exceptions.h>
#include <Eris/LogStream.h>
#include <Eris/MetaQuery.h>

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
}

MetaQuery::~MetaQuery() {
}

void MetaQuery::dispatch() {
}

void MetaQuery::setComplete() {
}

void MetaQuery::onConnect() {
}

void MetaQuery::handleFailure(const std::string& msg) {
}

void MetaQuery::handleTimeout(const std::string&) {
}

std::int64_t MetaQuery::getElapsed() {
        return 0LL;
}

BaseConnection::BaseConnection(boost::asio::io_context& io_service, std::string cnm,
							   std::string id) :
		_io_service(io_service),
		_status(DISCONNECTED),
		_id(id),
		_clientName(cnm),
		_bridge(nullptr),
		_host(""),
		_port(0) {
}

BaseConnection::~BaseConnection() {
}

int BaseConnection::connectRemote(const std::string& host, short port) {
	return 0;
}

int BaseConnection::connectLocal(const std::string& socket) {
	return 0;
}

void BaseConnection::onConnect() {
}

void BaseConnection::setStatus(Status sc) {
}

void ServerInfo::processServer(const Atlas::Objects::Entity::RootEntity& svr) {
}


EventService::EventService(boost::asio::io_context& io_service)
		: m_io_service(io_service), m_work(boost::asio::make_work_guard(io_service)) {}

EventService::~EventService() {
}

void EventService::runOnMainThread(std::function<void()> const&,
								   std::shared_ptr<bool> activeMarker) {
}

}
