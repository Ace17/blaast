#include "game.h"
#include "protocol.h" // GamePeriodMs
#include <cmath>
#include <cstring> // memcpy

namespace
{
// server-only state
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

GameLogicState::Bomb* findBombAt(GameLogicState& state, Vec2i pos)
{
  for(auto& b : state.bombs)
    if(b.enable && round(b.pos) == pos)
      return &b;

  return nullptr;
}

bool isThereABombHere(GameLogicState& state, Vec2i pos)
{
  return findBombAt(state, pos) != nullptr;
}

GameLogicState::Bomb* allocBomb(GameLogicState& state)
{
  for(auto& b : state.bombs)
    if(!b.enable)
      return &b;

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

static auto isRectColliding = [] (const GameLogicState& state, Vec2f pos, Vec2f size)
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

static auto sign = [] (float f) { return f ? (f < 0 ? -1.0f : +1.0f) : 0.0f; };

static auto directMove = [] (const GameLogicState& state, Vec2f& pos, Vec2f size, Vec2f delta) -> bool
  {
    auto newPos = pos + delta;

    if(isRectColliding(state, newPos - size * 0.5, size))
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

void updateBombs(GameLogicState& state, const FlameCoverage& flames)
{
  for(auto& b : state.bombs)
  {
    if(!b.enable)
      continue;

    {
      b.enable = false;

      if(!directMove(state, b.pos, Vec2f(0.9, 0.9), b.vel))
      {
        b.vel = { 0, 0 };
        b.pos.x = ::round(b.pos.x);
        b.pos.y = ::round(b.pos.y);
      }

      b.enable = true;
    }

    // flame-triggered explosion
    if(b.countdown > 10 && flames.inflames[(int)b.pos.y][(int)b.pos.x])
    {
      b.countdown = 10;
    }

    // bomb is exploding
    if(b.countdown <= 10)
    {
      b.vel = { 0, 0 };
      b.pos.x = ::round(b.pos.x);
      b.pos.y = ::round(b.pos.y);
    }

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
              state.board[finalPos.y][finalPos.x] = 0;
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

      const Vec2i pos0 = round(b.pos);
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

  // spawn items
  {
    static const int itemCounts[][2] =
    {
      { 10, ITEM_BOMB },
      { 10, ITEM_FLAME },
      { 3, ITEM_DISEASE },
      { 4, ITEM_KICK },
      { 8, ITEM_SKATE },
      { 2, ITEM_PUNCH },
      { 2, ITEM_GLOVE },
      { 1, ITEM_TRIBOMB },
      { 1, ITEM_JELLY },
      { -2, ITEM_GOLDFLAME },
      { -4, ITEM_TRIGGER },
      { -4, ITEM_EBOLA },
      { -2, ITEM_RANDOM },
    };

    for(auto& info : itemCounts)
    {
      const int itemType = info[1];
      int count = info[0];

      if(count < 0)
      {
        if(rand() % abs(count) == 0)
          count = 1;
        else
          count = 0;
      }

      for(int k = 0; k < count; ++k)
      {
        int watchdog = 0;
        Vec2i freePos;

        do
        {
          freePos.x = rand() % state.COLS;
          freePos.y = rand() % state.ROWS;

          if(++watchdog > 1000)
            break;
        }
        while (state.items[freePos.y][freePos.x] || state.board[freePos.y][freePos.x] != 2);

        state.items[freePos.y][freePos.x] = itemType;
      }
    }
  }

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

  auto pushMove = [&] (GameLogicState::Hero& h, Vec2f size, Vec2f delta) -> bool
    {
      auto blocked = !directMove(state, h.pos, size, delta);

      if(blocked && (h.upgrades & UPGRADE_KICK))
      {
        auto orthonormalize = [] (Vec2f v)
          {
            return Vec2f{ sign(v.x), sign(v.y) };
          };

        auto orthoDelta = orthonormalize(delta);

        if(auto bomb = findBombAt(state, round(h.pos + delta + orthoDelta * 0.5)))
          bomb->vel = orthoDelta * 0.3;
      }

      return !blocked;
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

      if(delta.x)
      {
        if(!pushMove(h, size, Vec2f(delta.x, 0)))
        {
          auto topPos = h.pos + Vec2f(delta.x, 0) + Vec2f(sign(delta.x) * 0.5, -0.6);
          auto botPos = h.pos + Vec2f(delta.x, 0) + Vec2f(sign(delta.x) * 0.5, +0.6);
          bool topClear = isTraversable(state, topPos);
          bool botClear = isTraversable(state, botPos);

          if(topClear || botClear)
          {
            auto dy = botClear ? 1 : -1;
            pushMove(h, size, Vec2f(0, speed * dy * dt));
          }
        }

        h.orientation = delta.x > 0 ? 0 : 2;
      }

      if(delta.y)
      {
        if(!pushMove(h, size, Vec2f(0, delta.y)))
        {
          auto topPos = h.pos + Vec2f(0, delta.y) + Vec2f(-0.6, sign(delta.y) * 0.5);
          auto botPos = h.pos + Vec2f(0, delta.y) + Vec2f(+0.6, sign(delta.y) * 0.5);
          bool topClear = isTraversable(state, topPos);
          bool botClear = isTraversable(state, botPos);

          if(topClear || botClear)
          {
            auto dx = botClear ? 1 : -1;
            pushMove(h, size, Vec2f(speed * dx * dt, 0));
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

    {
      const int itemType = state.items[roundPos.y][roundPos.x];
      switch(itemType)
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

      state.items[roundPos.y][roundPos.x] = 0;
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

