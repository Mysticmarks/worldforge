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

#include "server/Peer.h"

#include <cassert>
#include <cctype>
#include <string>

int main() {
        std::string key1 = Peer::generatePossessKey();
        std::string key2 = Peer::generatePossessKey();

        assert(key1.size() == 32);
        assert(key2.size() == 32);

        for (char c : key1) {
                assert(std::isalnum(static_cast<unsigned char>(c)));
        }
        for (char c : key2) {
                assert(std::isalnum(static_cast<unsigned char>(c)));
        }

        assert(key1 != key2);
}
