#include "Player.h"

#include <print>

#include <Dijkstra.h>

#include "LoggingAndDebugging.h"

namespace Bot
{
  using Swoq::Interface::DirectedAction;
  using Swoq::Interface::GameStatus;

  namespace
  {
    constexpr std::chrono::seconds delay(8);

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
      case GameStatus::GAME_STATUS_FINISHED_PLAYER2_DIED:
      case GameStatus::GAME_STATUS_FINISHED_CANCELED:
        return std::unexpected(std::format("{}", status));

      case GameStatus::GameStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
      case GameStatus::GameStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
        std::terminate();
      }
      assert(false);
    }

  } // namespace


  Player::Player(
    int id,
    GameCallbacks& callbacks,
    std::unique_ptr<Swoq::Game> game,
    ThreadSafe<DungeonMap::Ptr>& dungeonMap,
    ThreadSafe<PlayerMap::Ptr>& map)
    : m_id(id)
    , m_callbacks(callbacks)
    , m_game(std::move(game))
    , m_dungeonMap(dungeonMap)
    , m_playerMap(map)
  {
    // Show game stats
    std::println("Game {} started", m_game->game_id());
    std::println("- seed: {}", m_game->seed());
    std::println("- map size: {}x{}", m_game->map_height(), m_game->map_width());
    std::println("- visibility: {}", m_game->visibility_range());
  }

  bool Player::UpdateMap()
  {
    const int visibility = m_game->visibility_range();
    auto state = m_game->state().playerstate();
    const auto& pos = state.position();
    auto view = ViewFromState(visibility, state);
    {
      auto dungeonMap = m_dungeonMap.Lock();
      dungeonMap = dungeonMap->Update(pos, visibility, view);
    }

    auto map = m_playerMap.Lock();
    auto newMap = map->Update(pos, visibility, view);

    auto playerState = m_state.Lock();
    playerState->position = pos;

    // Not really map related :-(
    playerState->hasSword = state.has_hassword() && state.hassword();
    if(state.has_health())
      playerState->health = state.health();

    if(newMap != map.Get())
    {
      map = newMap;

      return true;
    }

    return false;
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
          [&](Attack_t& attack) { return Attack(attack); },
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
    auto commands = m_commands.Lock();
    return commands.WaitUntil(lastCommandTime + delay, [&] { return !commands->empty(); });
  }

  void Player::PrintMap()
  {
    if constexpr(Debugging::PrintPlayerMaps)
    {
      auto map = m_playerMap.Get();
      auto characterMap = map->Vector2d::Map([](Tile t) { return CharFromTile(t); });
      for(auto& [position, penalty]: map->enemies.locations)
        characterMap[position] = 'e';
      for(auto position: map->enemies.inSight)
        characterMap[position] = 'E';
      auto p0state = m_state.Get();
      Offset p0 = p0state.position;
      characterMap[p0] = 'a';
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
      std::println("Exit:                 {}", map->Exit());
      std::println("DoorData:             {}", map->DoorData());
      std::println("Unchecked boulders:   {}", map->uncheckedBoulders);
      std::println("Used boulders:        {}", map->usedBoulders);
      std::println("Enemies:              {}", map->enemies);
      std::println("NavigationParameters: {}", map->NavigationParameters());
    }
    if constexpr(Debugging::PrintPlayerMapsAsTiles)
    {
      PrintEnum(*m_playerMap.Get());
      std::println();
    }
  }

  std::expected<void, std::string> Player::UpdatePlan()
  {
    std::expected<bool, std::string> commandDone = false;
    bool commandArrived = true;

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
      auto state = m_state.Lock();
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
    auto state = m_game->state().playerstate();
    auto pos = state.position();
    auto map = m_playerMap.Lock();
    auto newMap = std::make_shared<PlayerMap>(*map, max(pos + 2 * One, map->Size()));
    auto& cell = (*newMap)[pos];
    if(cell == Tile::TILE_UNKNOWN)
    {
      cell = Tile::TILE_EMPTY;
    }
    map = newMap;
  }

  void Player::InitializeLevel()
  {
    InitializeMap();
    InitializeCommands();
  }

  std::expected<void, std::string> Player::StepAlongPath(ThreadSafeProxy<PlayerState>& state)
  {
    auto direction = state->reversedPath.back() - state->position;
    auto action = ActionFromDirection(direction);
    if(!action)
    {
      return std::unexpected(action.error());
    }

    std::println(
      "Player: {}, tick: {}, action: {} because position is {} and next is {}",
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
    bool use = state->pathLength == 1;
    auto action = use ? UseFromDirection(direction) : ActionFromDirection(direction);
    if(!action)
    {
      return std::unexpected(action.error());
    }

    std::println(
      "Player: {}, tick: {}, action: {} because position is {} and next is {}",
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
    auto map = m_playerMap.Get();
    return ComputePathAndThen(
      map,
      [&map = *map, &tiles](Offset p) { return tiles.contains(map[p]); },
      [&](auto& state) { return MoveToDestination(state); });
  }

  std::expected<bool, std::string> Player::Visit(Offset destination)
  {
    return ComputePathAndThen(
      m_playerMap.Get(),
      destination,
      [destination](Offset p) { return p == destination; },
      [&](auto& state) { return MoveToDestination(state, destination); });
  }


  std::expected<bool, std::string> Player::MoveAlongPathThenOpenDoor(ThreadSafeProxy<PlayerState>& state, Bot::OpenDoor& door)
  {
    return MoveAlongPathThenUse(
      state,
      door,
      [&]()
      {
        std::println("Player {}: Opened door of color {}", m_id, door.color);
        UpdateMap([&](auto map) { map->NavigationParameters().doorParameters.at(door.color).avoidDoor = false; });
      });
  }

  std::expected<bool, std::string> Player::MoveAlongPathThenUse(
    ThreadSafeProxy<PlayerState>& state,
    std::shared_ptr<const PlayerMap> map,
    Tile expectedTileAfterUse,
    std::string_view message)
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
    return ComputePathAndThen(
      m_playerMap.Get(),
      door.position,
      [&](Offset p) { return p == door.position; },
      [&](auto& state) { return MoveAlongPathThenOpenDoor(state, door); });
  }

  std::expected<bool, std::string> Player::FetchBoulder(Bot::FetchBoulder& fetchBoulder)
  {
    auto boulderPosition = fetchBoulder.position;
    return ComputePathAndThen(
      m_playerMap.Get(),
      boulderPosition,
      [&](Offset p) { return p == boulderPosition; },
      [&](auto& state)
      {
        return MoveAlongPathThenUse(
          state,
          fetchBoulder,
          [&]()
          {
            std::println("FetchBoulder: About to pick up boulder at {}", boulderPosition);
            UpdateMap(
              [&](auto map)
              {
                map->uncheckedBoulders.erase(boulderPosition);
                map->usedBoulders.erase(boulderPosition);
              });
          });
      });
  }

  std::expected<bool, std::string> Player::DropBoulder(Bot::DropBoulder_t& dropBoulder)
  {
    auto map = m_playerMap.Get();
    auto myLocation = m_state.Get().position;
    return ComputePathAndThen(
      map,
      [&](Offset p) { return (*map)[p] == Tile::TILE_EMPTY && map->IsGoodBoulder(p) && p != myLocation; },
      [&](auto& state) -> std::expected<bool, std::string>
      {
        return MoveAlongPathThenUse(
          state,
          dropBoulder,
          [&]()
          {
            auto destination = state->reversedPath.front();
            std::println("DropBoulder: About to drop boulder at {}", destination);
          });
      });
  }

  std::expected<bool, std::string> Player::PlaceBoulderOnPressurePlate(Bot::PlaceBoulderOnPressurePlate& placeBoulder)
  {
    auto pressurePlatePosition = placeBoulder.position;
    return ComputePathAndThen(
      m_playerMap.Get(),
      pressurePlatePosition,
      [&](Offset p) { return p == pressurePlatePosition; },
      [&](auto& state) -> std::expected<bool, std::string>
      {
        return MoveAlongPathThenUse(
          state,
          placeBoulder,
          [&]()
          {
            std::println("PlaceBoulderOnPressurePlate: About to drop boulder at {}", pressurePlatePosition);
            UpdateMap(
              [&](auto map)
              {
                map->usedBoulders.insert(pressurePlatePosition);
                map->NavigationParameters().doorParameters.at(placeBoulder.color).avoidDoor = false;
              });
          });
      });
  }

  std::expected<bool, std::string> Player::ReconsiderUncheckedBoulders()
  {
    UpdateMap(
      [&](auto map)
      {
        map->uncheckedBoulders = map->uncheckedBoulders | std::views::filter([&map](Offset p) { return !map->IsGoodBoulder(p); })
                               | std::ranges::to<OffsetSet>();
      });


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

  std::expected<bool, std::string> Player::LeaveSquare()
  {
    std::optional<Offset> dummy;
    auto result = LeaveSquare(dummy);
    if(result)
    {
      assert(!*result);
    }
    return result;
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
      m_playerMap.Get(), [position](Offset p) { return p != position; }, [&](auto& state) { return MoveToDestination(state); });
  }

  std::expected<bool, std::string> Player::Execute(DropDoorOnEnemy& dropDoorOnEnemy)
  {
    auto map = m_playerMap.Get();
    if(dropDoorOnEnemy.waiting)
    {
      auto enemies = map->enemies.locations | std::views::transform([](auto& pair) { return pair.first; });
      if(std::ranges::find_first_of(enemies, dropDoorOnEnemy.doorLocations) != std::ranges::end(enemies))
      {
        dropDoorOnEnemy.waiting = false;
        return LeaveSquare();
      }
      return Wait();
    }

    return true;
  }

  std::expected<bool, std::string> Player::PeekUnderEnemies(const OffsetSet& tileLocations)
  {
    auto map = m_playerMap.Get();
    auto remaining = tileLocations | std::views::filter([&](Offset location) { return (*map)[location] == Tile::TILE_UNKNOWN; })
                   | std::ranges::to<OffsetSet>();

    auto state = m_state.Get();
    auto destinationPredicate = [&](Offset p) { return remaining.contains(p); };
    auto navigationParameters = map->NavigationParameters();
    navigationParameters.avoidEnemies = false;
    auto weights = WeightMap(*map, map->enemies, navigationParameters, destinationPredicate);

    auto [dist, destination] = DistanceMap(weights, state.position, destinationPredicate);
    if(!destination)
      return true;

    auto distance = dist[*destination];

    if(map->enemies.locations.contains(*destination))
    {
      if(distance == 1)
        return LeaveSquare();

      if(distance >= 3)
        return Visit(*destination);

      return Wait();
    }

    return Visit(*destination);
  }
  std::expected<bool, std::string> Player::Attack(Attack_t&)
  {
    auto map = m_playerMap.Get();

    if(map->enemies.inSight.empty())
      return true;

    auto state = m_state.Lock();

    if(state->health <= 1)
    {
      std::println("Health low. Giving up");
      return true;
    }

    auto destinationPredicate = [&](Offset p) { return map->enemies.inSight.contains(p); };
    auto navigationParameters = map->NavigationParameters();
    navigationParameters.avoidEnemies = false;
    auto weights = WeightMap(*map, map->enemies, navigationParameters, destinationPredicate);

    auto [dist, destination] = DistanceMap(weights, state->position, destinationPredicate);
    if(!destination)
      return std::unexpected("Enemies are unreachable?");

    auto distance = dist[*destination];
    if(distance != 2)
    {
      state->reversedPath = ReversedPath(weights, state->position, destinationPredicate);
      state->pathLength = state->reversedPath.size();

      std::expected<bool, std::string> used = StepAlongPathOrUse(state);
      if(!used)
        return std::unexpected(used.error());
    }
    else
      state->next = DirectedAction::DIRECTED_ACTION_NONE;

    return false;
  }

  std::expected<bool, std::string> Player::Explore()
  {
    std::set tiles{Tile::TILE_UNKNOWN, Tile::TILE_HEALTH};
    auto state = m_state.Get();
    if(!state.hasSword)
      tiles.insert(Tile::TILE_SWORD);

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

      if(UpdateMap())
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
      auto result = m_game->act(state->next);
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
