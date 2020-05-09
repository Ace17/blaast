#include "socket.h"

#include <arpa/inet.h> // inet_addr
#include <assert.h>
#include <fcntl.h>
#include <unistd.h> // close

#include <stdexcept>
#include <string>

Socket::Socket(int port)
{
  m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if(m_sock <= 0)
    throw std::runtime_error("failed to create UDP socket");

  int recvSize = 0;
  int sendSize = 0;

  {
    const int size = 1024 * 1024;

    if(setsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0)
      printf("Can't set send buffer size to %d", size);

    if(setsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0)
      printf("Can't set recv buffer size to %d", size);

    socklen_t S = sizeof(sendSize);
    getsockopt(m_sock, SOL_SOCKET, SO_SNDBUF, &sendSize, &S);
    getsockopt(m_sock, SOL_SOCKET, SO_RCVBUF, &recvSize, &S);
  }

  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if(bind(m_sock, (const sockaddr*)&address, sizeof(sockaddr_in)) < 0)
    throw std::runtime_error("failed to bind socket");

  int nonBlocking = 1;

  if(fcntl(m_sock, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
    throw std::runtime_error("failed to set non-blocking");

  printf("Listening on: udp/%d (%dkb, %dkb)\n", this->port(), sendSize / 1024, recvSize / 1024);
}

Socket::~Socket()
{
  shutdown(m_sock, SHUT_WR);
  close(m_sock);
}

void Socket::send(Address dstAddr, const void* packet_data, int packet_size)
{
  sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(dstAddr.address);
  addr.sin_port = htons(dstAddr.port);

  int sent_bytes = sendto(m_sock, (const char*)packet_data, packet_size, 0, (sockaddr*)&addr, sizeof(sockaddr_in));

  if(sent_bytes != packet_size)
  {
    printf("failed to send packet: %d\n", errno);
  }
}

int Socket::recv(Address& sender, void* packet_data, int max_packet_size)
{
  sockaddr_in from;
  socklen_t fromLength = sizeof(from);

  int bytes = recvfrom(m_sock,
                       (char*)packet_data,
                       max_packet_size,
                       0,
                       (sockaddr*)&from,
                       &fromLength);

  if(bytes > 0)
  {
    sender.address = ntohl(from.sin_addr.s_addr);
    sender.port = ntohs(from.sin_port);
  }

  if(bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
    bytes = 0;

  return bytes;
}

int Socket::port() const
{
  struct sockaddr_in sin;
  socklen_t len = sizeof(sin);

  if(getsockname(m_sock, (struct sockaddr*)&sin, &len) == -1)
  {
    perror("getsockname");
    assert(0);
  }

  return ntohs(sin.sin_port);
}

