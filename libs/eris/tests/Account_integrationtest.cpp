// Eris Online RPG Protocol Library
// Copyright (C) 2007 Alistair Riddoch
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

// $Id$

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include <Eris/Account.h>

#include <Eris/Connection.h>
#include <Eris/Avatar.h>
#include <Eris/Exceptions.h>
#include <Eris/SpawnPoint.h>
#include <Eris/EventService.h>

#include "SignalFlagger.h"

#include <Atlas/Objects/Anonymous.h>
#include <Atlas/Objects/Entity.h>
#include <Atlas/Objects/Operation.h>

#include <sigc++/adaptors/hide.h>

#include <iostream>
#include <chrono>
#include <cstdint>

namespace {
    boost::asio::io_context io_service;
    Eris::EventService event_service(io_service);
}

class TestConnection : public Eris::Connection {
public:
	TestConnection(const std::string& name,
				   const std::string& host,
				   short port) :
			Eris::Connection(io_service, event_service, name, host, port) {}

	void test_setStatus(Eris::BaseConnection::Status sc) {
		setStatus(sc);
	}

        void send(const Atlas::Objects::Root& obj) override {
                std::cout << "Sending " << obj->getParent()
                                  << std::endl;
        }
};

class MockConnection : public TestConnection {
public:
        MockConnection(const std::string& name,
                                           const std::string& host,
                                           short port)
                        : TestConnection(name, host, port), lastSerial(0) {}

        void send(const Atlas::Objects::Root& obj) override {
                auto op = smart_dynamic_cast<Atlas::Objects::Operation::RootOperation>(obj);
                if (op.isValid()) {
                        lastSerial = op->getSerialno();
                }
        }

        void respond(const Atlas::Objects::Operation::RootOperation& op) {
                op->setRefno(lastSerial);
                getResponder().handleOp(op);
        }

        std::int64_t lastSerial;
};

class TestAccount : public Eris::Account {
public:
	TestAccount(Eris::Connection& con) : Eris::Account(con) {}

	void setup_setStatus(Eris::Account::Status s) {
		m_status = s;
	}

	void setup_setDoingCharacterRefresh(bool f) {
		m_doingCharacterRefresh = f;
	}

	void setup_fakeCharacter(const std::string& id) {
		m_characterIds.insert(id);
	}

	void setup_insertActiveCharacters(std::unique_ptr<Eris::Avatar> ea) {
		m_activeAvatars.emplace(ea->getId(), std::move(ea));
	}

	void setup_setUsername(const std::string& u) {
		m_username = u;
	}

	void setup_setPassword(const std::string& p) {
		m_pass = p;
	}

	Eris::Account::Status query_getStatus() const {
		return m_status;
	}

	void test_logoutResponse(const Atlas::Objects::Operation::RootOperation& op) {
		logoutResponse(op);
	}

	void test_loginComplete(const Atlas::Objects::Entity::Account& p) {
		loginComplete(p);
	}

	void test_updateFromObject(const Atlas::Objects::Entity::Account& p) {
		updateFromObject(p);
	}

	void test_loginError(const Atlas::Objects::Operation::Error& err) {
		loginError(err);
	}

	void test_handleLoginTimeout() {
		handleLoginTimeout();
	}

	void test_avatarResponse(const Atlas::Objects::Operation::RootOperation& op) {
		possessResponse(op);
	}

	void test_internalDeactivateCharacter(const std::string& avatarId) {
		internalDeactivateCharacter(avatarId);
	}

	void test_sightCharacter(const Atlas::Objects::Operation::RootOperation& op) {
		sightCharacter(op);
	}

	void test_netConnected() {
		netConnected();
	}

	bool test_netDisconnecting() {
		return netDisconnecting();
	}

	void test_netFailure(const std::string& msg) {
		netFailure(msg);
	}

        void test_handleLogoutTimeout() {
                handleLogoutTimeout();
        }

