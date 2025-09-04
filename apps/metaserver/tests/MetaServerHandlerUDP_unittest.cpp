/**
 Worldforge Next Generation MetaServer

 Copyright (C) 2011 Alistair Riddoch <alriddoch@googlemail.com>
 	 	 	 	 	Sean Ryan <sryan@evercrack.com>

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

/*
 * Local Includes
 */
#include "MetaServerHandlerUDP.hpp"
#include "MetaServer.hpp"

/*
 * System Includes
 */
#include <cppunit/TestCase.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TextTestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <boost/asio/error.hpp>

/*
 * Forward Declarations
 */


#include <cassert>

class MetaServerHandlerUDP_unittest : public CppUnit::TestCase {
CPPUNIT_TEST_SUITE(MetaServerHandlerUDP_unittest);
                CPPUNIT_TEST(testConstructor);
                CPPUNIT_TEST(testResponseLifetime);
                CPPUNIT_TEST(testProcessOutboundAbort);
                CPPUNIT_TEST(testProcessOutboundRetry);
        CPPUNIT_TEST_SUITE_END();
public:
	MetaServerHandlerUDP_unittest() {
		ms = NULL;
		ms_udp = NULL;
	}

	boost::asio::io_context io;
	std::string host;
	MetaServer* ms;
	MetaServerHandlerUDP* ms_udp;

	void setUp() {
		host = "127.0.0.1";
		ms = new MetaServer();
		ms_udp = new MetaServerHandlerUDP(*ms, io, host, 50000);
	}


	void tearDown() {
		delete ms;
		delete ms_udp;
	}

        void testConstructor() {
                CPPUNIT_ASSERT(ms_udp);
        }

        void testResponseLifetime() {
                using boost::asio::ip::udp;
                udp::socket client(io, udp::endpoint(udp::v4(), 0));
                udp::endpoint server(boost::asio::ip::make_address(host), 50000);

                MetaServerPacket req;
                req.setPacketType(NMT_SERVERKEEPALIVE);

                client.send_to(boost::asio::buffer(req.getBuffer(), req.getSize()), server);
                io.poll();

                std::array<char, MAX_PACKET_BYTES> reply{};
                udp::endpoint sender;
                auto len = client.receive_from(boost::asio::buffer(reply), sender);
                io.poll();

                MetaServerPacket rsp(reply, len);
                CPPUNIT_ASSERT_EQUAL(NMT_HANDSHAKE, rsp.getPacketType());
        }

        void testProcessOutboundAbort() {
                CPPUNIT_ASSERT_EQUAL(0UL, ms_udp->getOutboundTick());
                boost::system::error_code ec = boost::asio::error::operation_aborted;
                ms_udp->process_outbound(ec);
                CPPUNIT_ASSERT_EQUAL(0UL, ms_udp->getOutboundTick());
        }

        void testProcessOutboundRetry() {
                CPPUNIT_ASSERT_EQUAL(0UL, ms_udp->getOutboundTick());
                boost::system::error_code ec = boost::asio::error::fault;
                ms_udp->process_outbound(ec);
                CPPUNIT_ASSERT(ms_udp->getOutboundTick() > 0);
        }
};

CPPUNIT_TEST_SUITE_REGISTRATION(MetaServerHandlerUDP_unittest);


int main() {
	CppUnit::TextTestRunner runner;
	CppUnit::Test* tp =
			CppUnit::TestFactoryRegistry::getRegistry().makeTest();

	runner.addTest(tp);

	if (runner.run()) {
		return 0;
	} else {
		return 1;
	}
}

/*
 * Mock Stubs
 */
MetaServer::MetaServer()
		: m_handshakeExpirySeconds(30),
		  m_expiryDelayMilliseconds(1500),
		  m_updateDelayMilliseconds(4000),
		  m_sessionExpirySeconds(3600),
		  m_clientExpirySeconds(300),
		  m_packetLoggingFlushSeconds(10),
		  m_loggingFlushSeconds(10),
		  m_maxServerSessions(1024),
		  m_maxClientSessions(4096),
		  m_startTime(boost::posix_time::microsec_clock::local_time()),
		  m_keepServerStats(false),
		  m_keepClientStats(false),
		  m_logServerSessions(false),
		  m_logClientSessions(false),
		  m_logPackets(false),
		  m_isDaemon(false),
		  m_PacketSequence(0),
		  m_Logfile(""),
		  m_PacketLogfile(""),
		  m_isShutdown(false),
		  m_logPacketAllow(false),
		  m_logPacketDeny(false) {
	m_updateTimer = NULL;
	m_expiryTimer = NULL;
}

MetaServer::~MetaServer() {}

void MetaServer::processMetaserverPacket(MetaServerPacket&, MetaServerPacket& rsp) {
        rsp.setPacketType(NMT_HANDSHAKE);
        rsp.addPacketData(0);
}

DataObject::DataObject() {

}

DataObject::~DataObject() {

}

