#include "Player.h"

#include <print>

#include <Dijkstra.h>

#include "LoggingAndDebugging.h"
#include "Map.h"

namespace Bot
{
  using Swoq::Interface::DirectedAction;
  using Swoq::Interface::GameStatus;

  namespace
  {
    constexpr std::chrono::seconds delay(8);
    constexpr int                  EnemyPenalty = 5;

    std::expected<DirectedAction, std::string> ActionFromDirection(Offset direction)
    {
      if(direction == East)
      {
        return DirectedAction::DIRECTED_ACTION_MOVE_EAST;
      }
      if(direction == West)
      {
        return DirectedAction::DIRECTED_ACTION_MOVE_WEST;
      }
      if(direction == North)
      {
        return DirectedAction::DIRECTED_ACTION_MOVE_NORTH;
      }
      if(direction == South)
      {
        return DirectedAction::DIRECTED_ACTION_MOVE_SOUTH;
      }
      return std::unexpected("Invalid direction");
    }

    std::expected<DirectedAction, std::string> UseFromDirection(Offset direction)
    {
      if(direction == East)
      {
        return DirectedAction::DIRECTED_ACTION_USE_EAST;
      }
      if(direction == West)
      {
        return DirectedAction::DIRECTED_ACTION_USE_WEST;
      }
      if(direction == North)
      {
        return DirectedAction::DIRECTED_ACTION_USE_NORTH;
      }
      if(direction == South)
      {
        return DirectedAction::DIRECTED_ACTION_USE_SOUTH;
      }
      return std::unexpected("Invalid direction");
    }

    constexpr bool IsUse(Swoq::Interface::DirectedAction action)
    {
      using enum Swoq::Interface::DirectedAction;
      return action == DIRECTED_ACTION_USE_NORTH || action == DIRECTED_ACTION_USE_EAST || action == DIRECTED_ACTION_USE_SOUTH
             || action == DIRECTED_ACTION_USE_WEST;
    }

    std::expected<void, std::string> InterpretGameState(GameStatus status)
    {
      switch(status)
      {
      case GameStatus::GAME_STATUS_FINISHED_SUCCESS:
        return {};
      case GameStatus::GAME_STATUS_ACTIVE:
      case GameStatus::GAME_STATUS_FINISHED_TIMEOUT:
      case GameStatus::GAME_STATUS_FINISHED_NO_PROGRESS:
      case GameStatus::GAME_STATUS_FINISHED_PLAYER_DIED:
      case GameStatus::GAME_STATUS_FINISHED_CANCELED:
        return std::unexpected(std::format("{}", status));

      case GameStatus::GameStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
      case GameStatus::GameStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
        std::terminate();
      }
      assert(false);
    }

    void UpdateEnemyLocations(Offset                pos,
                              int                   visibility,
                              OffsetMap<int>&       enemyLocations,
                              const OffsetSet&      seenEnemies,
                              const Vector2d<Tile>& view)
    {
      const Offset offset(visibility, visibility);
      assert(view.Size() == 2 * offset + One);

      for(auto it = enemyLocations.begin(); it != enemyLocations.end();)
      {
        auto& [location, countDown] = *it;
        const auto viewPosition     = location - pos + offset;

        if(view.IsInRange(viewPosition) && view[viewPosition] != Tile::TILE_UNKNOWN && view[viewPosition] != Tile::TILE_ENEMY)
          it = enemyLocations.erase(it);
        else
        {
          --countDown;
          if(countDown <= 0)
            it = enemyLocations.erase(it);
          else
            ++it;
        }
      }

      for(const auto enemy: seenEnemies)
      {
        enemyLocations[enemy] = EnemyPenalty;
      }
    }

  } // namespace


  Player::Player(int id, GameCallbacks& callbacks, std::unique_ptr<Swoq::Game> game, ThreadSafe<std::shared_ptr<const Map>>& map)
    : m_id(id)
    , m_callbacks(callbacks)
    , m_game(std::move(game))
    , m_map(map)
  {
    // Show game stats
    std::println("Game {} started", m_game->game_id());
    std::println("- seed: {}", m_game->seed());
    std::println("- map size: {}x{}", m_game->map_height(), m_game->map_width());
    std::println("- visibility: {}", m_game->visibility_range());
  }

