// Shared state between clients and server.
#pragma once

#include <cstdint>
#include <vector>

#include "address.h"
#include "vec.h"

static const int MAX_HEROES = 8;

struct PlayerInputState
{
  bool left, right, up, down;
  bool dropBomb;
  bool action; // boxing-glove, detonate, etc.
};

struct GameSession
{
  struct Player
  {
    char name[16];
    int heroIndex;
    int watchdog;
    Address address;
  };

  std::vector<Player> players;
};

enum
{
  ITEM_UNDEF = 0,
  ITEM_DISEASE,
  ITEM_KICK,
  ITEM_FLAME,
  ITEM_PUNCH,
  ITEM_SKATE,
  ITEM_BOMB,
  ITEM_TRIBOMB,
  ITEM_GOLDFLAME,
  ITEM_EBOLA,
  ITEM_TRIGGER,
  ITEM_RANDOM,
  ITEM_JELLY,
  ITEM_GLOVE,
  MAX_ITEM,
};

enum
{
  UPGRADE_KICK = 1,
  UPGRADE_PUNCH = 2,
  UPGRADE_TRIBOMB = 4,
  UPGRADE_JELLY = 8,
  UPGRADE_GLOVE = 16,
};

// The gamestate, as seen by the server.
// This whole struct should fit in one UDP packet.
struct GameLogicState
{
  static constexpr int COLS = 15;
  static constexpr int ROWS = 11;

  struct Hero
  {
    Vec2f pos;
    uint16_t upgrades;
    uint8_t flamelength : 4;
    uint8_t walkspeed : 4;
    uint8_t maxbombs : 4;
    uint8_t orientation : 4; // left, up, right, down
    bool dead : 1;
    bool enable : 1;
    bool isHoldingBomb : 1;
  };

  struct Bomb
  {
    Vec2f pos;
    Vec2f vel; // kick, jelly-bomb
    int8_t countdown; // explodes at 10, removed at 0
    int8_t ownerIndex; // hero index, used to fetch properties
    bool enable;
  };

  struct Item
  {
    uint8_t row, col;
    uint8_t type;
    bool enable;
  };

  uint8_t board[ROWS][COLS];
  Hero heroes[MAX_HEROES];
  Bomb bombs[16];
  Item items[32];
};

