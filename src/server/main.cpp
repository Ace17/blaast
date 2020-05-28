// game server:
// - actual game logic updating
// - state broadcasting
// - client (player) bookeeping
// Should depend only on file I/O and network (socket).
// No SDL/OpenGL is allowed here: this program must be able to run headless.
#include <chrono>
#include <cstdio>
#include <thread>
#include <type_traits>

#include "protocol.h"
#include "server.h"
#include "socket.h"
#include "span.h"

void safeMain(Span<const String> args)
{
  (void)args;
  Socket sock(ServerUdpPort);
  printf("Server listening on: udp/%d\n", sock.port());

  auto server = createServer(sock);

  for(;;)
  {
    server->tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(GamePeriodMs));
  }
}

