// Eris Online RPG Protocol Library
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

// Eris Metaserver integration test for fragmented responses
#include <Eris/Metaserver.h>
#include <Eris/EventService.h>

#include <boost/asio.hpp>
#include <arpa/inet.h>

#include <array>
#include <atomic>
#include <cassert>
#include <thread>

using Eris::Meta;

class FragmentingMetaserver {
public:
    FragmentingMetaserver()
        : m_socket(m_io, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)),
          m_running(true),
          m_thread([this]{ serve(); }) {}

    ~FragmentingMetaserver() {
        m_running = false;
        m_socket.close();
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    unsigned short port() const { return m_socket.local_endpoint().port(); }

private:
    void serve() {
        using boost::asio::ip::udp;
        udp::endpoint remote;
        std::array<char, 64> data{};
        while (m_running) {
            boost::system::error_code ec;
            std::size_t len = m_socket.receive_from(boost::asio::buffer(data), remote, 0, ec);
            if (ec) break;
            // respond with handshake
            uint32_t handshake[2] = {htonl(3), htonl(1)}; // HANDSHAKE + stamp
            m_socket.send_to(boost::asio::buffer(handshake), remote, 0, ec);
            if (ec) break;
            // clientshake
            m_socket.receive_from(boost::asio::buffer(data), remote, 0, ec);
            if (ec) break;
            // list request
            m_socket.receive_from(boost::asio::buffer(data), remote, 0, ec);
            if (ec) break;
            uint32_t ip1; inet_pton(AF_INET, "10.0.0.1", &ip1);
            uint32_t ip2; inet_pton(AF_INET, "10.0.0.2", &ip2);
            uint32_t ip3; inet_pton(AF_INET, "10.0.0.3", &ip3);
            uint32_t part1[] = {htonl(8), htonl(3), htonl(3), ip1, ip2};
            m_socket.send_to(boost::asio::buffer(part1), remote, 0, ec);
            if (ec) break;
            m_socket.send_to(boost::asio::buffer(&ip3, sizeof(ip3)), remote, 0, ec);
            break;
        }
    }

    boost::asio::io_context m_io;
    boost::asio::ip::udp::socket m_socket;
    std::atomic<bool> m_running;
    std::thread m_thread;
};

int main() {
    boost::asio::io_context io_service;
    Eris::EventService event_service(io_service);
    FragmentingMetaserver server;
    Meta m(io_service, event_service,
            boost::asio::ip::address_v4::loopback().to_string(), 20, server.port());

    bool failure = false;
    m.Failure.connect([&](const std::string&){ failure = true; });
    m.refresh();
    io_service.run_for(std::chrono::milliseconds(50));
    io_service.restart();
    assert(!failure);
    assert(m.getGameServerCount() == 3);
    return 0;
}
