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

#include "DataObject.hpp"

#include <cppunit/TestCase.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TextTestRunner.h>
#include <cppunit/TestListener.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <cassert>
#include <iostream>
#include <algorithm>
#include <vector>

class DataObject_unittest : public CppUnit::TestFixture {
CPPUNIT_TEST_SUITE(DataObject_unittest);
		CPPUNIT_TEST(testConstructor);
		CPPUNIT_TEST(testConstructor_counts);
		CPPUNIT_TEST(test_ServerAttribute);
		CPPUNIT_TEST(test_ClientAttribute);
                CPPUNIT_TEST(test_ClientFilter);
                CPPUNIT_TEST(test_Handshake);
                CPPUNIT_TEST(test_ServerSession);
                CPPUNIT_TEST(test_ClientSession);
                CPPUNIT_TEST(test_ExpireServerSessions);
                CPPUNIT_TEST(test_ExpireHandshakes);


        CPPUNIT_TEST_SUITE_END();

private:
	DataObject* msdo;
public:
	DataObject_unittest() {
		msdo = 0;
	}

	void setUp() {
		msdo = new DataObject();
	}

	void tearDown() {
		delete msdo;
	}

	void testConstructor() {

		DataObject* msdo_temp = new DataObject();
		CPPUNIT_ASSERT(msdo_temp);
		delete msdo_temp;

	}

	void testConstructor_counts() {
		CPPUNIT_ASSERT(msdo->getServerSessionCount() == 0);
		CPPUNIT_ASSERT(msdo->getClientSessionCount() == 0);
		CPPUNIT_ASSERT(msdo->getHandshakeCount() == 0);
	}

	void test_ServerAttribute() {

		// Assert the default (which should be none), can not have attributes without session
		CPPUNIT_ASSERT(msdo->getServerSessionCount() == 0);

		// Add an attribute
		CPPUNIT_ASSERT(msdo->addServerAttribute("test-session", "test1", "test1") == true);

		// There is no "client" listreq associated with this, so only the main data
		// structure.
		std::cout << std::endl << "getServerSessionCount()-0: " << msdo->getServerSessionCount() << std::endl;
		CPPUNIT_ASSERT(msdo->getServerSessionCount() == 1);
		CPPUNIT_ASSERT(msdo->getServerSessionCount("test-session") == 0);

		// Create ourselves a fake listreq and make sure it's good
		std::cout << std::endl << "getServerSessionCount(test-session)-1: " << msdo->getServerSessionCount("test-session") << std::endl;
		msdo->createServerSessionListresp("test-session");
		std::cout << std::endl << "getServerSessionCount(test-session)-2: " << msdo->getServerSessionCount("test-session") << std::endl;
		CPPUNIT_ASSERT(msdo->getServerSessionCount("test-session") == 1);

		// Assert that the value we just inserted is good
		CPPUNIT_ASSERT(msdo->getServerAttribute("test-session", "test1") == "test1");

		// Add another of the same name, which is effectively an update
		CPPUNIT_ASSERT(msdo->addServerAttribute("test-session", "test1", "updated") == true);
		CPPUNIT_ASSERT(msdo->getServerAttribute("test-session", "test1") == "updated");

		// Remove the object
		msdo->removeServerAttribute("test-session", "test1");

		// Make sure it's gone
		CPPUNIT_ASSERT(msdo->getServerAttribute("test-session", "test1") == "");

		// Can't have an empty sessionid or name/key
		CPPUNIT_ASSERT(msdo->addServerAttribute("", "key", "value") == false);
		CPPUNIT_ASSERT(msdo->addServerAttribute("test-session", "", "value") == false);
	}

	void test_ClientAttribute() {
		// Add an attribute
		CPPUNIT_ASSERT(msdo->addClientAttribute("test-session", "test1", "test1") == true);

		// Assert that the value we just inserted is good
		CPPUNIT_ASSERT(msdo->getClientAttribute("test-session", "test1") == "test1");

		// Add another of the same name, which is effectively an update
		CPPUNIT_ASSERT(msdo->addClientAttribute("test-session", "test1", "updated") == true);
		CPPUNIT_ASSERT(msdo->getClientAttribute("test-session", "test1") == "updated");

		// Remove the object
		msdo->removeClientAttribute("test-session", "test1");

		// Make sure it's gone
		CPPUNIT_ASSERT(msdo->getClientAttribute("test-session", "test1") == "");

		// Can't have an empty sessionid or name/key
		CPPUNIT_ASSERT(msdo->addClientAttribute("", "key", "value") == false);
		CPPUNIT_ASSERT(msdo->addClientAttribute("test-session", "", "value") == false);
	}

	void test_ClientFilter() {

		// should fail because there is no session
		CPPUNIT_ASSERT(msdo->addClientFilter("non-existing-id", "test1", "test1") == false);

		// add a session so the other attribute checks will pass
		CPPUNIT_ASSERT(msdo->addClientAttribute("test-session", "test1", "value") == true);
		CPPUNIT_ASSERT(msdo->addClientFilter("test-session", "test1", "test1") == true);

		// Assert that the value we just inserted is good
		std::map<std::string, std::string> filterList;
		filterList = msdo->getClientFilter("test-session");
		CPPUNIT_ASSERT(filterList.size() == 1); // the one we just added
		CPPUNIT_ASSERT(filterList.find("test1") != filterList.end()); // make sure it exists
		CPPUNIT_ASSERT(filterList["test1"] == "test1");

		// Add another of the same name, which is effectively an update
		CPPUNIT_ASSERT(msdo->addClientFilter("test-session", "test1", "updated") == true);
		CPPUNIT_ASSERT(msdo->getClientFilter("test-session", "test1") == "updated");

		// Remove the object
		msdo->removeClientFilter("test-session", "test1");

		// Make sure it's gone
		CPPUNIT_ASSERT(msdo->getClientFilter("test-session", "test1").empty());

		// Can't have an empty sessionid or name/key
		CPPUNIT_ASSERT(msdo->addClientAttribute("", "key", "value") == false);
		CPPUNIT_ASSERT(msdo->addClientAttribute("test-session", "", "value") == false);

	}

