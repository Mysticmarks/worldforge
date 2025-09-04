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

#ifndef DATAOBJECT_HPP_
#define DATAOBJECT_HPP_

/*
 * Local Includes
 */

/*
 * System Includes
 */
#include <string>
#include <map>
#include <list>
#include <vector>
#include <set>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <algorithm>

class DataObject {

public:
	DataObject();

	~DataObject();

	uint32_t addHandshake(unsigned int hs);

	uint32_t removeHandshake(unsigned int hs);

	bool handshakeExists(unsigned int hs);

	std::vector<unsigned int> expireHandshakes(unsigned int expiry = 0);

	boost::posix_time::ptime getHandshakeExpiry(unsigned int hs);

	bool addServerAttribute(const std::string& sessionid, const std::string& name, const std::string& value);

	void removeServerAttribute(const std::string& sessionid, const std::string& name);

	std::string getServerAttribute(const std::string& sessionid, const std::string& key);

	bool addClientAttribute(const std::string& sessionid, const std::string& name, const std::string& value);

	void removeClientAttribute(const std::string& sessionid, const std::string& name);

	std::string getClientAttribute(const std::string& sessionid, const std::string& key);

	bool addClientFilter(const std::string& sessionid, const std::string& name, const std::string& value);

	void removeClientFilter(const std::string& sessionid, const std::string& name);

	std::map<std::string, std::string> getClientFilter(const std::string& sessionid);

	std::string getClientFilter(const std::string& sessionid, const std::string& key);

	bool addServerSession(const std::string& sessionid);

	void removeServerSession(const std::string& sessionid);

	bool serverSessionExists(const std::string& sessionid);

	std::list<std::string> getServerSessionList(uint32_t start_idx, uint32_t max_items, std::string sessionid = "default");

	std::map<std::string, std::string> getServerSession(const std::string& sessionid);

	std::vector<std::string> expireServerSessions(unsigned int expiry = 0);

	std::list<std::string> searchServerSessionByAttribute(const std::string& attr_name, const std::string& attr_value);

	bool addClientSession(const std::string& sessionid);

	void removeClientSession(const std::string& sessionid);

	bool clientSessionExists(const std::string& sessionid);

	std::list<std::string> getClientSessionList();

	std::map<std::string, std::string> getClientSession(const std::string& sessionid);

	std::vector<std::string> expireClientSessions(unsigned int expiry = 0);

	std::vector<std::string> expireClientSessionCache(unsigned int expiry = 0);

	uint32_t getHandshakeCount();

	uint32_t getServerSessionCount(std::string s = "default");

	uint32_t getClientSessionCount();

	static boost::posix_time::ptime getNow();

	static std::string getNowStr();

	static unsigned int getLatency(boost::posix_time::ptime& t1, boost::posix_time::ptime& t2);

	uint32_t createServerSessionListresp(std::string ip = "default");

        std::list<std::string> getServerSessionCacheList();

        std::string getServerExpiryIso(std::string& sessionid);

        bool aclAdminCheck(const std::string& ip) const;
        void addAdminACL(const std::string& ip);

private:
	/**
	 *  Example Data Structure ( m_serverData )
	 *  "192.168.1.200" => {
	 *  	"serverVersion" => "0.5.20",
	 *  	"serverType" => "cyphesis",
	 *  	"serverUsers" => "100",
	 *  	"attribute1" => "value1",
	 *  	"attribute2" => "value2",
	 *  	"latency" => "200"
	 *  }
	 *
	 *  m_serverDataList contains an ordered representation of
	 *  m_serverData keys so that multiple LISTREQ requests can be
	 *  done and avoid duplicate servers packet responses.
	 */

	template<class T>
	bool keyExists(std::map<T, std::map<std::string, std::string> >& mapRef, T key) {
		if (mapRef.find(key) != mapRef.end()) {
			return true;
		}

		return false;
	}

	std::map<std::string, std::map<std::string, std::string> > m_serverData;
	std::map<std::string, std::vector<std::string> > m_serverListreq;
	std::map<std::string, std::string> m_listreqExpiry;

        std::map<std::string, std::map<std::string, std::string> > m_clientData;
        std::map<std::string, std::map<std::string, std::string> > m_clientFilterData;

        std::map<unsigned int, std::map<std::string, std::string> > m_handshakeQueue;

        std::set<std::string> m_adminACL;


};


#endif /* DATAOBJECT_HPP_ */
