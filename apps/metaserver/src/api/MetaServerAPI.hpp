/**
 Worldforge Next Generation MetaServer

 Copyright (C) 2012 Sean Ryan <sryan@evercrack.com>

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

#ifndef METASERVERAPI_HPP_
#define METASERVERAPI_HPP_

/*
 * Packet is the core of the API, allowing for any local transport
 * mechanisms.
 */
#include "MetaServerPacket.hpp"
#include "MetaServerProtocol.hpp"
#include "MetaServerVersion.hpp"

#include <boost/asio.hpp>
#include <string>

/**
 * Simple helper for sending and receiving MetaServer packets over the
 * network.  The API currently uses UDP via boost::asio.  The target
 * endpoint can be configured through the constructor or later via the
 * setEndpoint method.
 */
class MetaServerAPI {
public:
  /**
   * Construct the API with an optional remote host and port.  The
   * default points to the standard MetaServer port on localhost.
   */
  MetaServerAPI(const std::string &host = "127.0.0.1",
                unsigned short port = 8453);

  /**
   * Change the remote endpoint used for subsequent send/receive
   * operations.
   */
  void setEndpoint(const std::string &host, unsigned short port);

  /**
   * Send a packet to the configured endpoint.
   */
  void sendPacket(const MetaServerPacket &packet);

  /**
   * Blocking receive of a packet from the configured endpoint.
   */
  MetaServerPacket receivePacket();

private:
  boost::asio::io_context m_ioContext;
  boost::asio::ip::udp::endpoint m_endpoint;
  boost::asio::ip::udp::socket m_socket;
};

#endif /* METASERVERAPI_HPP_ */
