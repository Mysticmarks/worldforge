// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2024
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

#include "tools/AccountContext.h"
#include "tools/Interactive.h"

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Anonymous.h>

#include <boost/asio/io_context.hpp>

#include <cassert>
#include <iostream>
#include <sstream>

int main() {
        Atlas::Objects::Factories factories;
        boost::asio::io_context io_context;
        Interactive interactive(factories, io_context);
        AccountContext ctx(interactive, "acc", "user");

        // Prepare for dispatch: set m_refNo via setFromContext
        Atlas::Objects::Operation::Info req;
        req->setSerialno(1);
        ctx.setFromContext(req);

        // Case 1: Info operation with no arguments
        Atlas::Objects::Operation::Info op1;
        op1->setRefno(1);
        std::ostringstream err;
        auto* old_buf = std::cerr.rdbuf(err.rdbuf());
        int res = ctx.dispatch(op1);
        std::cerr.rdbuf(old_buf);
        assert(res == -1);
        assert(err.str().find("No avatar or juncture context established") != std::string::npos);

        // Case 2: Info operation with malformed entity
        req->setSerialno(2);
        ctx.setFromContext(req);
        Atlas::Objects::Operation::Info op2;
        Atlas::Objects::Entity::Anonymous ent;
        // Missing id and parent
        op2->setArgs1(ent);
        op2->setRefno(2);
        err.str("");
        err.clear();
        old_buf = std::cerr.rdbuf(err.rdbuf());
        res = ctx.dispatch(op2);
        std::cerr.rdbuf(old_buf);
        assert(res == -1);
        assert(err.str().find("No avatar or juncture context established") != std::string::npos);

        return 0;
}
