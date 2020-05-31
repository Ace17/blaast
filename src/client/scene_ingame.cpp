#include "scenes.h"

#include "game.h"
#include "protocol.h"
#include "socket.h"
#include "sprite.h"
#include "stats.h"
#include "steamgui.h"
#include <cmath>
#include <cstring> // memcpy

// from main.cpp
extern std::vector<Sprite> g_Sprites;

// from app.cpp
extern Socket g_sock;
extern int GetTicks();

namespace
{
const int ServerTimeout = 2000;
int lastReceivedPacketDate = -ServerTimeout;

void drawScene(const GameLogicState& state)
{
  static const Vec2f TS2 = Vec2f(40, 36);

  auto transform = [] (Vec2f pos) -> Vec2f
    {
      return Vec2f(20, 67) + Vec2f{ pos.x* TS2.x, pos.y* TS2.y };
    };

  // Draw level
  for(int row = 0; row < state.ROWS; ++row)
  {
    for(int col = 0; col < state.COLS; ++col)
    {
      if(state.items[row][col])
      {
        g_Sprites.push_back({});
        Sprite& sprite = g_Sprites.back();
        sprite.tilenum = 3 + state.items[row][col];
        sprite.zorder = -0.1;
        sprite.pos = transform(Vec2f(col, row)) + Vec2f(TS2.x, TS2.y * 2) * 0.5;
      }

      if(state.board[row][col] == 0)
        continue;

      g_Sprites.push_back({});
      Sprite& sprite = g_Sprites.back();
      switch(state.board[row][col])
      {
      case 1: sprite.tilenum = 159;
        break;
      case 2: sprite.tilenum = 158;
        break;
      default:
        sprite.tilenum = 150;
        break;
      }

      sprite.pos = transform(Vec2f(col, row)) + Vec2f(TS2.x, TS2.y * 2) * 0.5;
      sprite.color = Vec4f(1, 1, 1, 1);
      sprite.zorder = 0;
    }
  }

  // draw players
  static const Vec4f colors[MAX_HEROES] =
  {
    Vec4f(1, 1, 1, 1),
    Vec4f(1, 0, 0, 1),
    Vec4f(0, 0, 1, 1),
    Vec4f(1, 1, 0, 1),
    Vec4f(0, 1, 1, 1),
    Vec4f(1, 0, 1, 1),
    Vec4f(0.5, 0.5, 1, 1),
    Vec4f(1, 0.5, 0.5, 1),
  };

  for(auto& hero : state.heroes)
  {
    if(!hero.enable)
      continue;

    const int idx = int(&hero - state.heroes);

    const int permute[] = { 0, 2, 3, 1 };
    g_Sprites.push_back({});
    Sprite& sprite = g_Sprites.back();
    sprite.tilenum = 74 + 15 * permute[hero.orientation] + (GetTicks() / 30) % 15;

    if(hero.dead)
      sprite.tilenum = 74;

    sprite.zorder = 2 + hero.pos.y / 100.0;
    sprite.pos = transform(hero.pos + Vec2f(1, 1.5) * 0.5);
    sprite.color = colors[idx];
    sprite.colormode = true;

    if(hero.dead)
    {
      sprite.colormode = false;
      sprite.color = { 0, 0, 0, 0.5 };
    }
  }

  // draw bombs
  for(auto& bomb : state.bombs)
  {
    if(!bomb.enable)
      continue;

    auto& owner = state.heroes[bomb.ownerIndex];

    // draw flame
    if(bomb.countdown <= 10)
    {
      auto drawFlame = [&state, &transform] (Vec2f origin, Vec2f dir, int maxSteps, int color)
        {
          auto traversable = [&] (int row, int col)
            {
              if(row < 0 || row >= state.ROWS)
                return false;

              if(col < 0 || col >= state.COLS)
                return false;

              return state.board[row][col] == 0;
            };

          int col = int(round(origin.x));
          int row = int(round(origin.y));
          int i = 0;

          while(traversable(row, col) && i <= maxSteps)
          {
            Vec2f pos = origin + dir * i;

            g_Sprites.push_back({});
            Sprite& sprite = g_Sprites.back();
            sprite.tilenum = 1490;
            sprite.zorder = 4;
            sprite.pos = transform(pos + Vec2f(1, 2) * 0.5);
            sprite.colormode = true;
            sprite.color = colors[color];

            ++i;
            row += dir.y;
            col += dir.x;
          }
        };

      drawFlame(bomb.pos, Vec2f(1, 0), owner.flamelength, bomb.ownerIndex);
      drawFlame(bomb.pos, Vec2f(-1, 0), owner.flamelength, bomb.ownerIndex);
      drawFlame(bomb.pos, Vec2f(0, 1), owner.flamelength, bomb.ownerIndex);
      drawFlame(bomb.pos, Vec2f(0, -1), owner.flamelength, bomb.ownerIndex);
    }
    else // draw bomb
    {
      g_Sprites.push_back({});
      Sprite& sprite = g_Sprites.back();

      if(bomb.jelly)
        sprite.tilenum = 144 + int((sin(GetTicks() * 0.001 * 3.14 * 2) * 0.5 + 0.5) * 6);
      else
        sprite.tilenum = 135 + int((sin(GetTicks() * 0.001 * 3.14 * 2) * 0.5 + 0.5) * 9);

      sprite.zorder = 2 + bomb.pos.y / 100.0;
      sprite.pos = transform(bomb.pos + Vec2f(1, 1.5) * 0.5);
      sprite.color = colors[bomb.ownerIndex];
      sprite.colormode = 1;
    }
  }
}

SceneFuncStruct gui(SteamGui* ui)
{
  const auto now = GetTicks();

  ui->text("BLAAST");
  ui->text("(in-game)");

  if(GetTicks() - lastReceivedPacketDate >= ServerTimeout)
    ui->text("No response from server");
  else
  {
    ui->text("Connected");
    char buf[256];
    ui->text(format(buf, "Last response: %.3fs ago", (now - lastReceivedPacketDate) / 1000.0));
  }

  ui->text("");

  if(ui->button("Quit"))
    return {};

  if(ui->button("Pause"))
    return { &scenePaused };

  {
    static float ftile = 0.5;
    static float fine = 0.5;
    ui->slider("Tile", ftile);
    ui->slider("FineTile", fine);

    g_Sprites.push_back({});
    Sprite& sprite = g_Sprites.back();
    sprite.tilenum = ftile * 1500 + (fine - 0.5) * 100;
    sprite.pos = { 1050, 250 };
    sprite.color = Vec4f(1, 1, 1, 1);
    sprite.zorder = 10;

    char buf[256];
    ui->text(format(buf, "tile: %d", sprite.tilenum));
  }

  ui->text("");
  ui->text("---- stats ----");

  for(int i = 0; i < getStatCount(); ++i)
  {
    auto stat = getStat(i);
    char buf[256];
    ui->text(format(buf, "%.*s: %.2f", stat.name.len, stat.name.data, stat.val));
  }

  return { &sceneIngame };
}
}

SceneFuncStruct sceneIngame(SteamGui* ui)
{
  static GameLogicState g_state;

  while(1)
  {
    uint8_t buffer[2048];
    Address unused;
    int n = g_sock.recv(unused, buffer);

    if(n == 0)
      break;

    lastReceivedPacketDate = GetTicks();

    if(buffer[0] == Op::State)
    {
      auto pkt = (PacketState*)buffer;
      memcpy(&g_state, &pkt->state, sizeof g_state);
    }
    else
    {
      printf("Unknown Op: %d\n", buffer[0]);
    }
  }

  drawScene(g_state);
  return gui(ui);
}