  bool Player::UpdateMap()
  {
    const int   visibility   = m_game->visibility_range();
    auto        state        = m_game->state().playerstate();
    const auto& pos          = state.position();
    auto        view         = ViewFromState(visibility, state);
    auto        map          = m_map.Lock();
    auto        updateResult = map->Update(pos, visibility, view);

    auto playerState         = m_state.Lock();
    playerState->position    = pos;
    playerState->currentView = updateResult.stuffHasMoved ? std::optional(view) : std::nullopt;
    UpdateEnemyLocations(pos, visibility, playerState->navigationParameters.enemyLocations, updateResult.enemies, view);
    playerState->navigationParameters.enemiesInSight = updateResult.enemies;
    playerState->hasSword                            = state.hassword(); // Not really map related :-(

#ifndef NDEBUG
    bool bad = false;
    for(auto p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = pos + p - visibility * One;
      const bool ok = view[p] != Tile::TILE_BOULDER || playerState->navigationParameters.currentBoulders.contains(destination)
                      || updateResult.newBoulders.contains(destination);

      if(!ok)
      {
        std::println("Boulder at {} (destination {}) not in known boulders", p - visibility * One, destination);
        bad = true;
      }
    }
    if(bad)
    {
      std::println("Player position: {}", static_cast<Offset>(pos));
      Print(view);
      std::println("currentBoulders: {}", playerState->navigationParameters.currentBoulders);
      std::println("newBoulders:     {}", updateResult.newBoulders);
    }
    std::println("Enemy locations: {}", playerState->navigationParameters.enemyLocations);
#endif

    if(const auto newMap = updateResult.map; newMap)
    {
      bool const updated =
        (newMap->Exit() && !map->Exit()) || newMap->DoorData() != map->DoorData() || !updateResult.newBoulders.empty();

      playerState->navigationParameters.uncheckedBoulders.insert(updateResult.newBoulders.begin(),
                                                                 updateResult.newBoulders.end());
      playerState->navigationParameters.currentBoulders.merge(updateResult.newBoulders);

      map = newMap;

      return updated;
    }

    return false;
  }

  std::shared_ptr<const Map> Player::GetMap(bool silently)
  {
    auto state = m_state.Get();
    auto map   = m_map.Get();
    if(state.currentView)
    {
      return map->IncludeLocalView(state.position, m_game->visibility_range(), *state.currentView, silently);
    }
    else
    {
      return map;
    }
  }

  std::expected<bool, std::string> Player::DoCommandIfAny()
  {
    auto commands = m_commands.Lock();

    std::expected<bool, std::string> result = true;
    while(result && *result && !commands->empty())
    {
      result = std::visit(
        Visitor{
          [&](Explore_t) { return Explore(); },
          [&](const Bot::VisitTiles& visitTiles) { return VisitTiles(visitTiles.tiles); },
          [&](Terminate_t) { return TerminateRequested(); },
          [&](const Bot::Visit& visit) { return Visit(visit.position); },
          [&](const FetchKey& key) { return Visit(key.position); },
          [&](Bot::OpenDoor& door) { return OpenDoor(door); },
          [&](Bot::FetchBoulder& boulder) { return FetchBoulder(boulder); },
          [&](DropBoulder_t& dropBoulder) { return DropBoulder(dropBoulder); },
          [&](const ReconsiderUncheckedBoulders_t&) { return ReconsiderUncheckedBoulders(); },
          [&](Bot::PlaceBoulderOnPressurePlate& place) { return PlaceBoulderOnPressurePlate(place); },
          [&](const Wait_t&) { return Wait(); },
          [&](LeaveSquare_t& leaveSquare) { return LeaveSquare(leaveSquare.originalSquare); },
          [&](DropDoorOnEnemy& dropDoorOnEnemy) { return Execute(dropDoorOnEnemy); },
          [&](const Bot::PeekUnderEnemies& peekUnderEnemies) { return PeekUnderEnemies(peekUnderEnemies.tileLocations); },
          [&](const Attack_t&) { return VisitTiles({Tile::TILE_ENEMY}); },
        },
        commands->front());

      if(!result)
        return result;
      if(*result)
        commands->pop();
    } // namespace Bot
    assert(result);
    assert(!*result || commands->empty());

    return !commands->empty();
  }

  bool Player::WaitForCommands()
  {
    auto lastCommandTime = m_state.Get().lastCommandTime;
    auto commands        = m_commands.Lock();
    return commands.WaitUntil(lastCommandTime + delay, [&] { return !commands->empty(); });
  }

