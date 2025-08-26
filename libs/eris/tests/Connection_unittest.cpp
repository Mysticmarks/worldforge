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

#include <Eris/Connection.h>

#include <Eris/Log.h>
#include <Eris/EventService.h>

#include <Atlas/Objects/Root.h>
#include <Atlas/Objects/SmartPtr.h>
#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Entity.h>

#include <iostream>


class TestConnection : public Eris::Connection {
  public:
    TestConnection(boost::asio::io_context& io_service,
    		Eris::EventService& eventService, 
    		const std::string &cnm, 
    		const std::string& host,
    		short port) 
    : Eris::Connection(io_service,
    		eventService, 
    		cnm, 
    		host
    		, port) {
    }

    void testSetStatus(Status sc) { setStatus(sc); }

    void testDispatch() { dispatch(); }

    std::size_t queuedOps() const { return m_opDeque.size(); }

    void testHandleFailure(const std::string& msg) { handleFailure(msg); }

    int lockValue() const { return m_lock; }
};

int main()
{

    // Test constructor and destructor
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        Eris::Connection c(io_service, event_service, "name", "localhost", 6767);
    }

    // Test getTypeService()
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        Eris::Connection c(io_service, event_service, " name", "localhost", 6767);

        c.getTypeService();
    }

    // Test connect()
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        Eris::Connection c(io_service, event_service, " name", "localhost", 6767);
        
        int ret = c.connect();

        assert(ret == 0);
    }

    // Test connect() with socket
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        Eris::Connection c(io_service, event_service, " name", "localhost", 6767);

        int ret = c.connect();

        assert(ret == 0);
    }

    // Test disconnect() when disconnected
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        Eris::Connection c(io_service, event_service, " name", "localhost", 6767);

        c.disconnect();
    }

    // Test disconnect() when disconnecting
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        c.testSetStatus(Eris::BaseConnection::DISCONNECTING);

        c.disconnect();

        c.testSetStatus(Eris::BaseConnection::DISCONNECTED);
    }

    // Test disconnect() when connecting
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        c.connect();

        assert(c.getStatus() == Eris::BaseConnection::CONNECTING);

        c.disconnect();

        c.testSetStatus(Eris::BaseConnection::DISCONNECTED);
    }

    // Test dispatch()
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        c.testDispatch();
    }

    // Additional tests exercising data reception paths
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        Atlas::Objects::Operation::Login login;
        c.objectArrived(login);
        assert(c.queuedOps() == 1);
        c.testDispatch();
        assert(c.queuedOps() == 0);
    }

    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        Atlas::Objects::Entity::RootEntity ent;
        c.objectArrived(ent);
        assert(c.queuedOps() == 0);
    }

    // Test that handleFailure resets state
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        c.lock();
        Atlas::Objects::Operation::Login login;
        c.objectArrived(login);
        assert(c.queuedOps() == 1);
        c.testHandleFailure("fail");
        assert(c.lockValue() == 0);
        assert(c.queuedOps() == 0);
    }

    // Test serial number generation
    {
        auto first = Eris::getNewSerialno();
        auto second = Eris::getNewSerialno();
        assert(second == first + 1);
        assert(first > 1001);
    }

    // FIXME Not testing all the code paths through gotData()

    // Test send()
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        Eris::Connection c(io_service, event_service, " name", "localhost", 6767);

        Atlas::Objects::Root obj;

        c.send(obj);
    }
    return 0;
}
