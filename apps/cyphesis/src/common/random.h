// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2000-2003 Alistair Riddoch
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


#ifndef COMMON_RANDOM_H
#define COMMON_RANDOM_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>

static inline int randint(int min, int max) {
        if (max == min) {
                return min;
        } else {
                return ::rand() % (max - min) + min;
        }
}

static inline float uniform(float min, float max) {
        return ((float) ::rand() / RAND_MAX) * (max - min) + min;
}

static inline std::mt19937_64& secure_random_engine() {
        thread_local std::mt19937_64 engine([]() {
                std::random_device rd;
                std::array<std::uint32_t, 8> seed_data{};
                std::generate(seed_data.begin(), seed_data.end(), [&rd]() { return rd(); });
                std::seed_seq seq(seed_data.begin(), seed_data.end());
                return std::mt19937_64(seq);
        }());

        return engine;
}

static inline std::string generate_secure_password(std::size_t min_bytes = 16U) {
        auto& engine = secure_random_engine();
        const std::size_t byte_count = std::max<std::size_t>(min_bytes, 16U);

        std::uniform_int_distribution<std::uint64_t> dist(0, std::numeric_limits<std::uint64_t>::max());

        std::string result;
        result.reserve(byte_count * 2U);

        auto append_hex = [&result](std::uint8_t value) {
                constexpr const char* hex = "0123456789abcdef";
                result.push_back(hex[value >> 4U]);
                result.push_back(hex[value & 0x0FU]);
        };

        std::size_t generated_bytes = 0U;
        while (generated_bytes < byte_count) {
                const auto random_value = dist(engine);
                for (std::size_t shift = 0U; shift < sizeof(random_value) && generated_bytes < byte_count; ++shift) {
                        append_hex(static_cast<std::uint8_t>((random_value >> (shift * 8U)) & 0xFFU));
                        ++generated_bytes;
                }
        }

        return result;
}

#endif // COMMON_RANDOM_H
