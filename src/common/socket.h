#pragma once

#include <stdint.h>
#include <string>

#include "address.h"
#include "span.h"

class Socket
{
public:
  Socket(int port);
  ~Socket();

  void send(Address dstAddr, Span<const uint8_t> packet);
  int recv(Address& sender, Span<uint8_t> buffer);
  int port() const;

  static Address resolve(String hostname, int port);

private:
  int m_sock;
};