	void test_Handshake() {

		// add our test handshake
		CPPUNIT_ASSERT(msdo->addHandshake(123456) == 123456);

		// check that the expiry and the now are within a couple of seconds
		// indicating that the handshake expiry is good (approximately)
		// We can't match it exactly, hence a little slack
		boost::posix_time::ptime hse = msdo->getHandshakeExpiry(123456);
		boost::posix_time::ptime lse = hse + boost::posix_time::milliseconds(1234);

		// should be zero
		CPPUNIT_ASSERT(msdo->getLatency(hse, hse) == 0);

		// lets make sure that the contrived latency is good
		std::cout << "getLatency(hse,lse): " << DataObject::getLatency(hse, lse) << std::endl;
		CPPUNIT_ASSERT(msdo->getLatency(hse, lse) == 1234);

		// does it exist
		CPPUNIT_ASSERT(msdo->handshakeExists(123456) == true);

		// negative exist
		CPPUNIT_ASSERT(msdo->handshakeExists(111) == false);

		// remove it
		CPPUNIT_ASSERT(msdo->removeHandshake(123456) == 123456);

		// random handshake add
		CPPUNIT_ASSERT(msdo->addHandshake(1) != 0);

		// second remove should yield a non-delete
		CPPUNIT_ASSERT(msdo->removeHandshake(123456) == 0);

		// a non-existing deletion
		CPPUNIT_ASSERT(msdo->removeHandshake(111111) == 0);

	}

	void test_ServerSession() {

		std::map<std::string, std::string> sess, sess_b;

		// check for non-existent session
		CPPUNIT_ASSERT(msdo->serverSessionExists("123123") == false);

		// add session
		CPPUNIT_ASSERT(msdo->addServerSession("123123") == true);

		// check for now existing session
		CPPUNIT_ASSERT(msdo->serverSessionExists("123123") == true);

		// try and add the same session, should do an update, not an add
		CPPUNIT_ASSERT(msdo->addServerSession("123123") == false);

		// get session, then check that the expected session key exists
		// and that it matches
		sess = msdo->getServerSession("123123");
		CPPUNIT_ASSERT(!sess.empty());
		CPPUNIT_ASSERT(sess.find("ip") != sess.end());
		CPPUNIT_ASSERT(sess["ip"] == "123123");

		// remove session
		msdo->removeServerSession("123123");

		// get empty session
		sess_b = msdo->getServerSession("123123");
		CPPUNIT_ASSERT(sess_b.empty());

		// negative check for session again
		CPPUNIT_ASSERT(msdo->serverSessionExists("123123") == false);

	}

        void test_ClientSession() {

		std::map<std::string, std::string> sess, sess_b;

		// check for non-existent session
		CPPUNIT_ASSERT(msdo->clientSessionExists("123123") == false);

		// add session
		CPPUNIT_ASSERT(msdo->addClientSession("123123") == true);

		// check for now existing session
		CPPUNIT_ASSERT(msdo->clientSessionExists("123123") == true);

		// try and add the same session, should do an update, not an add
		CPPUNIT_ASSERT(msdo->addClientSession("123123") == false);

		// get session, then check that the expected session key exists
		// and that it matches
		sess = msdo->getClientSession("123123");
		CPPUNIT_ASSERT(!sess.empty());
		CPPUNIT_ASSERT(sess.find("ip") != sess.end());
		CPPUNIT_ASSERT(sess["ip"] == "123123");

		// remove session
		msdo->removeClientSession("123123");

		// get empty session
		sess_b = msdo->getClientSession("123123");
		CPPUNIT_ASSERT(sess_b.empty());

		// negative check for session again
                CPPUNIT_ASSERT(msdo->clientSessionExists("123123") == false);

        }

        void test_ExpireServerSessions() {
                msdo->addServerSession("expired1");
                msdo->addServerSession("expired2");
                std::vector<std::string> expired = msdo->expireServerSessions(0);
                CPPUNIT_ASSERT(expired.size() == 2);
                CPPUNIT_ASSERT(std::find(expired.begin(), expired.end(), "expired1") != expired.end());
                CPPUNIT_ASSERT(msdo->serverSessionExists("expired1") == false);
        }

        void test_ExpireHandshakes() {
                msdo->addHandshake(1);
                msdo->addHandshake(2);
                std::vector<unsigned int> expired = msdo->expireHandshakes(0);
                CPPUNIT_ASSERT(expired.size() == 2);
                CPPUNIT_ASSERT(std::find(expired.begin(), expired.end(), 1u) != expired.end());
                CPPUNIT_ASSERT(msdo->handshakeExists(1) == false);
        }

};


CPPUNIT_TEST_SUITE_REGISTRATION(DataObject_unittest);


int main() {
	CppUnit::TextTestRunner runner;
	CppUnit::Test* tp =
			CppUnit::TestFactoryRegistry::getRegistry().makeTest();

	/**
	 * How can i either add the testsuite or whatever to produce better visual results.
	 */
	runner.addTest(tp);

	//runner.run("",false,true,true);
	if (runner.run()) {
		return 0;
	} else {
		return 1;
	}
}

// Stubs


