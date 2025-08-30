// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2012 Alistair Riddoch
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


#include "AccountContext.h"

#include "tools/Interactive.h"
#include "tools/AvatarContext.h"
#include "tools/JunctureContext.h"

#include "common/debug.h"

#include <iostream>


static constexpr auto debug_flag = false;

using Atlas::Objects::Root;
using Atlas::Objects::Operation::RootOperation;

using std::shared_ptr;

AccountContext::AccountContext(Interactive& i,
							   std::string id,
							   std::string u) :
		IdContext(i, std::move(id)),
		m_username(std::move(u)) {
}

bool AccountContext::accept(const RootOperation& op) const {
	cy_debug_print("Checking account context to see if it matches");
	return m_refNo != 0L && !op->isDefaultRefno() && op->getRefno() == m_refNo;
}

int AccountContext::dispatch(const RootOperation& op) {
        cy_debug_print("Dispatching with account context to see if it matches");

        if (op->getClassNo() == Atlas::Objects::Operation::INFO_NO) {
                bool context_established = false;
                if (!op->getArgs().empty()) {
                        std::cout << "Dispatching info" << std::endl;
                        const Root& ent = op->getArgs().front();
                        if (ent->hasAttrFlag(Atlas::Objects::ID_FLAG) &&
                                ent->hasAttrFlag(Atlas::Objects::PARENT_FLAG)) {
                                const std::string& type = ent->getParent();
                                if (type == "juncture") {
                                        std::cout << "created juncture" << std::endl;
                                        m_client.addCurrentContext(shared_ptr<ObjectContext>(
                                                        new JunctureContext(m_client, ent->getId())));
                                        context_established = true;
                                } else if (type == "avatar") {
                                        std::cout << "created avatar" << std::endl;
                                        m_client.addCurrentContext(shared_ptr<ObjectContext>(
                                                        new AvatarContext(m_client, ent->getId())));
                                        context_established = true;
                                }
                        }
                }
                if (!context_established) {
                        std::cerr << "Error: No avatar or juncture context established" << std::endl;
                        assert(m_refNo != 0L);
                        m_refNo = 0L;
                        return -1;
                }
        } else if (op->getClassNo() == Atlas::Objects::Operation::SIGHT_NO &&
                           !op->getArgs().empty()) {
                std::cout << "Sight(" << std::endl;
                m_client.output(op->getArgs().front());
                std::cout << ")" << std::endl;
        }
        assert(m_refNo != 0L);
        m_refNo = 0L;
        return 0;
}

std::string AccountContext::repr() const {
	return m_username;
}

bool AccountContext::checkContextCommand(const struct command*) {
	return false;
}
