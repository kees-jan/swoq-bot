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

  struct DoorParameters
  {
    bool avoidKey  = true;
    bool avoidDoor = true;
  };

  using DoorParameterMap = std::map<DoorColor, DoorParameters>;
  using DoorOpenMap      = std::map<DoorColor, bool>;

  struct NavigationParameters
  {
    DoorParameterMap doorParameters{DoorColors
                                    | std::views::transform([](auto color) { return std::pair(color, DoorParameters{}); })
                                    | std::ranges::to<DoorParameterMap>()};
    DoorOpenMap      openedDoors{DoorColors | std::views::transform([](auto color) { return std::pair(color, false); })
                            | std::ranges::to<DoorOpenMap>()};
  };

  struct PlayerState
  {

    Offset                                position{0, 0};
    DirectedAction                        next;
    std::vector<Offset>                   reversedPath;
    std::size_t                           pathLength         = 0;
    std::chrono::steady_clock::time_point lastCommandTime    = std::chrono::steady_clock::now();
    bool                                  terminateRequested = false;
    NavigationParameters                  navigationParameters;
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
    void                             InitializeLevel();
    void                             InitializeMap();
    void                             InitializeNavigation();
    std::expected<bool, std::string> VisitTiles(const std::set<Tile>& tiles);
    std::expected<bool, std::string> Visit(Offset destination);
    std::expected<bool, std::string> FetchKey(Offset destination, DoorColor color);
    std::expected<bool, std::string> OpenDoor(Offset destination, DoorColor color);
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
