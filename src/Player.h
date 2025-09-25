#pragma once

#include <chrono>
#include <expected>

#include "Commands.h"
#include "GameCallbacks.h"
#include "Map.h"
#include "Swoq.hpp"
#include "ThreadSafe.h"

namespace Bot
{
  using Swoq::Interface::DirectedAction;

  struct PlayerState
  {
    Offset                                position{0, 0};
    DirectedAction                        next;
    std::vector<Offset>                   reversedPath;
    std::size_t                           pathLength         = 0;
    std::chrono::steady_clock::time_point lastCommandTime    = std::chrono::steady_clock::now();
    bool                                  terminateRequested = false;
  };

  class Player
  {
  public:
    Player(int id, GameCallbacks& callbacks, std::unique_ptr<Swoq::Game> game, ThreadSafe<std::shared_ptr<const Map>>& map);
    std::expected<void, std::string> Run();

    PlayerState State() { return m_state.Get(); }
    void        SetCommands(Commands commands);
    void        SetCommand(Command command);

  private:
    std::expected<bool, std::string> VisitTiles(const std::set<Tile>& tiles);
    std::expected<bool, std::string> TerminateRequested();
    bool                             UpdateMap();
    std::expected<void, std::string> UpdatePlan();
    std::expected<bool, std::string> DoCommandIfAny();
    bool                             WaitForCommands();

    int                                     m_id;
    GameCallbacks&                          m_callbacks;
    std::unique_ptr<Swoq::Game>             m_game;
    ThreadSafe<std::shared_ptr<const Map>>& m_map;
    int                                     m_level = -1;
    ThreadSafe<PlayerState>                 m_state;
    ThreadSafe<Commands>                    m_commands;
  };

} // namespace Bot
