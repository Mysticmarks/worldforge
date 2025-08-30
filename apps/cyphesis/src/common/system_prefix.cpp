// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2010 Alistair Riddoch
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


#ifdef CYPHESIS_USE_BINRELOC
#include "binreloc.h"
#endif
#include "globals.h"
#include "system.h"
#include <iostream>
#include <fmt/format.h>

void getinstallprefix() {
#ifdef CYPHESIS_USE_BINRELOC
        BrInitError error;
        if (br_init(&error) == 0) {
                std::cerr << "Error when setting up BinReloc: " << error << std::endl;
                return;
        }

        etc_directory = br_find_etc_dir("");
        share_directory = br_find_data_dir("data");
        var_directory = fmt::format("{}/var", br_find_prefix(""));
        bin_directory = br_find_bin_dir("bin");
#else
        etc_directory = SYSCONFDIR;
        share_directory = DATADIR;
        var_directory = LOCALSTATEDIR;
        bin_directory = BINDIR;
#endif
}
