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

#include <cassert>

using Eris::Meta;

static const std::string TEST_METASERVER("127.0.0.1");
static const std::string TEST_INVALID_IP("327.0.0.1");

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
		Meta* m = new Meta(io_service, event_service, TEST_METASERVER, 20);

		assert(m->getGameServerCount() == 0);

		delete m;
	}

//    // Test refreshing with normal configuration
//    {
//        Meta * m = new Meta(io_service, TEST_METASERVER, 20);
//
//        test_failure_flag = false;
//
//        m->Failure.connect(sigc::ptr_fun(&test_fail));
//        m->refresh();
//
//        assert(!test_failure_flag);
//        assert(m->getStatus() == Meta::GETTING_LIST);
//
//        delete m;
//    }

	// Test refreshing with non-parsable IP fails
//    {
//        Meta * m = new Meta(io_service, TEST_INVALID_IP, 20);
//
//        test_failure_flag = false;
//
//        m->Failure.connect(sigc::ptr_fun(&test_fail));
//        m->refresh();
//
//        assert(test_failure_flag);
//        assert(m->getStatus() == Meta::INVALID);
//
//        delete m;
//    }

	// Test refreshing with normal configuration, refreshing twice
//    {
//        Meta * m = new Meta(io_service, TEST_METASERVER, 20);
//
//        test_failure_flag = false;
//
//        m->Failure.connect(sigc::ptr_fun(&test_fail));
//        m->refresh();
//
//        assert(!test_failure_flag);
//        assert(m->getStatus() == Meta::GETTING_LIST);
//
//        m->refresh();
//
//        assert(!test_failure_flag);
//        assert(m->getStatus() == Meta::GETTING_LIST);
//
//        delete m;
//    }

	// Test hitting poll does nothing before refresh
//    {
//        Meta * m = new Meta(io_service, TEST_METASERVER, 20);
//
//        test_failure_flag = false;
//
//        TestPollData test_data;
//        assert(!test_data.ready_called);
//
//        Eris::Poll::instance().Ready.emit(test_data);
//
//        assert(!test_failure_flag);
//        assert(!test_data.ready_called);
//        assert(m->getStatus() == Meta::INVALID);
//
//        delete m;
//    }

#if 0
	// Test poll works
	{
		Meta * m = new Meta(io_service, event_service, TEST_METASERVER, 20);

		test_failure_flag = false;

		TestPollData test_data;
		assert(!test_data.ready_called);

		m->refresh();
		Eris::Poll::instance().Ready.emit(test_data);

		assert(!test_failure_flag);
		assert(test_data.ready_called);
		assert(m->getStatus() == Meta::INVALID);

		delete m;
	}
#endif

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