  void Player::PrintMap()
  {
    if constexpr(Debugging::PrintPlayerMaps)
    {
      auto   characterMap = GetMap(true)->Vector2d::Map([](Tile t) { return CharFromTile(t); });
      auto   p0state      = m_state.Get();
      Offset p0           = p0state.position;
      characterMap[p0]    = 'a';
      for(auto& [enemy, countdown]: p0state.navigationParameters.enemyLocations)
      {
        characterMap[enemy] = 'E';
      }
      for(const auto& step: p0state.reversedPath)
      {
        if(characterMap[step] == '.' || characterMap[step] == ' ')
        {
          characterMap[step] = '*';
        }
      }
      std::println("Player {} map:", m_id);
      Print(characterMap);
      std::println();
    }
    if constexpr(Debugging::PrintPlayerMapsAsTiles)
    {
      PrintEnum(*GetMap(true));
      std::println();
    }
  }

  std::expected<void, std::string> Player::UpdatePlan()
  {
    std::expected<bool, std::string> commandDone    = false;
    bool                             commandArrived = true;

    while(commandDone && !*commandDone && commandArrived)
    {
      commandDone = DoCommandIfAny();
      if(!commandDone)
        return std::unexpected(commandDone.error());

      if(!*commandDone)
      {
        std::println("Player {}: No commands done", m_id);
        m_callbacks.Finished(m_id);
        commandArrived = WaitForCommands();
      }
    }

    assert(*commandDone || !commandArrived);
    if(!*commandDone)
    {
      std::println("Player {}: No commands found: {}", m_id, DirectedAction::DIRECTED_ACTION_NONE);
      auto state  = m_state.Lock();
      state->next = DirectedAction::DIRECTED_ACTION_NONE;
      state->reversedPath.clear();
      state->pathLength = 0;
    }

    return {};
  }

  void Player::SetCommands(Commands commands) { m_commands.Lock()->swap(commands); }

  void Player::SetCommand(Command command)
  {
    Commands commands({command});
    SetCommands(commands);
  }

  void Player::InitializeCommands() { SetCommands({}); }

  void Player::InitializeMap()
  {
    auto  state  = m_game->state().playerstate();
    auto  pos    = state.position();
    auto  map    = m_map.Lock();
    auto  newMap = std::make_shared<Map>(*map, max(pos + 2 * One, map->Size()));
    auto& cell   = (*newMap)[pos];
    if(cell == Tile::TILE_UNKNOWN)
    {
      cell = Tile::TILE_EMPTY;
    }
    map = newMap;
  }

  void Player::InitializeNavigation()
  {
    auto state                  = m_state.Lock();
    state->navigationParameters = NavigationParameters{};
  }

  void Player::InitializeLevel()
  {
    InitializeMap();
    InitializeNavigation();
    InitializeCommands();
  }

  std::expected<void, std::string> Player::StepAlongPath(ThreadSafeProxy<PlayerState>& state)
  {
    auto direction = state->reversedPath.back() - state->position;
    auto action    = ActionFromDirection(direction);
    if(!action)
    {
      return std::unexpected(action.error());
    }

    std::println("Player: {}, tick: {}, action: {} because position is {} and next is {}",
                 m_id,
                 m_game->state().tick(),
                 *action,
                 state->position,
                 state->reversedPath.back());

    state->next = *action;
    return {};
  }

  std::expected<bool, std::string> Player::StepAlongPathOrUse(ThreadSafeProxy<PlayerState>& state)
  {
    auto direction = state->reversedPath.back() - state->position;
    bool use       = state->pathLength == 1;
    auto action    = use ? UseFromDirection(direction) : ActionFromDirection(direction);
    if(!action)
    {
      return std::unexpected(action.error());
    }

    std::println("Player: {}, tick: {}, action: {} because position is {} and next is {}",
                 m_id,
                 m_game->state().tick(),
                 *action,
                 state->position,
                 state->reversedPath.back());

    state->next = *action;
    return use;
  }

  std::expected<bool, std::string> Player::MoveToDestination(ThreadSafeProxy<PlayerState>& state)
  {
    if(!state->reversedPath.empty())
    {
      auto result = StepAlongPath(state);
      if(!result)
      {
        return std::unexpected(result.error());
      }
    }

    return state->pathLength == 0;
  }

  std::expected<bool, std::string> Player::MoveToDestination(ThreadSafeProxy<PlayerState>& state, Offset destination)
  {
    if(!state->reversedPath.empty())
    {
      auto result = StepAlongPath(state);
      if(!result)
      {
        return std::unexpected(result.error());
      }

      return false;
    }

    if(state->position == destination)
      return true;

    return std::unexpected("Destination unreachable");
  }

