#include <cstdio>
#include <cstring> // memcpy

#include "game.h"
#include "protocol.h"
#include "server.h"

extern GameLogicState advanceGameLogic(GameLogicState state, PlayerInputState inputs[MAX_HEROES]);
extern GameLogicState initGame();

namespace
{
// Remove an element from a vector. Might change the ordering.
template<typename T, typename Lambda>
void unstableRemove(std::vector<T>& container, Lambda predicate)
{
  for(int i = 0; i < (int)container.size(); ++i)
  {
    if(predicate(container[i]))
    {
      auto const j = (int)container.size() - 1;

      std::swap(container[i], container[j]);

      if(i != j)
        --i;

      container.pop_back();
    }
  }
}

int getPlayerIndex(GameSession const& session, Address address)
{
  for(auto& player : session.players)
    if(player.address.address == address.address)
      if(player.address.port == address.port)
        return int(&player - session.players.data());

  return -1;
}

int allocHero(const GameSession& session)
{
  bool heroInUse[MAX_HEROES] {};

  for(auto& player : session.players)
    heroInUse[player.heroIndex] = true;

  for(auto& inUse : heroInUse)
  {
    if(!inUse)
      return int(&inUse - heroInUse);
  }

  return -1;
}

struct Server : ITickable
{
  Server(Socket& sock_) : sock(sock_), state(initGame())
  {
    printf("State packet size: %d\n", (int)sizeof(PacketState));
  }

  static constexpr int MAX_WATCHDOG = 200;

  void tick() override
  {
    static auto isDead = [] (const GameSession::Player& p) { return p.watchdog > MAX_WATCHDOG; };

    while(processOneIncomingPacket())
    {
    }

    state = advanceGameLogic(state, inputs);

    // remove unresponsive network clients
    unstableRemove(session.players, isDead);
    broadcastNewState();
  }

  void broadcastNewState()
  {
    PacketState pkt;
    pkt.hdr.op = Op::State;
    pkt.state = state;

    for(auto& player : session.players)
    {
      static_assert(std::is_standard_layout<decltype(pkt)>::value);
      sock.send(player.address, &pkt, sizeof pkt);
      player.watchdog++;

      if(player.watchdog > MAX_WATCHDOG / 2)
        printf("Player #%d is not responding\n", int(&player - session.players.data()));
    }
  }

private:
  Socket& sock;
  GameSession session {};
  GameLogicState state;
  PlayerInputState inputs[MAX_HEROES] {};

  bool processOneIncomingPacket()
  {
    uint8_t buf[2048];
    Address from;
    int n = sock.recv(from, buf, sizeof buf);

    if(n == 0)
      return false;

    int idx = getPlayerIndex(session, from);

    if(idx == -1 && buf[0] == Op::KeepAlive)
    {
      idx = session.players.size();
      const int heroIdx = allocHero(session);

      if(heroIdx >= 0)
      {
        session.players.push_back({});
        auto& player = session.players.back();
        player.heroIndex = heroIdx;
        player.address = from;
        state.heroes[heroIdx].enable = true;
        printf("New player (#%d): %s\n", heroIdx, from.toString().c_str());
      }
      else
      {
        printf("Server is full\n");
        idx = -1;
      }
    }

    if(idx == -1)
    {
      printf("Skipping packet from unknown player: %s\n", from.toString().c_str());
      return true;
    }

    session.players[idx].watchdog = 0;
    switch(buf[0])
    {
    case Op::KeepAlive:
      break;
    case Op::Disconnect:
      session.players.erase(session.players.begin() + idx);
      printf("Player #%d has left\n", idx);
      break;
    case Op::PlayerInput:
      {
        auto pkt = (PacketPlayerInput*)buf;
        const int heroIdx = session.players[idx].heroIndex;
        memcpy(&inputs[heroIdx], &pkt->input, sizeof(PlayerInputState));
      }
      break;
    case Op::Restart:
      state = initGame();
      break;
    default:
      printf("Skipping unknown packet (Op=%d) from player: %s\n", buf[0], from.toString().c_str());
      break;
    }

    return true;
  };
};
}

std::unique_ptr<ITickable> createServer(Socket& sock)
{
  return std::make_unique<Server>(sock);
}

