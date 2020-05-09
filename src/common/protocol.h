// protocol definition
#pragma once

#include "game.h" // GameLogicState

static const int ServerUdpPort = 0xACE1;
static const auto GamePeriodMs = 50;
static const int MTU = 1472;

enum Op
{
  // client-to-server messages
  KeepAlive,
  Disconnect,
  PlayerInput,
  Restart,

  // server-to-client messages
  State,
};

struct PacketHeader
{
  Op op;
};

struct PacketKeepAlive
{
  PacketHeader hdr;
};
static_assert(sizeof(PacketKeepAlive) < MTU);

struct PacketState
{
  PacketHeader hdr;
  GameLogicState state;
};
static_assert(sizeof(PacketState) < MTU);

struct PacketPlayerInput
{
  PacketHeader hdr;
  PlayerInputState input;
};
static_assert(sizeof(PacketPlayerInput) < MTU);

struct PacketRestart
{
  PacketHeader hdr;
};
static_assert(sizeof(PacketRestart) < MTU);

