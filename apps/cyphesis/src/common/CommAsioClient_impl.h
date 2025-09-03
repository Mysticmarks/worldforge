/*
 Copyright (C) 2014 Erik Ogenvik

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

#ifndef COMMASIOCLIENT_IMPL_H_
#define COMMASIOCLIENT_IMPL_H_

#include "common/log.h"
#include "common/Monitors.h"

#include "CommAsioClient.h"
#include "Remotery.h"

#include <Atlas/Objects/Encoder.h>
#include <Atlas/Objects/SmartPtr.h>
#include <Atlas/Net/Stream.h>

#include <Atlas/Codecs/Bach.h>

#include <sstream>
#include <iostream>

static const bool comm_asio_client_debug_flag = false;


template<class ProtocolT>
CommAsioClient<ProtocolT>::CommAsioClient(std::string name,
										  boost::asio::io_context& io_context,
										  const Atlas::Objects::Factories& factories) :
		ObjectsDecoder(factories),
		CommSocket(io_context),
		mSocket(io_context),
		mWriteBuffer(std::make_unique<boost::asio::streambuf>()),
		mSendBuffer(std::make_unique<boost::asio::streambuf>()),
		mInStream(&mReadBuffer),
		mOutStream(mWriteBuffer.get()),
                mNegotiateTimer(io_context, std::chrono::seconds(1)),
                mIsSending(false),
                mShouldSend(false),
                mAutoFlush(false),
                m_throttleTimer(io_context),
                m_maxThrottledOps(256),
                m_initialBackoff(std::chrono::milliseconds(50)),
                m_currentBackoff(m_initialBackoff),
                mName(std::move(name)) {
}

template<class ProtocolT>
CommAsioClient<ProtocolT>::~CommAsioClient() {
	try {
		mSocket.shutdown(ProtocolT::socket::shutdown_both);
	} catch (const std::exception& e) {
	}
	try {
		mSocket.close();
	} catch (const std::exception& e) {
	}
}

template<class ProtocolT>
typename ProtocolT::socket& CommAsioClient<ProtocolT>::getSocket() {
	return mSocket;
}

namespace {
template<typename T>
std::string socketName(const T& socket);

template<>
std::string socketName(const boost::asio::local::stream_protocol::socket& socket) {
	try {
		return socket.local_endpoint().path();
	} catch (...) {
		return "<local socket removed>";
	}
}

template<>
std::string socketName(const boost::asio::ip::tcp::socket& socket) {
	try {
		return socket.remote_endpoint().address().to_string();
	} catch (...) {
		return "<remote disconnected>";
	}
}
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::do_read() {
	auto self(this->shared_from_this());
	mSocket.async_read_some(mReadBuffer.prepare(read_buffer_size),
							[this, self](boost::system::error_code ec, std::size_t length) {
								if (!ec) {
									rmt_ScopedCPUSample(read, 0)
									mReadBuffer.commit(length);
									m_codec->poll();
									if (m_active) {
										//By calling do_read again we make sure that the instance
										//doesn't go out of scope ("shared_from this"). As soon as that
										//doesn't happen, and there's no write in progress, the instance
										//will be deleted since there's no more references to it.
										this->do_read();
									}
								} else {
									//No need to read if connection has been actively shut down.
									if (m_active) {
										std::stringstream ss;
										spdlog::level::level_enum level = spdlog::level::warn;
										if (ec == boost::asio::error::eof) {
											ss << fmt::format("Connection at '{}' hung up unexpectedly.", socketName(mSocket));
											level = spdlog::level::debug;
										} else {
											ss << fmt::format("Error when reading from socket at '{}': (", socketName(mSocket)) << ec << ") " << ec.message();

										}
										spdlog::log(level, ss.str());
									}
								}
							});
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::write() {
	if (mWriteBuffer->size() != 0) {
		if (mIsSending) {
			//We're already sending in the background.
			//Make that we should send again once we've completed sending.
			mShouldSend = true;
			return;
		}

		mShouldSend = false;

		//We'll use a self reference to make sure that the client isn't deleted while sending.
		auto self(this->shared_from_this());
		//Swap places between writing buffer and sending buffer, and attach new write buffer to the out stream.
		std::swap(mWriteBuffer, mSendBuffer);
		mOutStream.rdbuf(mWriteBuffer.get());
		mIsSending = true;

		boost::asio::async_write(mSocket, *mSendBuffer,
								 [this, self](boost::system::error_code ec, std::size_t length) {
									 mIsSending = false;
									 if (!ec) {
										 rmt_ScopedCPUSample(write, 0)
										 mSendBuffer->consume(length);
										 //Is there data queued for transmission which we should send right away?
										 if (mShouldSend) {
											 this->write();
										 }
									 } else {
										 //No need to write if connection has been actively shut down.
										 if (m_active) {
											 std::stringstream ss;
											 spdlog::level::level_enum level = spdlog::level::warn;
											 if (ec == boost::asio::error::eof) {
												 ss << fmt::format("Connection at '{}' hung up unexpectedly.", socketName(mSocket));
												 level = spdlog::level::debug;
											 } else {
												 ss << fmt::format("Error when reading from socket at '{}': (", socketName(mSocket)) << ec << ") " << ec.message();

											 }
											 spdlog::log(level, ss.str());
										 }

									 }
								 });
	}
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::negotiate_read() {
	auto self(this->shared_from_this());
	mSocket.async_read_some(mReadBuffer.prepare(read_buffer_size),
							[this, self](boost::system::error_code ec, std::size_t length) {
								if (!ec && m_active) {
									mReadBuffer.commit(length);
									if (length > 0) {
										int negotiateResult = this->negotiate();
										if (negotiateResult < 0) {
											//this should remove any shared references and delete this instance
											return;
										}
									}

									//If the m_negotiate instance is removed we're done with negotiation and should start the main loop.
									if (m_negotiate == nullptr) {
										this->write();
										this->do_read();
									} else {
										this->negotiate_write();
										this->negotiate_read();
									}
								} else {
									//If connection is shut down, we should consider this as an aborted negotiaton
									m_negotiate.reset();
									mNegotiateTimer.cancel();
								}
							});
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::negotiate_write() {
	auto self(this->shared_from_this());

	if (mWriteBuffer->size() != 0) {
		boost::asio::async_write(mSocket, mWriteBuffer->data(),
								 [this, self](boost::system::error_code ec, std::size_t length) {
									 if (!ec && m_active) {
										 mWriteBuffer->consume(length);
									 }
								 });
	}
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::startAccept(std::unique_ptr<Link> connection) {
	// Create the server side negotiator
	m_negotiate = std::make_unique<Atlas::Net::StreamAccept>("cyphesis " + mName, mInStream, mOutStream);

	m_link = std::move(connection);

	startNegotiation();
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::startConnect(std::unique_ptr<Link> connection) {
	// Create the client side negotiator
	m_negotiate = std::make_unique<Atlas::Net::StreamConnect>("cyphesis " + mName, mInStream, mOutStream);

	m_link = std::move(connection);

	startNegotiation();
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::startNegotiation() {
	auto self(this->shared_from_this());
#if BOOST_VERSION >= 106600
	mNegotiateTimer.expires_after(std::chrono::seconds(10));
#else
	mNegotiateTimer.expires_from_now(std::chrono::seconds(10));
#endif
	mNegotiateTimer.async_wait([this, self](const boost::system::error_code& ec) {
		//If the negotiator still exists after the deadline it means that the negotiation hasn't
		//completed yet; we'll consider that a "timeout".
		if (m_negotiate != nullptr) {
			spdlog::debug("Client at '{}' disconnected because of negotiation timeout.", socketName(mSocket));
			mSocket.close();
		}
	});

	m_negotiate->poll();

	negotiate_write();
	negotiate_read();
}

template<class ProtocolT>
int CommAsioClient<ProtocolT>::negotiate() {
	// poll and check if negotiation is complete
	m_negotiate->poll();

	if (m_negotiate->getState() == Atlas::Negotiate::IN_PROGRESS) {
		return 0;
	}

	// Check if negotiation failed
	if (m_negotiate->getState() == Atlas::Negotiate::FAILED) {
		spdlog::debug("Failed to negotiate with client at '{}'.", socketName(mSocket));
		return -1;
	}
	// Negotiation was successful

	// Get the codec that negotiation established
	m_codec = m_negotiate->getCodec(*this);

	// Acceptor is now finished with
	m_negotiate.reset();

	if (m_codec == nullptr) {
		spdlog::debug("Could not create codec during negotiation with '{}'.", socketName(mSocket));
		return -1;
	}
	// Create a new encoder to send high level objects to the codec
	m_encoder = std::make_unique<Atlas::Objects::ObjectsEncoder>(*m_codec);

	assert(m_link != 0);
	m_link->setEncoder(m_encoder.get());

	// This should always be sent at the beginning of a session
	m_codec->streamBegin();

	m_link->notifyConnectionComplete();

	return 0;
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::externalOperation(Atlas::Objects::Operation::RootOperation op) {
        assert(m_link != 0);

        auto queueOp = [&](Atlas::Objects::Operation::RootOperation&& o) {
                if (m_throttledOps.size() >= m_maxThrottledOps) {
                        spdlog::warn("Dropping op '{}' due to full throttle queue (limit {})", o->getParent(), m_maxThrottledOps);
                        return;
                }
                m_throttledOps.emplace_back(std::move(o));
                Monitors::instance().insert("commasio_throttled_ops", (Atlas::Message::IntType) m_throttledOps.size());
                if (m_throttledOps.size() == 1) {
                        spdlog::warn("Throttling activated for '{}'", mName);
                        scheduleThrottleRetry();
                }
        };

        if (!m_throttledOps.empty()) {
                queueOp(std::move(op));
                return;
        }

        try {
                m_link->externalOperation(op, *m_link);
        } catch (const std::exception& e) {
                spdlog::debug("Link rejected op '{}': {}", op->getParent(), e.what());
                queueOp(std::move(op));
        }
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::drainThrottledOps() {
        while (!m_throttledOps.empty()) {
                try {
                        m_link->externalOperation(m_throttledOps.front(), *m_link);
                        m_throttledOps.pop_front();
                } catch (const std::exception& e) {
                        spdlog::debug("Link still throttled: {}", e.what());
                        Monitors::instance().insert("commasio_throttled_ops", (Atlas::Message::IntType) m_throttledOps.size());
                        scheduleThrottleRetry();
                        return;
                }
        }

        // Successfully drained queue
        m_currentBackoff = m_initialBackoff;
        Monitors::instance().insert("commasio_throttled_ops", (Atlas::Message::IntType) 0);
        spdlog::debug("Throttling cleared for '{}'", mName);
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::scheduleThrottleRetry() {
        auto self(this->shared_from_this());
        m_throttleTimer.expires_after(m_currentBackoff);
        m_throttleTimer.async_wait([this, self](const boost::system::error_code& ec) {
                if (!ec) {
                        drainThrottledOps();
                }
        });
        m_currentBackoff *= 2;
        if (m_currentBackoff > std::chrono::seconds(5)) {
                m_currentBackoff = std::chrono::seconds(5);
        }
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::objectArrived(Atlas::Objects::Root obj) {
	if (comm_asio_client_debug_flag) {
		std::stringstream debugStream;

		Atlas::Codecs::Bach debugCodec(debugStream, debugStream, *this /*dummy*/);
		Atlas::Objects::ObjectsEncoder debugEncoder(debugCodec);
		debugEncoder.streamObjectsMessage(obj);
		debugStream << std::flush;

		std::cerr << "received: " << debugStream.str() << std::endl;
	}

	auto op = Atlas::Objects::smart_dynamic_cast<Atlas::Objects::Operation::RootOperation>(obj);
	if (!op.isValid()) {
		spdlog::error("Invalid object of type \"{}\" with parent "
					  "\"{}\" arrived from client at '{}'", obj->getObjtype(),
					  obj->getParent(), socketName(mSocket));
	} else {
		externalOperation(std::move(op));
	}
}

template<class ProtocolT>
int CommAsioClient<ProtocolT>::send(const Atlas::Objects::Operation::RootOperation& op) {
	if (!mSocket.is_open()) {
		spdlog::error("Writing to closed client");
		return -1;
	}
	assert(m_encoder);


	if (comm_asio_client_debug_flag) {
		std::stringstream debugStream;

		Atlas::Codecs::Bach debugCodec(debugStream, debugStream, *this /*dummy*/);
		Atlas::Objects::ObjectsEncoder debugEncoder(debugCodec);
		debugEncoder.streamObjectsMessage(op);
		debugStream << std::flush;

		std::cerr << "sending: " << debugStream.str() << std::endl;
	}

	m_encoder->streamObjectsMessage(op);

	if (mAutoFlush) {
		return flush();
	} else {
		return 0;
	}
}

template<class ProtocolT>
void CommAsioClient<ProtocolT>::disconnect() {
	m_active = false;
	m_negotiate.reset();
	mNegotiateTimer.cancel();
	mSocket.cancel();
}

template<class ProtocolT>
int CommAsioClient<ProtocolT>::flush() {
	write();
	return 0;
}

#endif /* COMMASIOCLIENT_IMPL_H_ */
