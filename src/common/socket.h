#pragma once

#include <stdint.h>
#include <string>

#include "address.h"

class Socket
{
public:
  Socket(int port);
  ~Socket();

  void send(Address dstAddr, const void* packet_data, int packet_size);
  int recv(Address& sender, void* packet_data, int max_packet_size);
  int port() const;

private:
  int m_sock;
};

