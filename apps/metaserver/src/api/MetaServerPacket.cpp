/**
 Worldforge Next Generation MetaServer

 Copyright (C) 2011 Sean Ryan <sryan@evercrack.com>

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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */


#include "MetaServerPacket.hpp"
#include <netinet/in.h> // htonl
#include <cstring>      // memcpy
#include <stdexcept>

MetaServerPacket::MetaServerPacket() :
		m_packetType(NMT_NULL),
		m_AddressInt(0),
		m_Port(0),
		m_Bytes(0),
		m_packetPayload{},
		m_needFree(true),
		m_outBound(false),
		m_Sequence(0),
		m_TimeOffset(0) {
	m_packetPayload.fill(0);
	m_readPtr = m_packetPayload.data();
	m_headPtr = m_packetPayload.data();
	m_writePtr = m_packetPayload.data();
}

MetaServerPacket::MetaServerPacket(const std::array<char, MAX_PACKET_BYTES>& pl, std::size_t bytes)
                : m_packetType(NMT_NULL),
                  m_AddressInt(0),
                  m_Port(0),
                  m_Bytes(bytes),
                  m_packetPayload(pl),
                  m_needFree(false),
                  m_outBound(false),
                  m_Sequence(0),
                  m_TimeOffset(0) {
        if (bytes > MAX_PACKET_BYTES) {
                throw std::length_error("MetaServerPacket payload exceeds maximum size");
        }

        m_readPtr = m_packetPayload.data();
        m_headPtr = m_packetPayload.data();
        m_writePtr = m_packetPayload.data();

	if (bytes > 0) {
		// if we have data ... parse out type
		parsePacketType();
	} else {
		// otherwise assume a construction and zero it out
		m_packetPayload.fill(0);
	}
}

MetaServerPacket::~MetaServerPacket() = default;

void
MetaServerPacket::setPacketType(const NetMsgType& nmt) {

	/**
	 *
	 */
	m_packetPayload.fill(0);
	m_readPtr = m_packetPayload.data();
	m_writePtr = m_packetPayload.data();
	m_Bytes = 0;

	// write must occur prior to read
	m_writePtr = pack_uint32(nmt, m_writePtr);
	parsePacketType();

}

void
MetaServerPacket::setAddress(const std::string& address, uint32_t addressInt) {

	/*
	 *   NOTE: this is a compat problem, as it is basically a hack
	 *   to make everything logic wise ( session setting, etc ) all
	 *   look like it's from ipv4.  This should work fine for aliased ipv4 addresses
	 *   but for actually ipv6<->ipv6 communication, this could potentially fall down.
	 *
	 *   For this reason ... the internal address here should be considered
	 *   cosmetic.
	 */

	m_AddressStr = address;
	m_AddressInt = addressInt;

}

unsigned int
MetaServerPacket::addPacketData(uint32_t i) {
        char* const current = m_writePtr;
        m_writePtr = pack_uint32(i, m_writePtr);
        return static_cast<unsigned int>(current - m_headPtr);
}

unsigned int
MetaServerPacket::addPacketData(const std::string& s) {
        char* const current = m_writePtr;
        m_writePtr = pack_string(s, m_writePtr);
        return static_cast<unsigned int>(current - m_headPtr);
}

std::string
MetaServerPacket::getPacketMessage(unsigned int offset) const {
	// Just initialise local variable for debug purposes
	std::string tmpstr = "0xDEADBEEF";
	unpack_string(&tmpstr, m_headPtr + offset, (m_Bytes - offset));
	return tmpstr;
}

uint32_t
MetaServerPacket::getIntData(unsigned int offset) const {
	// initial int to arbitrary value for potential debugging purposes
	uint32_t tmpint = 222;
	unpack_uint32(&tmpint, m_readPtr + offset);
	return tmpint;
}

/*  This is the correct way to do things.
 *  ex:
 *  String value	127.0.2.1
	Binary	01111111 . 00000000 . 00000010 . 00000001
	Integer	2130706945
uint32_t
MetaServerPacket::IpAsciiToNet(const char *buffer) {

  uint32_t ret = 0;
  int shift = 24;  //  fill out the MSB first
  bool startQuad = true;
  while ((shift >= 0) && (*buffer)) {
    if (startQuad) {
      unsigned char quad = (unsigned char) atoi(buffer);
      ret |= (((uint32_t)quad) << shift);
      shift -= 8;
    }
    startQuad = (*buffer == '.');
    ++buffer;
  }
  return ret;
}
*/


/**
 * Pulls out the first byte of a packet, which universally indicates the packet type.
 * For empty packets ( ie newly created for outbound )
 * @return NetMsgType
 */
void
MetaServerPacket::parsePacketType() {
	if (m_Bytes > 0) {
		m_readPtr = m_packetPayload.data();
		m_packetType = getIntData(0);
	}

}

/**
 *
 * Not sure if read only accessor is same address space or not
 *
 * @param data - src int
 * @param buffer - dest pointer
 * @return
 */
char*
MetaServerPacket::pack_uint32(uint32_t data, char* buffer) {
        uint32_t netorder;
        const std::size_t max_bytes = MAX_PACKET_BYTES;

        if (m_Bytes > max_bytes || max_bytes - m_Bytes < sizeof(uint32_t)) {
                throw std::length_error("MetaServerPacket::pack_uint32 overflow");
        }

        netorder = htonl(data);

        memcpy(buffer, &netorder, sizeof(uint32_t));
        m_Bytes += sizeof(uint32_t);

	return buffer + sizeof(uint32_t);

}

char*
MetaServerPacket::unpack_uint32(uint32_t* dest, char* buffer) const {
	uint32_t netorder;

	memcpy(&netorder, buffer, sizeof(uint32_t));
	*dest = ntohl(netorder);
	return buffer + sizeof(uint32_t);

}


char*
MetaServerPacket::pack_string(std::string str, char* buffer) {
        const std::size_t ss = str.size();
        const std::size_t max_bytes = MAX_PACKET_BYTES;

        if (m_Bytes > max_bytes || max_bytes - m_Bytes < ss) {
                throw std::length_error("MetaServerPacket::pack_string overflow");
        }

        memcpy(buffer, str.data(), ss);
        m_Bytes += ss;
        return buffer + ss;
}

char*
MetaServerPacket::unpack_string(std::string* dest, char* buffer, unsigned int length) const {
	std::string s(buffer, length);
	*dest = s;
	return buffer + length;
}
