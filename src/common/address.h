#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string>

struct Address
{
  uint32_t address;
  int port;

  static Address build(const char* ipv4, int port)
  {
    int values[4];
    int n = sscanf(ipv4, "%d.%d.%d.%d", &values[0], &values[1], &values[2], &values[3]);

    if(n != 4)
      throw std::runtime_error("Invalid IP address: '" + std::string(ipv4) + "'");

    Address r {};

    for(auto& val : values)
    {
      r.address <<= 8;
      r.address |= val;
    }

    r.port = port;
    return r;
  }

  std::string toString() const
  {
    char buf[256];
    sprintf(buf, "%d.%d.%d.%d",
            (address >> 24) & 0xff,
            (address >> 16) & 0xff,
            (address >> 8) & 0xff,
            (address >> 0) & 0xff);
    return buf;
  }
};

