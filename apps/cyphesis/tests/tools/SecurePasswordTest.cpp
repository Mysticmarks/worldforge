// Cyphesis Online RPG Server and AI Engine
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

#include "common/random.h"

#include <cassert>
#include <cctype>

int main() {
        auto first = generate_secure_password();
        auto second = generate_secure_password();

        assert(first.size() >= 32);
        assert(second.size() >= 32);

        for (auto ch : first) {
                assert(std::isxdigit(static_cast<unsigned char>(ch)) != 0);
        }
        for (auto ch : second) {
                assert(std::isxdigit(static_cast<unsigned char>(ch)) != 0);
        }

        assert(first != second);

        return 0;
}
