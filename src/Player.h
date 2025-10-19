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
    NavigationParameters                  navigationParameters;
    std::optional<Vector2d<Tile>>         currentView;
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
    void                             InitializeCommands();
    void                             InitializeLevel();
    void                             InitializeMap();
    bool                             UpdateMap();
    std::shared_ptr<const Map>       GetMap(bool silently = false);
    void                             InitializeNavigation();
    std::expected<bool, std::string> VisitTiles(const std::set<Tile>& tiles);
    std::expected<bool, std::string> Visit(Offset destination);
    std::expected<bool, std::string> OpenDoor(Offset destination, DoorColor color);
    std::expected<bool, std::string> FetchBoulder(Offset destination);
    std::expected<bool, std::string> DropBoulder();
    std::expected<bool, std::string> PlaceBoulderOnPressurePlate(Offset destination, DoorColor color);
    std::expected<bool, std::string> ReconsiderUncheckedBoulders();
    std::expected<bool, std::string> TerminateRequested();
    std::expected<bool, std::string> Wait();
    std::expected<bool, std::string> LeaveSquare(std::optional<Offset>& originalSquare);
    std::expected<bool, std::string> Execute(DropDoorOnEnemy& dropDoorOnEnemy);
    std::expected<void, std::string> UpdatePlan();
    std::expected<bool, std::string> DoCommandIfAny();
    bool                             WaitForCommands();
    void                             PrintMap();
    std::expected<void, std::string> StepAlongPath(ThreadSafeProxy<PlayerState>& state);
    std::expected<void, std::string> StepAlongPathOrUse(ThreadSafeProxy<PlayerState>& state);
    std::expected<bool, std::string> MoveToDestination(ThreadSafeProxy<PlayerState>& state);
    std::expected<bool, std::string> MoveAlongPathThenOpenDoor(ThreadSafeProxy<PlayerState>& state,
                                                               Offset                        destination,
                                                               DoorColor                     color,
                                                               std::shared_ptr<const Map>    map);
    std::expected<bool, std::string> MoveAlongPathThenUse(ThreadSafeProxy<PlayerState>& state,
                                                          std::shared_ptr<const Map>    map,
                                                          Tile                          expectedTileAfterUse,
                                                          std::string_view              message);

    template <typename Predicate, typename Callable>
    std::expected<bool, std::string>
      ComputePathAndThen(const std::shared_ptr<const Map>& map, Predicate&& predicate, Callable&& callable)
    {
      return ComputePathAndThen(map, std::nullopt, std::forward<Predicate>(predicate), std::forward<Callable>(callable));
    }

    template <typename Predicate, typename Callable>
    std::expected<bool, std::string> ComputePathAndThen(const std::shared_ptr<const Map>& map,
                                                        std::optional<Offset>             destination,
                                                        Predicate&&                       predicate,
                                                        Callable&&                        callable)
    {
      auto state          = m_state.Lock();
      auto weights        = WeightMap(*map, state->navigationParameters, destination);
      state->reversedPath = ReversedPath(weights, state->position, std::forward<Predicate>(predicate));
      state->pathLength   = state->reversedPath.size();

      return std::forward<Callable>(callable)(state);
    }


    int                                     m_id;
    GameCallbacks&                          m_callbacks;
    std::unique_ptr<Swoq::Game>             m_game;
    ThreadSafe<std::shared_ptr<const Map>>& m_map;
    int                                     m_level = -1;
    ThreadSafe<PlayerState>                 m_state;
    ThreadSafe<Commands>                    m_commands;
  };

} // namespace Bot