        void test_avatarLogoutResponse(const Atlas::Objects::Operation::RootOperation& op) {
                avatarLogoutResponse(op);
        }

        bool query_hasTimeout() const {
                return static_cast<bool>(m_timeout);
        }

        void setup_makeTimeout() {
                m_timeout = std::make_unique<Eris::TimedEvent>(m_con.getEventService(),
                                        std::chrono::seconds(1), []() {});
        }

        Eris::Result test_internalLogin(const std::string& u, const std::string& p) {
                return internalLogin(u, p);
        }

	static const Eris::Account::Status DISCONNECTED = Eris::Account::Status::DISCONNECTED;
	static const Eris::Account::Status LOGGING_IN = Eris::Account::Status::LOGGING_IN;
	static const Eris::Account::Status LOGGED_IN = Eris::Account::Status::LOGGED_IN;
	static const Eris::Account::Status LOGGING_OUT = Eris::Account::Status::LOGGING_OUT;
	static const Eris::Account::Status CREATING_CHAR = Eris::Account::Status::CREATING_CHAR;
	static const Eris::Account::Status TAKING_CHAR = Eris::Account::Status::TAKING_CHAR;
};

class TestAvatar : public Eris::Avatar {
public:
	TestAvatar(Eris::Account* ac, std::string mindId, const std::string& ent_id) :
			Eris::Avatar(*ac, mindId, ent_id) {}
};

