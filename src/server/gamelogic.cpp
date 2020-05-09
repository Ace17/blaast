#include "game.h"
#include "protocol.h" // GamePeriodMs
#include <cmath>
#include <cstring> // memcpy

namespace
{
PlayerInputState lastInputs[MAX_HEROES];
int intergameTimer = 0;

struct FlameCoverage
{
  bool inflames[GameLogicState::ROWS][GameLogicState::COLS] {};
};

Vec2i round(Vec2f v)
{
  return Vec2i{ (int)::round(v.x), (int)::round(v.y) };
}

bool isThereABombHere(GameLogicState& state, Vec2i pos)
{
  for(auto& b : state.bombs)
    if(b.enable && round(b.pos) == pos)
      return true;

  return false;
}

GameLogicState::Bomb* allocBomb(GameLogicState& state)
{
  for(auto& b : state.bombs)
    if(!b.enable)
      return &b;

  return nullptr;
}

GameLogicState::Item* allocItem(GameLogicState& state)
{
  for(auto& i : state.items)
    if(!i.enable)
      return &i;

  return nullptr;
}

bool isInside(const GameLogicState& state, int row, int col)
{
  if(row < 0 || row >= state.ROWS)
    return false;

  if(col < 0 || col >= state.COLS)
    return false;

  return true;
}

bool isTraversable(const GameLogicState& state, int row, int col)
{
  return isInside(state, row, col) && state.board[row][col] == 0;
}

bool isDestroyable(const GameLogicState& state, int row, int col)
{
  return isInside(state, row, col) && state.board[row][col] == 2;
}

bool isTraversable(const GameLogicState& state, Vec2i pos)
{
  return isTraversable(state, pos.y, pos.x);
}

bool isTraversable(const GameLogicState& state, Vec2f pos)
{
  return isTraversable(state, round(pos));
}

bool isDestroyable(const GameLogicState& state, Vec2i pos)
{
  return isDestroyable(state, pos.y, pos.x);
}

float sqrLen(Vec2f v)
{
  return v * v;
}

int scan(const GameLogicState& state, Vec2i pos, Vec2i dir, int maxSteps)
{
  int r = 0;

  while(maxSteps >= 0 && isTraversable(state, pos))
  {
    pos = pos + dir;
    --maxSteps;
    ++r;
  }

  return r;
}

void updateBombs(GameLogicState& state, const FlameCoverage& flames)
{
  for(auto& b : state.bombs)
  {
    if(!b.enable)
      continue;

    // flame-triggered explosion
    if(b.countdown > 10 && flames.inflames[(int)b.pos.y][(int)b.pos.x])
      b.countdown = 10;

    if(b.countdown > 0)
    {
      b.countdown--;

      if(b.countdown == 0)
      {
        b.enable = false;

        auto scan = [&] (Vec2i pos, Vec2i dir, int maxSteps)
          {
            int n = ::scan(state, pos, dir, maxSteps);
            auto finalPos = pos + dir * n;

            if(n <= maxSteps && isDestroyable(state, finalPos))
            {
              state.board[finalPos.y][finalPos.x] = 0;

              if(rand() % 3 == 0)
              {
                if(auto item = allocItem(state))
                {
                  item->enable = 1;
                  item->row = finalPos.y;
                  item->col = finalPos.x;
                  item->type = 1 + rand() % (MAX_ITEM - ITEM_UNDEF);
                }
              }
            }
          };

        const Vec2i pos0 = { (int)b.pos.x, (int)b.pos.y };
        const int flamelength = state.heroes[b.ownerIndex].flamelength;
        scan(pos0, { 1, 0 }, flamelength);
        scan(pos0, { -1, 0 }, flamelength);
        scan(pos0, { 0, 1 }, flamelength);
        scan(pos0, { 0, -1 }, flamelength);
      }
    }
  }
}

FlameCoverage computeFlameCoverage(const GameLogicState& state)
{
  FlameCoverage r;

  // compute board cells covered with flames
  for(auto& b : state.bombs)
  {
    if(!b.enable)
      continue;

    if(b.countdown <= 10) // bomb is currently exploding
    {
      auto scan = [&] (Vec2i pos, Vec2i dir, int maxSteps)
        {
          int n = ::scan(state, pos, dir, maxSteps);

          for(int i = 0; i < n; ++i)
          {
            auto pos2 = pos + dir * i;
            r.inflames[pos2.y][pos2.x] = true;
          }
        };

      const Vec2i pos0 = { (int)b.pos.x, (int)b.pos.y };
      const int flamelength = state.heroes[b.ownerIndex].flamelength;
      scan(pos0, { 1, 0 }, flamelength);
      scan(pos0, { -1, 0 }, flamelength);
      scan(pos0, { 0, 1 }, flamelength);
      scan(pos0, { 0, -1 }, flamelength);
    }
  }

  return r;
}
}

