// This file may be redistributed and modified only under the terms of
// the GNU Lesser General Public License (See COPYING for details).
// Copyright (C) 2000 Stefanus Du Toit, Dmitry Derevyanko

// $Id$

#if defined(HAVE_BZLIB_H) && defined(HAVE_LIBBZ2)

#include <Atlas/Filters/Bzip2.h>
#include <stdexcept>

#ifndef ASSERT

#include <cassert>

#define ASSERT(exp) assert(exp)
#endif

using Atlas::Filters::Bzip2;

constexpr auto BS100K = 6;
constexpr auto WORKFACTOR = 30;

Bzip2::Bzip2(): incoming{}, outgoing{}, buf{} {
}


void Bzip2::begin() {
	incoming.next_in = nullptr;
	incoming.avail_in = 0;
	incoming.bzalloc = nullptr;
	incoming.bzfree = nullptr;
	incoming.opaque = nullptr;

	outgoing.bzalloc = nullptr;
	outgoing.bzfree = nullptr;
	outgoing.opaque = nullptr;

	BZ2_bzCompressInit(&outgoing, BS100K, 0, WORKFACTOR);
	BZ2_bzDecompressInit(&incoming, 0, 0);
}

void Bzip2::end() {
	BZ2_bzCompressEnd(&outgoing);
	BZ2_bzDecompressEnd(&incoming);
}

std::string Bzip2::encode(const std::string& data) {
	std::string out_string;

	buf[0] = 0;

	outgoing.next_in = const_cast<char*>(data.data());
	outgoing.avail_in = (unsigned int)data.size();

        int status;
        do {
                outgoing.next_out = buf.data();
                outgoing.avail_out = sizeof(buf);

                status = BZ2_bzCompress(&outgoing, BZ_FLUSH);

                if (status != BZ_RUN_OK && status != BZ_FINISH_OK && status != BZ_STREAM_END) {
                        throw std::runtime_error("BZ2_bzCompress failed");
                }

                out_string.append((char*)buf.data(), sizeof(buf) - outgoing.avail_out);
        } while (outgoing.avail_out == 0);

	return out_string;
}

std::string Bzip2::decode(const std::string& data) {
	std::string out_string;

	buf[0] = 0;

	incoming.next_in = const_cast<char*>(data.data());
	incoming.avail_in = (unsigned int)data.size();

        int status;
        do {
                incoming.next_out = buf.data();
                incoming.avail_out = sizeof(buf);

                status = BZ2_bzDecompress(&incoming);

                if (status != BZ_OK && status != BZ_STREAM_END) {
                        throw std::runtime_error("BZ2_bzDecompress failed");
                }

                out_string.append((char*)buf.data(), sizeof(buf) - incoming.avail_out);

        } while (status != BZ_STREAM_END && incoming.avail_out == 0);

	return out_string;
}

#endif // HAVE_BZLIB_H && HAVE_LIBBZ2