int main() {
	boost::asio::io_context io_service;
	Eris::EventService tes(io_service);

	// Test constructor
	{
		Eris::Connection con(io_service,
													 tes,
													 "name",
													 "localhost",
													 6767);

		Eris::Account acc(con);
	}
	// Test getActiveCharacters()
	{
		Eris::Connection con(io_service, tes, "name", "localhost",
													 6767);

		Eris::Account acc(con);

		const Eris::ActiveCharacterMap& acm = acc.getActiveCharacters();

		assert(acm.empty());
	}
	// Test getId()
	{
		Eris::Connection con(io_service, tes, "name", "localhost",
													 6767);

		Eris::Account acc(con);

		const std::string& id = acc.getId();

		assert(id.empty());
	}
	// Test getUsername()
	{
		Eris::Connection con(io_service, tes, "name", "localhost",
													 6767);

		Eris::Account acc(con);

		const std::string& username = acc.getUsername();

		assert(username.empty());
	}

	// Test getConnection()
	{
		Eris::Connection con(io_service, tes, "name", "localhost",
													 6767);

		Eris::Account acc(con);

		Eris::Connection& got_con = acc.getConnection();

		assert(&got_con == &con);
	}

	// Test login() fails if not connected
	{
		TestConnection con("name", "localhost",
												 6767);

		Eris::Account acc(con);

		Eris::Result res = acc.login("foo", "bar");
		assert(res == Eris::NOT_CONNECTED);
	}

	// Test login() fails if we fake connected and logged in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGING_IN);

		Eris::Result res = acc.login("foo", "bar");
		assert(res == Eris::ALREADY_LOGGED_IN);
	}

	// Test login() works if we fake connected
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		con.test_setStatus(Eris::BaseConnection::CONNECTED);

		Eris::Result res = acc.login("foo", "bar");
		assert(res == Eris::NO_ERR);
	}

	// Test createAccount() fails if not connected
	{
		TestConnection con("name", "localhost",
												 6767);

		Eris::Account acc(con);

		Eris::Result res = acc.createAccount("foo", "bar", "baz");
		assert(res == Eris::NOT_CONNECTED);
	}

	// Test createAccount() works if we fake connected.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		con.test_setStatus(Eris::BaseConnection::CONNECTED);

		Eris::Result res = acc.createAccount("foo", "bar", "baz");
		assert(res == Eris::NO_ERR);
	}

	// Test logout() fails if not connected.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		Eris::Result res = acc.logout();
		assert(res == Eris::NOT_CONNECTED);
	}

	// Test logout() does nothing if we fake connected, and logging out.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGING_OUT);

		Eris::Result res = acc.logout();
		assert(res == Eris::NO_ERR);
	}

	// Test logout() fails if we are fake connected but not logged in.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		con.test_setStatus(Eris::BaseConnection::CONNECTED);

		Eris::Result res = acc.logout();
		assert(res == Eris::NOT_LOGGED_IN);
	}

	// Test logout() works if we are fake connected and logged in.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGED_IN);

		Eris::Result res = acc.logout();
		assert(res == Eris::NO_ERR);
	}

	// Test getCharacters()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		const Eris::CharacterMap& ecm = acc.getCharacters();

		assert(ecm.empty());
	}

	// Test refreshCharacterInfo() fails if not connected
	{
		TestConnection con("name", "localhost",
												 6767);

		Eris::Account acc(con);

		Eris::Result res = acc.refreshCharacterInfo();
		assert(res == Eris::NOT_CONNECTED);
	}

	// Test refreshCharacterInfo() fails if we fake connected and logging in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGING_IN);

		Eris::Result res = acc.refreshCharacterInfo();
		assert(res == Eris::NOT_LOGGED_IN);
	}

	// Test refreshCharacterInfo() returns if we fake connected and logged in,
	// and pretend a query is already occuring.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGED_IN);
		acc.setup_setDoingCharacterRefresh(true);

		Eris::Result res = acc.refreshCharacterInfo();
		assert(res == Eris::NO_ERR);

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}

	// Test refreshCharacterInfo() returns if we fake connected and logged in,
	// and have no characters IDs.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger gotAllCharacters_checker;

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGED_IN);
		acc.GotAllCharacters.connect(sigc::mem_fun(gotAllCharacters_checker,
												   &SignalFlagger::set));

		assert(!gotAllCharacters_checker.flagged());
		Eris::Result res = acc.refreshCharacterInfo();
		assert(res == Eris::NO_ERR);
		assert(gotAllCharacters_checker.flagged());

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}

	// Test refreshCharacterInfo() works if we fake connected and logged in,
	// and have characters IDs.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger gotAllCharacters_checker;
		std::string fake_char_id("1");

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGED_IN);
		acc.GotAllCharacters.connect(sigc::mem_fun(gotAllCharacters_checker,
												   &SignalFlagger::set));
		acc.setup_fakeCharacter(fake_char_id);


		assert(!gotAllCharacters_checker.flagged());
		Eris::Result res = acc.refreshCharacterInfo();
		assert(res == Eris::NO_ERR);
		assert(!gotAllCharacters_checker.flagged());

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}

	// Test createCharacter() fails if not connected
	{
		TestConnection con("name", "localhost",
												 6767);

		Eris::Account acc(con);
		Atlas::Objects::Entity::Anonymous ent;

		Eris::Result res = acc.createCharacterThroughEntity(ent);
		assert(res == Eris::NOT_CONNECTED);
	}

	// Test createCharacter() fails if we fake connected and creating char
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Entity::Anonymous ent;

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::CREATING_CHAR);

		Eris::Result res = acc.createCharacterThroughEntity(ent);
		assert(res == Eris::DUPLICATE_CHAR_ACTIVE);

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}

	// Test createCharacter() fails if we fake connected and taking char
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Entity::Anonymous ent;

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::TAKING_CHAR);

		Eris::Result res = acc.createCharacterThroughEntity(ent);
		assert(res == Eris::DUPLICATE_CHAR_ACTIVE);

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}

	// Test createCharacter() fails if we fake connected but not logged in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Entity::Anonymous ent;

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGING_IN);

		Eris::Result res = acc.createCharacterThroughEntity(ent);
		assert(res == Eris::NOT_LOGGED_IN);
	}

	// Test createCharacter() works if we fake connected and logged in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Entity::Anonymous ent;

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGED_IN);

		Eris::Result res = acc.createCharacterThroughEntity(ent);
		assert(res == Eris::NO_ERR);
	}

	// Test takeCharacter() fails if not connected
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		std::string fake_char_id("1");

		acc.setup_fakeCharacter(fake_char_id);

		Eris::Result res = acc.takeCharacter(fake_char_id);
		assert(res == Eris::NOT_CONNECTED);
	}

	// Test takeCharacter() fails if we fake connected and logging in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		std::string fake_char_id("1");

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGING_IN);
		acc.setup_fakeCharacter(fake_char_id);

		Eris::Result res = acc.takeCharacter(fake_char_id);
		assert(res == Eris::NOT_LOGGED_IN);
	}

	// Test takeCharacter() fails if we fake connected and logging in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		std::string fake_char_id("1");

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::CREATING_CHAR);
		acc.setup_fakeCharacter(fake_char_id);

		Eris::Result res = acc.takeCharacter(fake_char_id);
		assert(res == Eris::DUPLICATE_CHAR_ACTIVE);

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}

	// Test takeCharacter() fails if we fake connected and logging in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		std::string fake_char_id("1");

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::TAKING_CHAR);
		acc.setup_fakeCharacter(fake_char_id);

		Eris::Result res = acc.takeCharacter(fake_char_id);
		assert(res == Eris::DUPLICATE_CHAR_ACTIVE);

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}

	// Test takeCharacter() works if we fake connected and logged in
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		std::string fake_char_id("1");

		con.test_setStatus(Eris::BaseConnection::CONNECTED);
		acc.setup_setStatus(TestAccount::LOGGED_IN);
		acc.setup_fakeCharacter(fake_char_id);

		Eris::Result res = acc.takeCharacter(fake_char_id);
		assert(res == Eris::NO_ERR);

		// Make sure it no longer thinks it is connected, so it does not try
		// to log out in the destructor.
		acc.setup_setStatus(TestAccount::LOGGING_OUT);
	}


	// Test isLoggedIn()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		assert(!acc.isLoggedIn());
	}

	// Test isLoggedIn()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.setup_setStatus(TestAccount::LOGGED_IN);

		assert(acc.isLoggedIn());
	}

	// Test isLoggedIn()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.setup_setStatus(TestAccount::TAKING_CHAR);

		assert(acc.isLoggedIn());
	}

	// Test isLoggedIn()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.setup_setStatus(TestAccount::CREATING_CHAR);

		assert(acc.isLoggedIn());
	}

        // Test internalLogin()
        {
                TestConnection con("name", "localhost",
                                                                                                 6767);

                TestAccount acc(con);

                Eris::Result res = acc.test_internalLogin("foo", "bar");

                assert(res == Eris::NO_ERR);
        }

        // Test loginResponse() with successful Info
        {
                MockConnection con("name", "localhost",
                                                                                                6767);

                TestAccount acc(con);
                SignalFlagger loginSuccess_checker;

                con.test_setStatus(Eris::BaseConnection::CONNECTED);
                acc.LoginSuccess.connect(sigc::mem_fun(loginSuccess_checker,
                                                                                           &SignalFlagger::set));

                Eris::Result res = acc.login("foo", "bar");
                assert(res == Eris::NO_ERR);
                assert(acc.query_hasTimeout());
                assert(!acc.isLoggedIn());

                Atlas::Objects::Operation::Info info;
                Atlas::Objects::Entity::Account p;
                p->setId("1");
                p->setUsername("foo");
                info->setArgs1(p);
                con.respond(info);

                assert(acc.isLoggedIn());
                assert(!acc.query_hasTimeout());
                assert(loginSuccess_checker.flagged());
        }

        // Test logoutResponse() with successful Info
        {
                MockConnection con("name", "localhost",
                                                                                                6767);

                TestAccount acc(con);
                bool logoutSignal = false;

                con.test_setStatus(Eris::BaseConnection::CONNECTED);
                acc.login("foo", "bar");
                Atlas::Objects::Operation::Info info;
                Atlas::Objects::Entity::Account p;
                p->setId("1");
                p->setUsername("foo");
                info->setArgs1(p);
                con.respond(info);
                assert(acc.isLoggedIn());

                acc.LogoutComplete.connect([&](bool clean){ logoutSignal = clean; });

                Eris::Result res = acc.logout();
                assert(res == Eris::NO_ERR);
                assert(acc.query_hasTimeout());

                Atlas::Objects::Operation::Info logoutInfo;
                con.respond(logoutInfo);

                assert(!acc.isLoggedIn());
                assert(!acc.query_hasTimeout());
                assert(logoutSignal);
        }

        // Test internalLogout() forced
        {
                MockConnection con("name", "localhost",
                                                                                                6767);

                TestAccount acc(con);
                bool logoutSignal = true;

                acc.setup_setStatus(TestAccount::LOGGED_IN);
                acc.setup_makeTimeout();
                acc.LogoutComplete.connect([&](bool clean){ logoutSignal = clean; });

                assert(acc.query_hasTimeout());
                assert(acc.isLoggedIn());

                acc.test_internalLogout(false);

                assert(!acc.isLoggedIn());
                assert(!acc.query_hasTimeout());
                assert(!logoutSignal);
        }

        // Test loginComplete() does nothing when not logged in.
        {
                TestConnection con("name", "localhost",
                                                                                                 6767);

		TestAccount acc(con);
		Atlas::Objects::Entity::Account p;

		p->setUsername("bob");

		acc.test_loginComplete(p);
	}

        // Test loginComplete()
        {
                TestConnection con("name", "localhost",
                                                                                                 6767);

                TestAccount acc(con);
                Atlas::Objects::Entity::Account p;
                SignalFlagger loginSuccess_checker;

                p->setUsername("bob");
                acc.setup_makeTimeout();
                acc.LoginSuccess.connect(sigc::mem_fun(loginSuccess_checker,
                                                                                           &SignalFlagger::set));

                acc.test_loginComplete(p);

                assert(acc.isLoggedIn());
                assert(loginSuccess_checker.flagged());
                assert(!acc.query_hasTimeout());
        }

	// Test updateFromObject() with a bad character list
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Entity::Account p;

		p->setAttr("character_types", "non-list");

		acc.test_updateFromObject(p);
	}


	// Test loginError() with no arg
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Error err;

		acc.test_loginError(err);
	}

	// Test loginError() with empty arg
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Error err;
		Atlas::Objects::Root err_arg;
		err->setArgs1(err_arg);

		acc.test_loginError(err);
	}

	// Test loginError() with arg that has non-string message
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Error err;
		Atlas::Objects::Root err_arg;
		err_arg->setAttr("message", 1);
		err->setArgs1(err_arg);

		acc.test_loginError(err);
	}

	// Test loginError() with arg that has non-string message
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Error err;
		Atlas::Objects::Root err_arg;
		SignalFlagger loginFailure_checker;

		err_arg->setAttr("message", "Yur loginz, let me show you them.");
		err->setArgs1(err_arg);
		acc.LoginFailure.connect(sigc::hide(sigc::mem_fun(loginFailure_checker,
														  &SignalFlagger::set)));

		acc.test_loginError(err);

		assert(loginFailure_checker.flagged());
	}

	// Test handleLoginTimeout()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger loginFailure_checker;

		acc.LoginFailure.connect(sigc::hide(sigc::mem_fun(loginFailure_checker,
														  &SignalFlagger::set)));

		acc.test_handleLoginTimeout();

		assert(loginFailure_checker.flagged());
	}

	// Test avatarPossessResponse() Error op
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Error op;
		SignalFlagger avatarFailure_checker;

		acc.AvatarFailure.connect(sigc::hide(sigc::mem_fun(avatarFailure_checker,
														   &SignalFlagger::set)));

		acc.test_avatarResponse(op);
		assert(avatarFailure_checker.flagged());
		assert(acc.query_getStatus() == TestAccount::LOGGED_IN);

		acc.setup_setStatus(TestAccount::DISCONNECTED);
	}

	// Test avatarPossessResponse() Info op with no arg
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Info op;
		SignalFlagger avatarSuccess_checker;

		acc.AvatarSuccess.connect(sigc::hide(sigc::mem_fun(avatarSuccess_checker,
														   &SignalFlagger::set)));

		acc.test_avatarResponse(op);
		assert(!avatarSuccess_checker.flagged());
		assert(acc.query_getStatus() == TestAccount::DISCONNECTED);
	}

	// Test avatarPossessResponse() Info op with non-entity arg
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Info op;
		Atlas::Objects::Root bad_arg;
		SignalFlagger avatarSuccess_checker;

		op->setArgs1(bad_arg);
		acc.AvatarSuccess.connect(sigc::hide(sigc::mem_fun(avatarSuccess_checker,
														   &SignalFlagger::set)));

		acc.test_avatarResponse(op);
		assert(!avatarSuccess_checker.flagged());
		assert(acc.query_getStatus() == TestAccount::DISCONNECTED);
	}

	// Test avatarPossessResponse() Info op with entity arg
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Info op;
		Atlas::Objects::Entity::Anonymous info_arg;
		Atlas::Objects::Entity::Anonymous entity_arg;
		entity_arg->setId("1");
		info_arg->setAttr("entity", entity_arg->asMessage());
		SignalFlagger avatarSuccess_checker;

		op->setArgs1(info_arg);
		acc.AvatarSuccess.connect(sigc::hide(sigc::mem_fun(avatarSuccess_checker,
														   &SignalFlagger::set)));

		acc.test_avatarResponse(op);
		assert(avatarSuccess_checker.flagged());
		assert(acc.query_getStatus() == TestAccount::LOGGED_IN);

		acc.setup_setStatus(TestAccount::DISCONNECTED);
	}

	// Test avatarPossessResponse() Get (not info or error) op with entity arg
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Get op; // Arbitrary not Info or Error
		Atlas::Objects::Entity::Anonymous info_arg;
		SignalFlagger avatarSuccess_checker;

		op->setArgs1(info_arg);
		acc.AvatarSuccess.connect(sigc::hide(sigc::mem_fun(avatarSuccess_checker,
														   &SignalFlagger::set)));

		acc.test_avatarResponse(op);
		assert(!avatarSuccess_checker.flagged());
		assert(acc.query_getStatus() == TestAccount::DISCONNECTED);
	}

	// Test internalDeactivateCharacter() directly
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		std::string fake_char_id("1");
		std::string fake_mind_id("1");
		//This avatar will be
		auto ea = std::make_unique<TestAvatar>(&acc, fake_mind_id, fake_char_id);

		acc.setup_insertActiveCharacters(std::move(ea));

		assert(!acc.getActiveCharacters().empty());

		acc.test_internalDeactivateCharacter(fake_char_id);

		assert(acc.getActiveCharacters().empty());
	}

	// Test sightCharacter() with empty sight
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Sight sight;
		SignalFlagger gotCharacterInfo_checker;

		acc.setup_setDoingCharacterRefresh(true);
		acc.GotCharacterInfo.connect(sigc::hide(sigc::mem_fun(gotCharacterInfo_checker,
															  &SignalFlagger::set)));

		acc.test_sightCharacter(sight);

		assert(!gotCharacterInfo_checker.flagged());
	}

	// Test sightCharacter() with Sight with bad args
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Sight sight;
		Atlas::Objects::Root bad_arg;
		SignalFlagger gotCharacterInfo_checker;

		acc.setup_setDoingCharacterRefresh(true);
		acc.GotCharacterInfo.connect(sigc::hide(sigc::mem_fun(gotCharacterInfo_checker,
															  &SignalFlagger::set)));
		sight->setArgs1(bad_arg);

		acc.test_sightCharacter(sight);

		assert(!gotCharacterInfo_checker.flagged());
	}

	// Test sightCharacter() with Sight with valid args
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Sight sight;
		Atlas::Objects::Entity::Anonymous sight_arg;
		SignalFlagger gotCharacterInfo_checker, gotAllCharacters_checker;

		acc.setup_setDoingCharacterRefresh(true);
		acc.GotCharacterInfo.connect(sigc::hide(sigc::mem_fun(gotCharacterInfo_checker,
															  &SignalFlagger::set)));
		acc.GotAllCharacters.connect(sigc::mem_fun(gotAllCharacters_checker,
												   &SignalFlagger::set));
		sight->setArgs1(sight_arg);
		acc.setup_fakeCharacter("1");

		acc.test_sightCharacter(sight);

		assert(gotCharacterInfo_checker.flagged());
		assert(gotAllCharacters_checker.flagged());
	}

	// Test sightCharacter() with Sight with valid args, but one character reamining
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		Atlas::Objects::Operation::Sight sight;
		Atlas::Objects::Entity::Anonymous sight_arg;
		SignalFlagger gotCharacterInfo_checker, gotAllCharacters_checker;

		acc.setup_setDoingCharacterRefresh(true);
		acc.GotCharacterInfo.connect(sigc::hide(sigc::mem_fun(gotCharacterInfo_checker,
															  &SignalFlagger::set)));
		acc.GotAllCharacters.connect(sigc::mem_fun(gotAllCharacters_checker,
												   &SignalFlagger::set));
		sight->setArgs1(sight_arg);
		acc.setup_fakeCharacter("1");
		acc.setup_fakeCharacter("2");

		acc.test_sightCharacter(sight);

		assert(gotCharacterInfo_checker.flagged());
		assert(!gotAllCharacters_checker.flagged());
	}

	// Test netConnected() Does nothing with an empty password
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.test_netConnected();
	}

	// Test netConnected() Does nothing with valid username and password,
	// but not DISCONNECTED
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.setup_setUsername("foo");
		acc.setup_setPassword("foo");
		acc.setup_setStatus(TestAccount::LOGGING_IN);

		acc.test_netConnected();
	}

	// Test netConnected() works with valid username and password,
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.setup_setUsername("foo");
		acc.setup_setPassword("foo");

		acc.test_netConnected();
	}

	// Test netDisconnecting() when account is not logged in.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		bool res = acc.test_netDisconnecting();

		assert(res);
	}

	// Test netDisconnecting() when account is logged in, but connection is
	// disconnected.
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.setup_setStatus(TestAccount::LOGGED_IN);

		// The fact that the connection is not really connected will be
		// caught in Account::logout() safely.
		bool res = acc.test_netDisconnecting();

		assert(!res);

		acc.setup_setStatus(TestAccount::DISCONNECTED);
	}

	// Test netFailure()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);

		acc.test_netFailure("foo");
	}

	// Test handleLogoutTimeout()
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger logoutComplete_checker;

		acc.setup_setStatus(TestAccount::LOGGED_IN);
		acc.LogoutComplete.connect(sigc::hide(sigc::mem_fun(logoutComplete_checker, &SignalFlagger::set)));

		acc.test_handleLogoutTimeout();

		assert(acc.query_getStatus() == TestAccount::DISCONNECTED);
		assert(logoutComplete_checker.flagged());
	}

	// Test avatarLogoutResponse() with non Info operation
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger avatarDeactivated_checker;
		Atlas::Objects::Operation::Get op;

		acc.AvatarDeactivated.connect(sigc::hide(sigc::mem_fun(avatarDeactivated_checker, &SignalFlagger::set)));

		acc.test_avatarLogoutResponse(op);

		assert(!avatarDeactivated_checker.flagged());
	}

	// Test avatarLogoutResponse() with Info operation
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger avatarDeactivated_checker;
		Atlas::Objects::Operation::Info op;

		acc.AvatarDeactivated.connect(sigc::hide(sigc::mem_fun(avatarDeactivated_checker, &SignalFlagger::set)));

		acc.test_avatarLogoutResponse(op);

		assert(!avatarDeactivated_checker.flagged());
	}

	// Test avatarLogoutResponse() with Info operation with bad args
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger avatarDeactivated_checker;
		Atlas::Objects::Operation::Info op;
		Atlas::Objects::Root bad_arg;
		op->setArgs1(bad_arg);

		acc.AvatarDeactivated.connect(sigc::hide(sigc::mem_fun(avatarDeactivated_checker, &SignalFlagger::set)));

		acc.test_avatarLogoutResponse(op);

		assert(!avatarDeactivated_checker.flagged());
	}

	// Test avatarLogoutResponse() with Info operation with logout args, which
	// have no args
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger avatarDeactivated_checker;
		Atlas::Objects::Operation::Info op;
		Atlas::Objects::Operation::Logout info_arg;

		op->setArgs1(info_arg);

		acc.AvatarDeactivated.connect(sigc::hide(sigc::mem_fun(avatarDeactivated_checker, &SignalFlagger::set)));

		acc.test_avatarLogoutResponse(op);

		assert(!avatarDeactivated_checker.flagged());
	}

	// Test avatarLogoutResponse() with Info operation with completely well
	// formed args for an ID which does not exist
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger avatarDeactivated_checker;
		Atlas::Objects::Operation::Info op;
		Atlas::Objects::Operation::Logout info_arg;
		Atlas::Objects::Entity::Anonymous logout_arg;
		std::string fake_char_id("1");

		op->setArgs1(info_arg);
		info_arg->setArgs1(logout_arg);
		logout_arg->setId(fake_char_id);

		acc.AvatarDeactivated.connect(sigc::hide(sigc::mem_fun(avatarDeactivated_checker, &SignalFlagger::set)));

		acc.test_avatarLogoutResponse(op);

		assert(!avatarDeactivated_checker.flagged());
	}

	// Test avatarLogoutResponse() with Info operation with completely well
	// formed args for an ID which is one of our characters, but not currently
	// active
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger avatarDeactivated_checker;
		Atlas::Objects::Operation::Info op;
		Atlas::Objects::Operation::Logout info_arg;
		Atlas::Objects::Entity::Anonymous logout_arg;
		std::string fake_char_id("1");

		op->setArgs1(info_arg);
		info_arg->setArgs1(logout_arg);
		logout_arg->setId(fake_char_id);
		acc.setup_fakeCharacter(fake_char_id);
		acc.AvatarDeactivated.connect(sigc::hide(sigc::mem_fun(avatarDeactivated_checker, &SignalFlagger::set)));

		acc.test_avatarLogoutResponse(op);

		assert(!avatarDeactivated_checker.flagged());
	}

	// Test avatarLogoutResponse() with Info operation with completely well
	// formed args for an ID which is one of our characters, and currently
	// active
	{
		TestConnection con("name", "localhost",
												 6767);

		TestAccount acc(con);
		SignalFlagger avatarDeactivated_checker;
		Atlas::Objects::Operation::Info op;
		Atlas::Objects::Operation::Logout info_arg;
		Atlas::Objects::Entity::Anonymous logout_arg;
		std::string fake_char_id("1");
		std::string fake_mind_id("1");
		auto ea = std::make_unique<TestAvatar>(&acc, fake_mind_id, fake_char_id);

		op->setArgs1(info_arg);
		info_arg->setArgs1(logout_arg);
		logout_arg->setId(fake_char_id);
		acc.setup_fakeCharacter(fake_char_id);
		acc.setup_insertActiveCharacters(std::move(ea));
		acc.AvatarDeactivated.connect(sigc::hide(sigc::mem_fun(avatarDeactivated_checker, &SignalFlagger::set)));

		acc.test_avatarLogoutResponse(op);

		assert(avatarDeactivated_checker.flagged());
	}


	return 0;
}
