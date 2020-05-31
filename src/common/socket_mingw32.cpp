#include "socket.h"

#include <assert.h>
#include <winsock2.h>
#include <ws2tcpip.h>// addrinfo

#include <stdexcept>
#include <string>

Socket::Socket(int port)
{
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);

  m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if(m_sock <= 0)
    throw std::runtime_error("failed to create UDP socket");

  int enable = 1;

  if(setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(int)) < 0)
    throw std::runtime_error("failed to reuse addr");

  sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if(bind(m_sock, (const sockaddr*)&address, sizeof(sockaddr_in)) < 0)
    throw std::runtime_error("failed to bind socket");

  // set non-blocking
  unsigned long Yes = TRUE;
  ioctlsocket(m_sock, FIONBIO, &Yes);

  printf("Listening on: udp/%d\n", this->port());
}

Socket::~Socket()
{
  closesocket(m_sock);
  WSACleanup();
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
    printf("failed to send packet\n");
    assert(0);
  }
}

int Socket::recv(Address& sender, void* packet_data, int max_packet_size)
{
  sockaddr_in from;
  int fromLength = sizeof(from);

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

  if(bytes == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
    return 0; // no data available

  return bytes;
}

int Socket::port() const
{
  struct sockaddr_in sin;
  int len = sizeof(sin);

  if(getsockname(m_sock, (struct sockaddr*)&sin, &len) == -1)
  {
    perror("getsockname");
    assert(0);
  }

  return ntohs(sin.sin_port);
}

Address Socket::resolve(String hostname, int port)
{
  Address r {};

  addrinfo hints {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  char zeroTerminatedHostName[256] {};
  memcpy(zeroTerminatedHostName, hostname.data, hostname.len);
  zeroTerminatedHostName[hostname.len] = 0;

  addrinfo* result;
  int s = getaddrinfo(zeroTerminatedHostName, nullptr, &hints, &result);

  if(s == -2)
    throw std::runtime_error("failed to resolve " + std::string(zeroTerminatedHostName));

  if(s != 0)
    printf("getaddrinfo returned error %d : %s\n", s, gai_strerror(s));

  assert(s == 0);

  auto p = result;

  while(p)
  {
    if(p->ai_family == AF_INET)
    {
      auto ipv4 = (sockaddr_in*)p->ai_addr;

      r = { ntohl(ipv4->sin_addr.s_addr), port };
    }

    p = p->ai_next;
  }

  freeaddrinfo(p);

  return r;
}

