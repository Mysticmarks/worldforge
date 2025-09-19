/*
 Copyright (C) 2023 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef WORLDFORGE_NETWORK_H
#define WORLDFORGE_NETWORK_H

#include <arpa/inet.h>

#include <string>
#include <array>
#include <optional>
#include <string_view>
#include <cctype>

inline std::string
IpNetToAscii(uint32_t address) {
	std::array<char, INET_ADDRSTRLEN> chars{};
	inet_ntop(AF_INET, &address, chars.data(), INET_ADDRSTRLEN);
	return {chars.data()};
}


/*
 * This is the original metaserver way
 * This ... is stupid IMO, metaserver expects from
 * 127.0.2.1
 *
 * String value	1.2.0.127
   Binary	00000001 . 00000010 . 00000000 . 01111111
   Integer	16908415
 */
inline std::optional<uint32_t>
IpAsciiToNet(std::string_view input) {

        uint32_t ret = 0;
        std::size_t pos = 0;
        for (int octet = 0; octet < 4; ++octet) {
                if (pos >= input.size()) {
                        return std::nullopt;
                }

                uint32_t value = 0;
                std::size_t digitCount = 0;
                while (pos < input.size() && std::isdigit(static_cast<unsigned char>(input[pos]))) {
                        value = value * 10 + static_cast<uint32_t>(input[pos] - '0');
                        if (value > 255) {
                                return std::nullopt;
                        }
                        ++pos;
                        ++digitCount;
                }

                if (digitCount == 0) {
                        return std::nullopt;
                }

                ret |= (value << (octet * 8));

                if (octet < 3) {
                        if (pos >= input.size() || input[pos] != '.') {
                                return std::nullopt;
                        }
                        ++pos;
                } else if (pos != input.size()) {
                        return std::nullopt;
                }
        }

        return ret;
}


#endif //WORLDFORGE_NETWORK_H

