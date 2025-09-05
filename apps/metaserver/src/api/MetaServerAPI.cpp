#include "MetaServerAPI.hpp"

MetaServerAPI::MetaServerAPI(const std::string &host, unsigned short port)
    : m_ioContext(), m_endpoint(), m_socket(m_ioContext) {
  setEndpoint(host, port);
}

void MetaServerAPI::setEndpoint(const std::string &host, unsigned short port) {
  boost::system::error_code ec;
  if (m_socket.is_open()) {
    m_socket.close(ec);
  }
  m_endpoint = boost::asio::ip::udp::endpoint(
      boost::asio::ip::make_address(host, ec), port);
  m_socket.open(boost::asio::ip::udp::v4(), ec);
  if (ec) {
    throw boost::system::system_error(ec);
  }
  m_socket.connect(m_endpoint, ec);
  if (ec) {
    throw boost::system::system_error(ec);
  }
}

void MetaServerAPI::sendPacket(const MetaServerPacket &packet) {
  m_socket.send(
      boost::asio::buffer(packet.getBuffer().data(), packet.getSize()));
}

MetaServerPacket MetaServerAPI::receivePacket() {
  std::array<char, MAX_PACKET_BYTES> buf;
  boost::asio::ip::udp::endpoint sender;
  std::size_t len = m_socket.receive_from(boost::asio::buffer(buf), sender);
  MetaServerPacket packet(buf, len);
  packet.setAddress(sender.address().to_string(),
                    sender.address().to_v4().to_uint());
  packet.setPort(sender.port());
  return packet;
}
