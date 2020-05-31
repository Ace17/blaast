#include "app.h"
#include "protocol.h"
#include "scenes.h"
#include "socket.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>

Socket g_sock(0);

int GetTicks()
{
  static auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();

  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

namespace
{
Address g_address;
int lastSentPacketDate = 0;
SceneFuncStruct g_currScene { &sceneIngame };

template<typename T>
void sendPacket(const T& pkt)
{
  g_sock.send(g_address, &pkt, sizeof pkt);
  lastSentPacketDate = GetTicks();
}

void sendKeepAlive()
{
  const uint8_t pkt[] = { Op::KeepAlive };
  sendPacket(pkt);
}
}

void AppInit(Span<const String> args)
{
  if(args.len != 2)
  {
    fprintf(stderr, "Usage: %.*s <host>\n", args[0].len, args[0].data);
    throw std::runtime_error("Invalid command line");
  }

  const auto host = args[1];
  g_address = Socket::resolve(host, ServerUdpPort);
  printf("Connecting to: %.*s (%s)\n", host.len, host.data, g_address.toString().c_str());

  sendKeepAlive();
}

void AppExit()
{
  {
    const uint8_t pkt[] = { Op::Disconnect };
    g_sock.send(g_address, pkt, sizeof pkt);
  }
}

bool AppTick(SteamGui* ui, Span<const uint8_t> keys)
{
  if(keys[Key::Escape])
    return false;

  if(keys[Key::F2])
  {
    PacketRestart pkt {};
    pkt.hdr.op = Op::Restart;
    sendPacket(pkt);
  }

  // send inputs to server
  {
    PacketPlayerInput pkt {};
    pkt.hdr.op = Op::PlayerInput;
    pkt.input.left = keys[Key::Left];
    pkt.input.right = keys[Key::Right];
    pkt.input.up = keys[Key::Up];
    pkt.input.down = keys[Key::Down];
    pkt.input.dropBomb = keys[Key::Space];
    sendPacket(pkt);
  }

  auto now = GetTicks();

  if(now - lastSentPacketDate > 2000)
    sendKeepAlive();

  g_currScene = g_currScene.stateFunc(ui);
  return g_currScene.stateFunc != nullptr;
}