  std::expected<bool, std::string> Player::VisitTiles(const std::set<Tile>& tiles)
  {
    auto map = GetMap();
    return ComputePathAndThen(
      map,
      [&map = *map, &tiles](Offset p) { return tiles.contains(map[p]); },
      [&](auto& state) { return MoveToDestination(state); });
  }

  std::expected<bool, std::string> Player::Visit(Offset destination)
  {
    auto map = GetMap();
    return ComputePathAndThen(
      map,
      destination,
      [destination](Offset p) { return p == destination; },
      [&](auto& state) { return MoveToDestination(state, destination); });
  }


  std::expected<bool, std::string> Player::MoveAlongPathThenOpenDoor(ThreadSafeProxy<PlayerState>& state, Bot::OpenDoor& door)
  {
    return MoveAlongPathThenUse(state,
                                door,
                                [&]()
                                {
                                  std::println("Player {}: Opened door of color {}", m_id, door.color);
                                  state->navigationParameters.doorParameters.at(door.color).avoidDoor = false;
                                });
  }

  std::expected<bool, std::string> Player::MoveAlongPathThenUse(ThreadSafeProxy<PlayerState>& state,
                                                                std::shared_ptr<const Map>    map,
                                                                Tile                          expectedTileAfterUse,
                                                                std::string_view              message)
  {
    if(!state->reversedPath.empty())
    {
      auto destination = state->reversedPath.front();
      if((*map)[destination] == expectedTileAfterUse)
      {
        std::println("Player {}: Finished {}", m_id, message);
        return true;
      }

      std::expected<bool, std::string> used = StepAlongPathOrUse(state);
      if(!used)
      {
        return std::unexpected(used.error());
      }
    }
    else
    {
      return std::unexpected("Destination unreachable");
    }

    return false;
  }

  std::expected<bool, std::string> Player::OpenDoor(Bot::OpenDoor& door)
  {
    auto map = GetMap();
    return ComputePathAndThen(
      map,
      door.position,
      [&](Offset p) { return p == door.position; },
      [&](auto& state) { return MoveAlongPathThenOpenDoor(state, door); });
  }

  std::expected<bool, std::string> Player::FetchBoulder(Bot::FetchBoulder& fetchBoulder)
  {
    auto map             = GetMap();
    auto boulderPosition = fetchBoulder.position;
    return ComputePathAndThen(
      map,
      boulderPosition,
      [&](Offset p) { return p == boulderPosition; },
      [&](auto& state)
      {
        return MoveAlongPathThenUse(state,
                                    fetchBoulder,
                                    [&]()
                                    {
                                      std::println("FetchBoulder: About to pick up boulder at {}", boulderPosition);
                                      state->navigationParameters.uncheckedBoulders.erase(boulderPosition);
                                      state->navigationParameters.usedBoulders.erase(boulderPosition);
                                      auto eraseCount = state->navigationParameters.currentBoulders.erase(boulderPosition);
                                      assert(eraseCount == 1);
                                    });
      });
  }

  std::expected<bool, std::string> Player::DropBoulder(Bot::DropBoulder_t& dropBoulder)
  {
    auto map        = GetMap();
    auto myLocation = m_state.Get().position;
    return ComputePathAndThen(
      map,
      [&](Offset p) { return (*map)[p] == Tile::TILE_EMPTY && map->IsGoodBoulder(p) && p != myLocation; },
      [&](auto& state) -> std::expected<bool, std::string>
      {
        return MoveAlongPathThenUse(state,
                                    dropBoulder,
                                    [&]()
                                    {
                                      auto destination = state->reversedPath.front();
                                      std::println("DropBoulder: About to drop boulder at {}", destination);
                                      state->navigationParameters.currentBoulders.insert(destination);
                                    });
      });
  }

  std::expected<bool, std::string> Player::PlaceBoulderOnPressurePlate(Bot::PlaceBoulderOnPressurePlate& placeBoulder)
  {
    auto map                   = GetMap();
    auto pressurePlatePosition = placeBoulder.position;
    return ComputePathAndThen(
      map,
      pressurePlatePosition,
      [&](Offset p) { return p == pressurePlatePosition; },
      [&](auto& state) -> std::expected<bool, std::string>
      {
        return MoveAlongPathThenUse(state,
                                    placeBoulder,
                                    [&]()
                                    {
                                      std::println("PlaceBoulderOnPressurePlate: About to drop boulder at {}",
                                                   pressurePlatePosition);
                                      state->navigationParameters.currentBoulders.insert(pressurePlatePosition);
                                      state->navigationParameters.usedBoulders.insert(pressurePlatePosition);
                                      state->navigationParameters.doorParameters.at(placeBoulder.color).avoidDoor = false;
                                    });
      });
  }

