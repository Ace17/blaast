#pragma once

#include "socket.h"
#include <memory>

struct ITickable
{
  virtual ~ITickable() = default;
  virtual void tick() = 0;
};

std::unique_ptr<ITickable> createServer(Socket& sock);

