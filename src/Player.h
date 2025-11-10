#pragma once

#include <chrono>
#include <expected>

#include "Commands.h"
#include "DungeonMap.h"
#include "GameCallbacks.h"
#include "PlayerMap.h"
#include "Swoq.hpp"
#include "ThreadSafe.h"

namespace Bot
{
  using Swoq::Interface::DirectedAction;

  struct PlayerState
  {
    bool active = false;
    size_t playerId = std::numeric_limits<size_t>::max();
    Offset position{0, 0};
    DirectedAction next;
    std::vector<Offset> reversedPath;
    std::size_t pathLength = 0;
    bool hasSword = false;
    int health = 5;
    int visibility = 0;
    Vector2d<Swoq::Interface::Tile> view;

    void Update(
      const std::optional<Swoq::Interface::PlayerState>& state,
      int visibility_,
      std::optional<Vector2d<Swoq::Interface::Tile>>& view_);
    std::optional<DirectedAction> GetAction();
  };

  using PlayerStateArray = std::array<PlayerState, 2>;

  class Player
  {
  public:
    Player(
      GameCallbacks& callbacks,
      std::unique_ptr<Swoq::Game> game,
      ThreadSafe<DungeonMap::Ptr>& dungeonMap,
      ThreadSafe<PlayerMap::Ptr>& map);
    std::expected<void, std::string> Run();

    PlayerStateArray State() { return m_state.Get(); }
    void SetCommands(size_t playerId, Commands commands);
    void SetCommand(size_t playerId, Command command);
    void FirstDo(size_t playerId, Commands commands);
    void FirstDo(size_t playerId, Command command);

  private:
    void InitializeCommands();
    void InitializeLevel();
    void InitializeMap();
    void InitializeState();
    bool UpdateMap();
    std::expected<bool, std::string> VisitTiles(size_t playerId, const std::set<Tile>& tiles);
    std::expected<bool, std::string> Visit(size_t playerId, Offset destination);
    std::expected<bool, std::string> Visit(size_t playerId, OffsetSet destinations);
    std::expected<bool, std::string> OpenDoor(size_t playerId, Bot::OpenDoor& door);
    std::expected<bool, std::string> FetchBoulder(size_t playerId, Bot::FetchBoulder& fetchBoulder);
    std::expected<bool, std::string> DropBoulder(size_t playerId, Bot::DropBoulder_t& dropBoulder);
    std::expected<bool, std::string> PlaceBoulderOnPressurePlate(size_t playerId, Bot::PlaceBoulderOnPressurePlate& placeBoulder);
    std::expected<bool, std::string> ReconsiderUncheckedBoulders();
    std::expected<bool, std::string> TerminateRequested(size_t playerId);
    std::expected<bool, std::string> Wait(size_t playerId);
    std::expected<bool, std::string> LeaveSquare(size_t playerId);
    std::expected<bool, std::string> LeaveSquare(size_t playerId, std::optional<Offset>& originalSquare);
    std::expected<bool, std::string> Execute(size_t playerId, DropDoorOnEnemy& dropDoorOnEnemy);
    std::expected<bool, std::string> PeekUnderEnemies(size_t playerId, const OffsetSet& tileLocations);
    std::expected<bool, std::string> Attack(size_t playerId, Attack_t&);
    std::expected<bool, std::string> HuntEnemies(size_t playerId, HuntEnemies& huntEnemies);
    std::expected<bool, std::string> Explore(size_t playerId);
    std::expected<void, std::string> UpdatePlan(size_t playerId);
    std::expected<bool, std::string> DoCommandIfAny(size_t playerId);
    bool WaitForCommands();
    void PrintMap();
    std::expected<void, std::string> StepAlongPath(PlayerState& state);
    std::expected<bool, std::string> StepAlongPathOrUse(PlayerState& state);
    std::expected<bool, std::string> MoveToDestination(PlayerState& state);
    std::expected<bool, std::string> MoveToDestination(PlayerState& state, Offset destination);
    std::expected<bool, std::string> MoveAlongPathThenOpenDoor(PlayerState& state, Bot::OpenDoor& door);
    std::expected<bool, std::string> MoveAlongPathThenUse(
      PlayerState& state,
      std::shared_ptr<const PlayerMap> map,
      Tile expectedTileAfterUse,
      std::string_view message);

    template <typename Predicate, typename Callable>
      requires std::is_invocable_v<Predicate, Offset> && std::is_invocable_v<Callable, PlayerState&>
    std::expected<bool, std::string> ComputePathToDestinationAndThen(
      size_t playerId,
      const std::shared_ptr<const PlayerMap>& map,
      Predicate&& predicate,
      Callable&& callable)
    {
      auto stateArrayProxy = m_state.Lock();
      auto& state = (*stateArrayProxy)[playerId];
      auto weights = WeightMap(playerId, *map, map->enemies, map->NavigationParameters(), predicate);
      state.reversedPath = ReversedPath(weights, state.position, std::forward<Predicate>(predicate));
      state.pathLength = state.reversedPath.size();

      return std::forward<Callable>(callable)(state);
    }

    template <typename Predicate, typename Callable>
      requires std::is_invocable_v<Predicate, Offset> && std::is_invocable_v<Callable, PlayerState&>
    std::expected<bool, std::string>
      ComputePathAndThen(size_t playerId, const std::shared_ptr<const PlayerMap>& map, Predicate&& predicate, Callable&& callable)
    {
      auto stateArrayProxy = m_state.Lock();
      auto& state = (*stateArrayProxy)[playerId];
      auto weights = WeightMap(playerId, *map, map->enemies, map->NavigationParameters());
      state.reversedPath = ReversedPath(weights, state.position, std::forward<Predicate>(predicate));
      state.pathLength = state.reversedPath.size();

      return std::forward<Callable>(callable)(state);
    }
    template <typename Callable>
      requires std::is_invocable_v<Callable>
    std::expected<bool, std::string> MoveAlongPathThenUse(PlayerState& state, Bot::MoveThenUse& moveThenUse, Callable&& onUse)
    {
      if(moveThenUse.done)
        return true;

      if(!state.reversedPath.empty())
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

    template <typename Callable>
      requires std::is_invocable_v<Callable, const std::shared_ptr<PlayerMap>&>
    void UpdateMap(Callable&& callable)
    {
      auto map = m_playerMap.Lock();
      auto newMap = map->Clone();
      std::invoke(std::forward<Callable>(callable), newMap);
      map = newMap;
    }

    GameCallbacks& m_callbacks;
    std::unique_ptr<Swoq::Game> m_game;
    ThreadSafe<DungeonMap::Ptr>& m_dungeonMap;
    ThreadSafe<std::shared_ptr<const PlayerMap>>& m_playerMap;
    int m_level = -1;
    ThreadSafe<PlayerStateArray> m_state;
    ThreadSafe<std::array<Commands, 2>> m_commands;
    std::chrono::steady_clock::time_point m_lastCommandTime = std::chrono::steady_clock::now();
    std::atomic<bool> m_terminateRequested = false;
  };

} // namespace Bot
