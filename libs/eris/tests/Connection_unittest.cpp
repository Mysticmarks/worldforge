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
#include <Atlas/Codecs/Bach.h>
#include <Atlas/Exception.h>

#include <iostream>
#include <sstream>

// Helper bridge used when encoding objects to the Bach codec.
struct NullBridge : Atlas::Bridge {
    void streamBegin() override {}
    void streamMessage() override {}
    void streamEnd() override {}
    void mapMapItem(std::string) override {}
    void mapListItem(std::string) override {}
    void mapIntItem(std::string, std::int64_t) override {}
    void mapFloatItem(std::string, double) override {}
    void mapStringItem(std::string, std::string) override {}
    void mapNoneItem(std::string) override {}
    void mapEnd() override {}
    void listMapItem() override {}
    void listListItem() override {}
    void listIntItem(std::int64_t) override {}
    void listFloatItem(double) override {}
    void listStringItem(std::string) override {}
    void listNoneItem() override {}
    void listEnd() override {}
};

static std::string encodeToBach(const Atlas::Objects::Root& obj)
{
    std::stringstream in;
    std::stringstream out;
    NullBridge bridge;
    Atlas::Codecs::Bach codec(in, out, bridge);
    Atlas::Objects::ObjectsEncoder encoder(codec);
    encoder.streamObjectsMessage(obj);
    out << std::flush;
    return out.str();
}


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
                , port), failureCalled(false) {
    }

    void testSetStatus(Status sc) { setStatus(sc); }

    void testDispatch() { dispatch(); }

    std::size_t queuedOps() const { return m_opDeque.size(); }

    void testHandleFailure(const std::string& msg) { Eris::Connection::handleFailure(msg); }

    int lockValue() const { return m_lock; }

    void feedBuffer(const std::string& data)
    {
        std::stringstream in(data);
        std::stringstream out;
        Atlas::Codecs::Bach codec(in, out, *m_decoder);
        try {
            codec.poll();
        } catch (const Atlas::Exception& e) {
            handleFailure(e.what());
        }
    }

    bool failureCalled;

  protected:
    void handleFailure(const std::string& msg) override
    {
        failureCalled = true;
        Eris::Connection::handleFailure(msg);
    }
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

    // Test gotData with a valid packet
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        Atlas::Objects::Operation::Login login;
        auto encoded = encodeToBach(login);
        c.feedBuffer(encoded);
        assert(c.queuedOps() == 1);
        c.testDispatch();
        assert(c.queuedOps() == 0);
        assert(!c.failureCalled);
    }

    // Test gotData with malformed data triggers failure
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        c.feedBuffer("{malformed");
        assert(c.failureCalled);
        assert(c.queuedOps() == 0);
    }

    // Test gotData with empty buffer
    {
        boost::asio::io_context io_service;
        Eris::EventService event_service(io_service);
        TestConnection c(io_service, event_service, " name", "localhost", 6767);

        c.feedBuffer("");
        assert(c.queuedOps() == 0);
        assert(!c.failureCalled);
    }

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