  std::expected<bool, std::string> Player::ReconsiderUncheckedBoulders()
  {
    auto map                                      = GetMap();
    auto state                                    = m_state.Lock();
    state->navigationParameters.uncheckedBoulders = state->navigationParameters.uncheckedBoulders
                                                    | std::views::filter([&map](Offset p) { return !map->IsGoodBoulder(p); })
                                                    | std::ranges::to<OffsetSet>();

    return true;
  }

  std::expected<bool, std::string> Player::TerminateRequested()
  {
    std::println("Player {}: Terminate requested", m_id);
    m_state.Lock()->terminateRequested = true;
    return false;
  }

  std::expected<bool, std::string> Player::Wait()
  {
    m_state.Lock()->next = DirectedAction::DIRECTED_ACTION_NONE;
    return false;
  }

  std::expected<bool, std::string> Player::LeaveSquare(std::optional<Offset>& originalSquare)
  {
    auto position = m_state.Get().position;
    if(!originalSquare)
    {
      originalSquare = position;
    }
    else if(*originalSquare != position)
    {
      return true;
    }

    return ComputePathAndThen(
      GetMap(), [position](Offset p) { return p != position; }, [&](auto& state) { return MoveToDestination(state); });
  }

  std::expected<bool, std::string> Player::Execute(DropDoorOnEnemy& dropDoorOnEnemy)
  {
    auto state = m_state.Get();
    if(dropDoorOnEnemy.waiting)
    {
      auto enemies = state.navigationParameters.enemyLocations | std::views::transform([](auto& pair) { return pair.first; });
      if(std::ranges::find_first_of(enemies, dropDoorOnEnemy.doorLocations) != std::ranges::end(enemies))
      {
        dropDoorOnEnemy.waiting = false;
        std::optional<Offset> dummy;
        return LeaveSquare(dummy);
      }
      return Wait();
    }

    return true;
  }

  std::expected<bool, std::string> Player::PeekUnderEnemies(const OffsetSet& tileLocations)
  {

    auto map       = GetMap();
    auto remaining = tileLocations | std::views::filter([&](Offset location) { return (*map)[location] == Tile::TILE_UNKNOWN; })
                     | std::ranges::to<OffsetSet>();

    auto state                = m_state.Get();
    auto destinationPredicate = [&](Offset p) { return remaining.contains(p); };
    auto weights              = WeightMap(*map, state.navigationParameters, destinationPredicate);

    auto [dist, destination] = DistanceMap(weights, state.position, destinationPredicate);
    if(!destination)
      return true;

    auto distance = dist[*destination];

    if(state.navigationParameters.enemyLocations.contains(*destination))
    {
      if(distance >= 3)
        return Visit(*destination);

      return Wait();
    }

    return Visit(*destination);
  }

  std::expected<bool, std::string> Player::Explore()
  {
    std::set tiles{Tile::TILE_UNKNOWN, Tile::TILE_HEALTH};
    auto     state = m_state.Get();
    if(!state.hasSword)
      tiles.insert(Tile::TILE_SWORD);

    std::println("Explore: Moving towards {}", tiles);

    return VisitTiles(tiles);
  }

  std::expected<void, std::string> Player::Run()
  {
    // Game loop
    while(m_game->state().status() == GameStatus::GAME_STATUS_ACTIVE)
    {
      auto level = m_game->state().level();
      if(level != m_level)
      {
        m_callbacks.LevelReached(m_id, level);
        m_level = level;
        InitializeLevel();
      }

      if(UpdateMap() || !m_state.Get().navigationParameters.enemiesInSight.empty())
      {
        m_callbacks.MapUpdated(m_id);
      }
      auto finished = UpdatePlan();
      if(!finished)
      {
        return std::unexpected(finished.error());
      }

      m_callbacks.PrintMap();
      PrintMap();

      auto state = m_state.Lock();
      if(state->terminateRequested)
      {
        std::println("Player {}: Terminating", m_id);
        return {};
      }
      auto result            = m_game->act(state->next);
      state->lastCommandTime = std::chrono::steady_clock::now();
      if(!result)
      {
        std::println("Player {}: Action failed: {}", m_id, result.error());
        return std::unexpected("Action failed");
      }
    }
    return InterpretGameState(m_game->state().status());
  }

} // namespace Bot