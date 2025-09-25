#pragma once

#include <expected>

#include "Commands.h"
#include "GameCallbacks.h"
#include "Map.h"
#include "Swoq.hpp"
#include "ThreadSafe.h"

namespace Bot
{
  struct PlayerState
  {
    Offset              position{0, 0};
    std::vector<Offset> reversedPath;
    std::size_t         pathLength = 0;
  };

  class Player
  {
  public:
    Player(int id, GameCallbacks& callbacks, std::unique_ptr<Swoq::Game> game, ThreadSafe<std::shared_ptr<const Map>>& map);
    std::expected<void, std::string> Run();

    PlayerState State() { return m_state.Get(); }

  private:
    std::expected<bool, std::string> VisitTiles(const std::set<Tile>& tiles);
     bool                             UpdateMap();
    std::expected<bool, std::string> UpdatePlan();

    int                                     m_id;
    GameCallbacks&                          m_callbacks;
    std::unique_ptr<Swoq::Game>             m_game;
    ThreadSafe<std::shared_ptr<const Map>>& m_map;
    int                                     m_level = -1;
    ThreadSafe<PlayerState>                 m_state;
    ThreadSafe<Commands>                    m_commands;
  };

} // namespace Bot