GameLogicState initGame()
{
  GameLogicState state {};

  const Vec2f startingPositions[MAX_HEROES] =
  {
    { 0, 0 },
    { state.COLS - 1, 0 },
    { state.COLS - 1, state.ROWS - 1 },
    { 0, state.ROWS - 1 },
  };

  for(int i = 0; i < 4; ++i)
  {
    state.heroes[i].enable = true;
    state.heroes[i].maxbombs = 1;
    state.heroes[i].pos = startingPositions[i];
    state.heroes[i].walkspeed = 2;
    state.heroes[i].flamelength = 2;
  }

  for(int row = 0; row < state.ROWS; ++row)
  {
    for(int col = 0; col < state.COLS; ++col)
    {
      if(row % 2 && col % 2)
        state.board[row][col] = 1;
      else
        state.board[row][col] = 2;
    }
  }

  auto maybeClear = [&] (int row, int col)
    {
      if(row < 0 || row >= state.ROWS)
        return;

      if(col < 0 || col >= state.COLS)
        return;

      state.board[row][col] = 0;
    };

  auto clearCross = [&] (int row, int col)
    {
      maybeClear(row - 2, col);
      maybeClear(row - 1, col);
      maybeClear(row, col);
      maybeClear(row + 1, col);
      maybeClear(row + 2, col);
      maybeClear(row, col - 2);
      maybeClear(row, col - 1);
      maybeClear(row, col + 1);
      maybeClear(row, col + 2);
    };

  clearCross(0, 0);
  clearCross(state.ROWS - 1, 0);
  clearCross(state.ROWS - 1, state.COLS - 1);
  clearCross(0, state.COLS - 1);

  for(int i = 0; i < 0; ++i)
    clearCross(rand() % state.ROWS, rand() % state.COLS);

  return state;
}

