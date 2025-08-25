#ifndef ERIS_META_QUERY_H
#define ERIS_META_QUERY_H

#include "BaseConnection.h"

#include <wfmath/timestamp.h>

namespace Eris {

class Meta;

/** MetaQuery is a temporary connection used to retrieve information
about a game server. It issues an anoymous GET operation, and expects
to receive an INFO operation containing a 'server' entity in response. This
entity contains attributes such as the ruleset, uptime, number of connectec
players and so on. In addition, MetaQuery tracks the time the server
takes to response, and this estimates the server's ping. This time also
includes server latency. */

class MetaQuery : public BaseConnection {
public:
	MetaQuery(boost::asio::io_context& io_service,
			  Atlas::Bridge& bridge,
			  Meta& meta,
			  const std::string& host,
			  size_t index);

	~MetaQuery() override;

	/// return the serial-number of the query GET operation [for identification of replies]
	std::int64_t getQueryNo() const;

	size_t getServerIndex() const;

        /// Access the elapsed time (in milliseconds) since the query was issued
        std::int64_t getElapsed() const;

	bool isComplete() const;

	friend class Meta;

protected:
	void setComplete();

	/// Over-ride the default connection behaviour to issue the query
	void onConnect() override;

	void handleFailure(const std::string& msg) override;

	void handleTimeout(const std::string& msg) override;

	void onQueryTimeout();

	void dispatch() override;

	Meta& _meta;            ///< The Meta-server object which owns the query

	std::int64_t _queryNo;        ///< The serial number of the query GET
	WFMath::TimeStamp _stamp;    ///< Time stamp of the request, to estimate ping to server
	size_t m_serverIndex;
	bool m_complete;
	boost::asio::steady_timer m_completeTimer;
};

/// return the serial-number of the query GET operation [for identification of replies]
inline std::int64_t MetaQuery::getQueryNo() const {
	return _queryNo;
}

inline size_t MetaQuery::getServerIndex() const {
	return m_serverIndex;
}

inline bool MetaQuery::isComplete() const {
	return m_complete;
}


} // of namespace 

#endif
