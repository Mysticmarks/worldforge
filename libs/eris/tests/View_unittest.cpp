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

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include <Eris/Account.h>
#include <Eris/Avatar.h>
#include <Eris/Connection.h>
#include <Eris/EventService.h>
#include <Eris/View.h>

#include <Atlas/Objects/Entity.h>
#include <Atlas/Objects/Operation.h>

#include <boost/asio/io_context.hpp>

#include <cassert>

// Simple connection stub that swallows outgoing ops.
class DummyConnection : public Eris::Connection {
public:
    DummyConnection(boost::asio::io_context& io, Eris::EventService& es)
        : Eris::Connection(io, es, "", "", 0) {}

    void send(const Atlas::Objects::Root&) override {}
};

int main() {
    boost::asio::io_context io;
    Eris::EventService es(io);
    DummyConnection conn(io, es);
    Eris::Account account(conn);
    Eris::Avatar avatar(account, "mind_id", "char_id");

    Eris::View* view = avatar.getView();

    const std::string fromId = "seer";
    const std::string appearId = "new_object";

    assert(!view->isPending(appearId));
    assert(!view->isPending(fromId));

    Atlas::Objects::Operation::Appearance app;
    app->setFrom(fromId);
    Atlas::Objects::Entity::Anonymous arg;
    arg->setId(appearId);
    app->setArgs1(arg);

    view->handleOperation(app);

    assert(view->isPending(appearId));
    assert(!view->isPending(fromId));

    return 0;
}

