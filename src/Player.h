#pragma once

#include <chrono>
#include <expected>

#include "Commands.h"
#include "DungeonMap.h"
#include "GameCallbacks.h"
#include "Map.h"
#include "Swoq.hpp"
#include "ThreadSafe.h"

namespace Bot
{
  using Swoq::Interface::DirectedAction;

  struct PlayerState
  {

    Offset position{0, 0};
    DirectedAction next;
    std::vector<Offset> reversedPath;
    std::size_t pathLength = 0;
    std::chrono::steady_clock::time_point lastCommandTime = std::chrono::steady_clock::now();
    bool terminateRequested = false;
    NavigationParameters navigationParameters;
    std::optional<Vector2d<Tile>> currentView;
    bool hasSword = false;
  };

  class Player
  {
  public:
    Player(
      int id,
      GameCallbacks& callbacks,
      std::unique_ptr<Swoq::Game> game,
      ThreadSafe<DungeonMap::Ptr>& dungeonMap,
      ThreadSafe<std::shared_ptr<const Map>>& map);
    std::expected<void, std::string> Run();

    PlayerState State() { return m_state.Get(); }
    void SetCommands(Commands commands);
    void SetCommand(Command command);

  private:
    void InitializeCommands();
    void InitializeLevel();
    void InitializeMap();
    bool UpdateMap();
    std::shared_ptr<const Map> GetMap(bool silently = false);
    void InitializeNavigation();
    std::expected<bool, std::string> VisitTiles(const std::set<Tile>& tiles);
    std::expected<bool, std::string> Visit(Offset destination);
    std::expected<bool, std::string> OpenDoor(Bot::OpenDoor& door);
    std::expected<bool, std::string> FetchBoulder(Bot::FetchBoulder& fetchBoulder);
    std::expected<bool, std::string> DropBoulder(Bot::DropBoulder_t& dropBoulder);
    std::expected<bool, std::string> PlaceBoulderOnPressurePlate(Bot::PlaceBoulderOnPressurePlate& placeBoulder);
    std::expected<bool, std::string> ReconsiderUncheckedBoulders();
    std::expected<bool, std::string> TerminateRequested();
    std::expected<bool, std::string> Wait();
    std::expected<bool, std::string> LeaveSquare(std::optional<Offset>& originalSquare);
    std::expected<bool, std::string> Execute(DropDoorOnEnemy& dropDoorOnEnemy);
    std::expected<bool, std::string> PeekUnderEnemies(const OffsetSet& tileLocations);
    std::expected<bool, std::string> Explore();
    std::expected<void, std::string> UpdatePlan();
    std::expected<bool, std::string> DoCommandIfAny();
    bool WaitForCommands();
    void PrintMap();
    std::expected<void, std::string> StepAlongPath(ThreadSafeProxy<PlayerState>& state);
    std::expected<bool, std::string> StepAlongPathOrUse(ThreadSafeProxy<PlayerState>& state);
    std::expected<bool, std::string> MoveToDestination(ThreadSafeProxy<PlayerState>& state);
    std::expected<bool, std::string> MoveToDestination(ThreadSafeProxy<PlayerState>& state, Offset destination);
    std::expected<bool, std::string> MoveAlongPathThenOpenDoor(ThreadSafeProxy<PlayerState>& state, Bot::OpenDoor& door);
    std::expected<bool, std::string> MoveAlongPathThenUse(
      ThreadSafeProxy<PlayerState>& state,
      std::shared_ptr<const Map> map,
      Tile expectedTileAfterUse,
      std::string_view message);

    template <typename Predicate, typename Callable>
      requires std::is_invocable_v<Predicate, Offset> && std::is_invocable_v<Callable, ThreadSafeProxy<PlayerState>&>
    std::expected<bool, std::string>
      ComputePathAndThen(const std::shared_ptr<const Map>& map, Predicate&& predicate, Callable&& callable)
    {
      return ComputePathAndThen(map, std::nullopt, std::forward<Predicate>(predicate), std::forward<Callable>(callable));
    }

    template <typename Predicate, typename Callable>
      requires std::is_invocable_v<Predicate, Offset> && std::is_invocable_v<Callable, ThreadSafeProxy<PlayerState>&>
    std::expected<bool, std::string> ComputePathAndThen(
      const std::shared_ptr<const Map>& map,
      std::optional<Offset> destination,
      Predicate&& predicate,
      Callable&& callable)
    {
      auto state = m_state.Lock();
      auto weights = WeightMap(*map, state->navigationParameters, destination);
      state->reversedPath = ReversedPath(weights, state->position, std::forward<Predicate>(predicate));
      state->pathLength = state->reversedPath.size();

      return std::forward<Callable>(callable)(state);
    }

    template <typename Callable>
      requires std::is_invocable_v<Callable>
    std::expected<bool, std::string>
      MoveAlongPathThenUse(ThreadSafeProxy<PlayerState>& state, Bot::MoveThenUse& moveThenUse, Callable&& onUse)
    {
      if(moveThenUse.done)
        return true;

      if(!state->reversedPath.empty())
      {
        std::expected<bool, std::string> used = StepAlongPathOrUse(state);
        if(!used)
        {
          return std::unexpected(used.error());
        }
        if(*used)
        {
          moveThenUse.done = true;
          std::invoke(onUse);
        }

        return false;
      }

      return std::unexpected("Destination unreachable");
    }

    int m_id;
    GameCallbacks& m_callbacks;
    std::unique_ptr<Swoq::Game> m_game;
    ThreadSafe<DungeonMap::Ptr>& m_dungeonMap;
    ThreadSafe<std::shared_ptr<const Map>>& m_map;
    int m_level = -1;
    ThreadSafe<PlayerState> m_state;
    ThreadSafe<Commands> m_commands;
  };

} // namespace Bot