GameLogicState advanceGameLogic(GameLogicState state, PlayerInputState inputs[MAX_HEROES])
{
  if(intergameTimer > 0)
  {
    intergameTimer--;

    if(intergameTimer > 0)
      return state;

    printf("New game\n");
    state = initGame();
  }

  auto const flames = computeFlameCoverage(state);

  auto activeBombCount = [&] (int heroIdx)
    {
      int r = 0;

      for(auto& b : state.bombs)
        if(b.enable && b.ownerIndex == heroIdx)
          r++;

      return r;
    };

  auto isRectColliding = [&] (Vec2f pos, Vec2f size)
    {
      if(!isTraversable(state, pos))
        return true;

      if(!isTraversable(state, pos + Vec2f(size.x, 0)))
        return true;

      if(!isTraversable(state, pos + Vec2f(0, size.y)))
        return true;

      if(!isTraversable(state, pos + size))
        return true;

      return false;
    };

  auto directMove = [&] (Vec2f& pos, Vec2f size, Vec2f delta) -> bool
    {
      auto newPos = pos + delta;

      if(isRectColliding(newPos - size * 0.5, size))
        return false;

      for(auto& b : state.bombs)
      {
        if(b.enable)
        {
          auto newDist = sqrLen(b.pos - newPos);

          if(newDist < 1.0 && sqrLen(b.pos - pos) >= 1.0)
            return false;

          if(newDist < 0.5 && sqrLen(b.pos - pos) >= 0.5)
            return false;
        }
      }

      pos = newPos;
      return true;
    };

  auto moveHero = [&] (GameLogicState::Hero& h, PlayerInputState input)
    {
      auto speed = 3.0 + h.walkspeed * 0.3;

      Vec2f vel = Vec2f::zero();

      if(input.left)
        vel.x -= speed;

      if(input.right)
        vel.x += speed;

      if(input.up)
        vel.y -= speed;

      if(input.down)
        vel.y += speed;

      auto dt = (GamePeriodMs / 1000.0f);
      auto delta = vel * dt;
      auto size = Vec2f(1, 1) * 0.7;

      static auto sign = [] (float f) { return f < 0 ? -1 : +1; };

      if(delta.x)
      {
        if(!directMove(h.pos, size, Vec2f(delta.x, 0)))
        {
          auto topPos = h.pos + Vec2f(delta.x, 0) + Vec2f(sign(delta.x) * 0.5, -0.6);
          auto botPos = h.pos + Vec2f(delta.x, 0) + Vec2f(sign(delta.x) * 0.5, +0.6);
          bool topClear = isTraversable(state, topPos);
          bool botClear = isTraversable(state, botPos);

          if(topClear || botClear)
          {
            auto dy = botClear ? 1 : -1;
            directMove(h.pos, size, Vec2f(0, speed * dy * dt));
          }
        }

        h.orientation = delta.x > 0 ? 0 : 2;
      }

      if(delta.y)
      {
        if(!directMove(h.pos, size, Vec2f(0, delta.y)))
        {
          auto topPos = h.pos + Vec2f(0, delta.y) + Vec2f(-0.6, sign(delta.y) * 0.5);
          auto botPos = h.pos + Vec2f(0, delta.y) + Vec2f(+0.6, sign(delta.y) * 0.5);
          bool topClear = isTraversable(state, topPos);
          bool botClear = isTraversable(state, botPos);

          if(topClear || botClear)
          {
            auto dx = botClear ? 1 : -1;
            directMove(h.pos, size, Vec2f(speed * dx * dt, 0));
          }
        }

        h.orientation = delta.y > 0 ? 1 : 3;
      }
    };

  int survivorCount = 0;

  for(auto& h : state.heroes)
  {
    if(!h.enable || h.dead)
      continue;

    survivorCount++;

    const int idx = int(&h - state.heroes);
    const auto& input = inputs[idx];
    const auto& prevInput = lastInputs[idx];

    auto roundPos = round(h.pos);

    for(auto& i : state.items)
    {
      if(!i.enable)
        continue;

      auto ipos = Vec2i{ i.col, i.row };

      if(roundPos == ipos)
      {
        i.enable = false;
        switch(i.type)
        {
        case ITEM_DISEASE:
          break;
        case ITEM_KICK:
          h.upgrades |= UPGRADE_KICK;
          break;
        case ITEM_FLAME:
          h.flamelength = std::min(h.flamelength + 1, 15);
          break;
        case ITEM_PUNCH:
          h.upgrades |= UPGRADE_PUNCH;
          break;
        case ITEM_SKATE:
          h.walkspeed = std::min(h.walkspeed + 1, 15);
          break;
        case ITEM_BOMB:
          h.maxbombs = std::min(h.maxbombs + 1, 15);
          break;
        case ITEM_TRIBOMB:
          h.upgrades |= UPGRADE_TRIBOMB;
          break;
        case ITEM_GOLDFLAME:
          h.flamelength = 15;
          break;
        case ITEM_EBOLA: break;
        case ITEM_TRIGGER: break;
        case ITEM_RANDOM: break;
        case ITEM_JELLY:
          h.upgrades |= UPGRADE_JELLY;
          break;
        case ITEM_GLOVE:
          h.upgrades |= UPGRADE_GLOVE;
          break;
        }
      }
    }

    if(flames.inflames[roundPos.y][roundPos.x])
    {
      printf("Killed!\n");
      h.dead = true;
      continue;
    }

    moveHero(h, input);

    if(input.dropBomb && !prevInput.dropBomb && activeBombCount(idx) < h.maxbombs)
    {
      auto pos = round(h.pos);

      if(!isThereABombHere(state, pos))
      {
        if(auto bomb = allocBomb(state))
        {
          bomb->enable = true;
          bomb->pos = { (float)pos.x, (float)pos.y };
          bomb->countdown = 75;
          bomb->ownerIndex = idx;
        }
      }
    }
  }

  if(survivorCount <= 1)
  {
    printf("Game over!\n");
    intergameTimer = 30;
  }

  updateBombs(state, flames);

  memcpy(lastInputs, inputs, sizeof lastInputs);
  return state;
}

