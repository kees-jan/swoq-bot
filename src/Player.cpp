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


  void PlayerState::Update(
    const std::optional<Swoq::Interface::PlayerState>& state,
    int visibility_,
    std::optional<Vector2d<Swoq::Interface::Tile>>& view_)
  {
    auto newPosition = state.transform([](const auto& s) -> Offset { return s.position(); }).value_or(Offset(-1, -1));

    if(state && newPosition.x >= 0 && newPosition.y >= 0)
    {
      position = newPosition;
      std::println("Player {} at position {}", playerId, position);
      assert(position.x >= 0 && position.y >= 0);
      hasSword = state->has_hassword() && state->hassword();
      if(state->has_health())
        health = state->health();
      visibility = visibility_;
      assert(view_);
      view = *view_;
      active = true;
    }
    else
      active = false;
  }

  std::optional<DirectedAction> PlayerState::GetAction() { return active ? std::make_optional(next) : std::nullopt; }

  Player::Player(
    GameCallbacks& callbacks,
    std::unique_ptr<Swoq::Game> game,
    ThreadSafe<DungeonMap::Ptr>& dungeonMap,
    ThreadSafe<PlayerMap::Ptr>& map)
    : m_callbacks(callbacks)
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

    auto state0 = m_game->state().has_playerstate() ? std::make_optional(m_game->state().playerstate()) : std::nullopt;
    const auto& pos0 = state0.transform([](const auto& state) { return state.position(); });
    auto view0 = state0.transform([&](const auto& state) { return ViewFromState(visibility, state); });

    auto state1 = m_game->state().has_player2state() ? std::make_optional(m_game->state().player2state()) : std::nullopt;
    const auto& pos1 = state1.transform([](const auto& state) { return state.position(); });
    auto view1 = state1.transform([&](const auto& state) { return ViewFromState(visibility, state); });

    {
      auto dungeonMap = m_dungeonMap.Lock();
      if(state0)
      {
        assert(pos0 && view0);
        dungeonMap = dungeonMap->Update(*pos0, visibility, *view0);
      }

      if(state1)
      {
        assert(pos1 && view1);
        dungeonMap = dungeonMap->Update(*pos1, visibility, *view1);
      }
    }

    auto map = m_playerMap.Lock();
    auto newMap = map.Get();
    if(state0)
    {
      newMap = newMap->Update(0, *pos0, visibility, *view0);
    }
    if(state1)
    {
      newMap = newMap->Update(1, *pos1, visibility, *view1);
    }

    auto playerStateArray = m_state.Lock();
    (*playerStateArray)[0].Update(state0, visibility, view0);
    (*playerStateArray)[1].Update(state1, visibility, view1);

    if(newMap != map.Get())
    {
      map = newMap;

      return true;
    }

    return false;
  }

  std::expected<bool, std::string> Player::DoCommandIfAny(size_t playerId)
  {
    if(m_state.Get()[playerId].active)
    {
      auto commandsArray = m_commands.Lock();
      auto& commands = (*commandsArray)[playerId];

      std::expected<bool, std::string> result = true;
      while(result && *result && !commands.empty())
      {
        result = std::visit(
          Visitor{
            [&](Explore_t) { return Explore(playerId); },
            [&](const Bot::VisitTiles& visitTiles) { return VisitTiles(playerId, visitTiles.tiles); },
            [&](Terminate_t) { return TerminateRequested(playerId); },
            [&](const Bot::Visit& visit) { return Visit(playerId, visit.position); },
            [&](const FetchKey& key) { return Visit(playerId, key.position); },
            [&](Bot::OpenDoor& door) { return OpenDoor(playerId, door); },
            [&](Bot::FetchBoulder& boulder) { return FetchBoulder(playerId, boulder); },
            [&](DropBoulder_t& dropBoulder) { return DropBoulder(playerId, dropBoulder); },
            [&](const ReconsiderUncheckedBoulders_t&) { return ReconsiderUncheckedBoulders(); },
            [&](Bot::PlaceBoulderOnPressurePlate& place) { return PlaceBoulderOnPressurePlate(playerId, place); },
            [&](const Wait_t&) { return Wait(playerId); },
            [&](LeaveSquare_t& leaveSquare) { return LeaveSquare(playerId, leaveSquare.originalSquare); },
            [&](DropDoorOnEnemy& dropDoorOnEnemy) { return Execute(playerId, dropDoorOnEnemy); },
            [&](const Bot::PeekUnderEnemies& peekUnderEnemies)
            { return PeekUnderEnemies(playerId, peekUnderEnemies.tileLocations); },
            [&](Attack_t& attack) { return Attack(playerId, attack); },
            [&](Bot::HuntEnemies& huntEnemies) { return HuntEnemies(playerId, huntEnemies); },
          },
          commands.front());

        if(!result)
          return result;
        if(*result)
          commands.pop();
      } // namespace Bot
      assert(result);
      assert(!*result || commands.empty());

      return !commands.empty();
    }

    return true;
  }

  bool Player::WaitForCommands()
  {
    auto commandsArray = m_commands.Lock();
    return commandsArray.WaitUntil(
      m_lastCommandTime + delay,
      [&] { return !std::ranges::all_of(*commandsArray, [](auto& commands) { return commands.empty(); }); });
  }

  void Player::PrintMap()
  {
    if constexpr(Debugging::PrintPlayerMaps)
    {
      auto map = m_playerMap.Get();
      auto characterMap = map->Vector2d::Map([](Tile t) { return CharFromTile(t); });
      for(auto& position: map->enemies.locations)
        characterMap[position] = 'e';
      for(auto position: map->enemies.inSight | std::views::join)
        characterMap[position] = 'E';
      auto p0stateArray = m_state.Get();
      auto& p0state = p0stateArray[0];
      if(p0state.active)
      {
        characterMap[p0state.position] = 'A';
        for(const auto& step: p0state.reversedPath)
        {
          if(characterMap[step] == '.' || characterMap[step] == ' ')
            characterMap[step] = '*';
        }
      }
      auto& p1state = p0stateArray[1];
      if(p1state.active)
      {
        characterMap[p1state.position] = 'a';
        for(const auto& step: p1state.reversedPath)
        {
          if(characterMap[step] == '.' || characterMap[step] == ' ')
            characterMap[step] = '*';
        }
      }
      std::println("Player map:");
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

  std::expected<void, std::string> Player::UpdatePlan(size_t playerId)
  {
    std::expected<bool, std::string> commandDone = false;
    bool commandArrived = true;

    while(commandDone && !*commandDone && commandArrived)
    {
      commandDone = DoCommandIfAny(playerId);
      if(!commandDone)
        return std::unexpected(commandDone.error());

      if(!*commandDone)
      {
        std::println("Player {}: No commands done", playerId);
        m_callbacks.Finished(playerId);
        commandArrived = WaitForCommands();
      }
    }

    assert(*commandDone || !commandArrived);
    if(!*commandDone)
    {
      auto stateArray = m_state.Lock();
      auto& state = (*stateArray)[playerId];
      if(state.active)
      {
        std::println("Player {}: No commands found: {}", playerId, DirectedAction::DIRECTED_ACTION_NONE);
        state.next = DirectedAction::DIRECTED_ACTION_NONE;
        state.reversedPath.clear();
        state.pathLength = 0;
      }
    }

    return {};
  }

  void Player::SetCommands(size_t playerId, Commands commands) { (*m_commands.Lock())[playerId].swap(commands); }

  void Player::SetCommand(size_t playerId, Command command)
  {
    Commands commands({command});
    SetCommands(playerId, commands);
  }

  void Player::FirstDo(size_t playerId, Commands commands)
  {
    auto commandsArray = m_commands.Lock();
    auto& currentCommands = (*commandsArray)[playerId];

    while(!currentCommands.empty())
    {
      commands.push(std::move(currentCommands.front()));
      currentCommands.pop();
    }
    currentCommands.swap(commands);
  }

  void Player::FirstDo(size_t playerId, Command command)
  {
    Commands commands({command});
    FirstDo(playerId, commands);
  }

  void Player::InitializeCommands() { m_commands.Lock() = {}; }

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
    if(m_game->state().has_player2state())
    {
      auto state2 = m_game->state().player2state();
      auto pos2 = state2.position();
      newMap = std::make_shared<PlayerMap>(*newMap, max(pos2 + 2 * One, map->Size()));
      auto& cell2 = (*newMap)[pos2];
      if(cell2 == Tile::TILE_UNKNOWN)
      {
        cell2 = Tile::TILE_EMPTY;
      }
    }

    map = newMap;
  }

  void Player::InitializeState()
  {
    auto stateArray = m_state.Lock();
    stateArray = {};
    auto state = m_game->state();

    if(state.has_playerstate())
    {
      auto& player0 = (*stateArray)[0];
      player0.playerId = 0;
      player0.active = true;
    }

    if(state.has_player2state())
    {
      auto& player1 = (*stateArray)[1];
      player1.playerId = 1;
      player1.active = true;
    }
  }

  void Player::InitializeLevel()
  {
    InitializeMap();
    InitializeCommands();
    InitializeState();
  }

  std::expected<void, std::string> Player::StepAlongPath(PlayerState& state)
  {
    auto direction = state.reversedPath.back() - state.position;
    auto action = ActionFromDirection(direction);
    if(!action)
    {
      return std::unexpected(action.error());
    }

    std::println(
      "Player {}: tick: {}, action: {} because position is {} and next is {}",
      state.playerId,
      m_game->state().tick(),
      *action,
      state.position,
      state.reversedPath.back());

    state.next = *action;
    return {};
  }

  std::expected<bool, std::string> Player::StepAlongPathOrUse(PlayerState& state)
  {
    auto direction = state.reversedPath.back() - state.position;
    bool use = state.pathLength == 1;
    auto action = use ? UseFromDirection(direction) : ActionFromDirection(direction);
    if(!action)
    {
      return std::unexpected(action.error());
    }

    std::println(
      "Player: {}, tick: {}, action: {} because position is {} and next is {}",
      state.playerId,
      m_game->state().tick(),
      *action,
      state.position,
      state.reversedPath.back());

    state.next = *action;
    return use;
  }

  std::expected<bool, std::string> Player::MoveToDestination(PlayerState& state)
  {
    if(!state.reversedPath.empty())
    {
      auto result = StepAlongPath(state);
      if(!result)
      {
        return std::unexpected(result.error());
      }
    }

    return state.pathLength == 0;
  }

  std::expected<bool, std::string> Player::MoveToDestination(PlayerState& state, Offset destination)
  {
    if(!state.reversedPath.empty())
    {
      auto result = StepAlongPath(state);
      if(!result)
      {
        return std::unexpected(result.error());
      }

      return false;
    }
    if(state.position == destination)
      return true;
    return std::unexpected("Destination unreachable");
  }

  std::expected<bool, std::string> Player::VisitTiles(size_t playerId, const std::set<Tile>& tiles)
  {
    auto map = m_playerMap.Get();
    return ComputePathAndThen(
      playerId,
      map,
      [&map = *map, &tiles](Offset p) { return tiles.contains(map[p]); },
      [&](PlayerState& state) { return MoveToDestination(state); });
  }

  std::expected<bool, std::string> Player::Visit(size_t playerId, Offset destination)
  {
    return ComputePathAndThen(
      playerId,
      m_playerMap.Get(),
      destination,
      [destination](Offset p) { return p == destination; },
      [&](PlayerState& state) { return MoveToDestination(state, destination); });
  }

  std::expected<bool, std::string> Player::Visit(size_t playerId, OffsetSet destinations)
  {
    return ComputePathAndThen(
      playerId,
      m_playerMap.Get(),
      [destinations](Offset p) { return destinations.contains(p); },
      [&](PlayerState& state) { return MoveToDestination(state); });
  }

  std::expected<bool, std::string> Player::MoveAlongPathThenOpenDoor(PlayerState& state, Bot::OpenDoor& door)
  {
    return MoveAlongPathThenUse(
      state,
      door,
      [&]()
      {
        std::println("Player {}: Opened door of color {}", state.playerId, door.color);
        UpdateMap([&](auto map) { map->NavigationParameters().doorParameters.at(door.color).avoidDoor = false; });
      });
  }

  std::expected<bool, std::string> Player::MoveAlongPathThenUse(
    PlayerState& state,
    std::shared_ptr<const PlayerMap> map,
    Tile expectedTileAfterUse,
    std::string_view message)
  {
    if(!state.reversedPath.empty())
    {
      auto destination = state.reversedPath.front();
      if((*map)[destination] == expectedTileAfterUse)
      {
        std::println("Player {}: Finished {}", state.playerId, message);
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

  std::expected<bool, std::string> Player::OpenDoor(size_t playerId, Bot::OpenDoor& door)
  {
    return ComputePathAndThen(
      playerId,
      m_playerMap.Get(),
      door.position,
      [&](Offset p) { return p == door.position; },
      [&](PlayerState& state) { return MoveAlongPathThenOpenDoor(state, door); });
  }

  std::expected<bool, std::string> Player::FetchBoulder(size_t playerId, Bot::FetchBoulder& fetchBoulder)
  {
    auto boulderPosition = fetchBoulder.position;
    return ComputePathAndThen(
      playerId,
      m_playerMap.Get(),
      boulderPosition,
      [&](Offset p) { return p == boulderPosition; },
      [&](PlayerState& state)
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

  std::expected<bool, std::string> Player::DropBoulder(size_t playerId, Bot::DropBoulder_t& dropBoulder)
  {
    auto map = m_playerMap.Get();
    auto myLocation = m_state.Get()[playerId].position;
    return ComputePathAndThen(
      playerId,
      map,
      [&](Offset p) { return (*map)[p] == Tile::TILE_EMPTY && map->IsGoodBoulder(p) && p != myLocation; },
      [&](PlayerState& state) -> std::expected<bool, std::string>
      {
        return MoveAlongPathThenUse(
          state,
          dropBoulder,
          [&]()
          {
            auto destination = state.reversedPath.front();
            std::println("DropBoulder: About to drop boulder at {}", destination);
          });
      });
  }

  std::expected<bool, std::string>
    Player::PlaceBoulderOnPressurePlate(size_t playerId, Bot::PlaceBoulderOnPressurePlate& placeBoulder)
  {
    auto pressurePlatePosition = placeBoulder.position;
    return ComputePathAndThen(
      playerId,
      m_playerMap.Get(),
      pressurePlatePosition,
      [&](Offset p) { return p == pressurePlatePosition; },
      [&](PlayerState& state) -> std::expected<bool, std::string>
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

  std::expected<bool, std::string> Player::TerminateRequested(size_t playerId)
  {
    std::println("Player {}: Terminate requested", playerId);
    m_terminateRequested = true;
    return false;
  }

  std::expected<bool, std::string> Player::Wait(size_t playerId)
  {
    auto stateArray = m_state.Lock();
    auto& state = (*stateArray)[playerId];
    state.next = DirectedAction::DIRECTED_ACTION_NONE;
    return false;
  }

  std::expected<bool, std::string> Player::LeaveSquare(size_t playerId)
  {
    std::optional<Offset> dummy;
    auto result = LeaveSquare(playerId, dummy);
    if(result)
    {
      assert(!*result);
    }
    return result;
  }

  std::expected<bool, std::string> Player::LeaveSquare(size_t playerId, std::optional<Offset>& originalSquare)
  {
    auto position = m_state.Get()[playerId].position;
    if(!originalSquare)
    {
      originalSquare = position;
    }
    else if(*originalSquare != position)
    {
      return true;
    }

    return ComputePathAndThen(
      playerId,
      m_playerMap.Get(),
      [position](Offset p) { return p != position; },
      [&](PlayerState& state) { return MoveToDestination(state); });
  }

  std::expected<bool, std::string> Player::Execute(size_t playerId, DropDoorOnEnemy& dropDoorOnEnemy)
  {
    auto map = m_playerMap.Get();
    if(dropDoorOnEnemy.waiting)
    {
      const auto& enemies = map->enemies.inSight[playerId];
      if(std::ranges::find_first_of(enemies, dropDoorOnEnemy.doorLocations) != std::ranges::end(enemies))
      {
        dropDoorOnEnemy.waiting = false;
        return LeaveSquare(playerId);
      }
      return Wait(playerId);
    }

    return true;
  }

  std::expected<bool, std::string> Player::PeekUnderEnemies(size_t playerId, const OffsetSet& tileLocations)
  {
    auto map = m_playerMap.Get();
    auto remaining = tileLocations | std::views::filter([&](Offset location) { return (*map)[location] == Tile::TILE_UNKNOWN; })
                   | std::ranges::to<OffsetSet>();

    auto stateCopy = m_state.Get();
    auto& state = stateCopy[playerId];
    auto destinationPredicate = [&](Offset p) { return remaining.contains(p); };
    auto navigationParameters = map->NavigationParameters();
    navigationParameters.avoidEnemies = false;
    auto weights = WeightMap(playerId, *map, map->enemies, navigationParameters, destinationPredicate);

    auto [dist, destination] = DistanceMap(weights, state.position, destinationPredicate);
    if(!destination)
      return true;

    auto distance = dist[*destination];

    if(map->enemies.locations.contains(*destination))
    {
      if(distance == 1)
        return LeaveSquare(playerId);

      if(distance >= 3)
        return Visit(playerId, *destination);

      return Wait(playerId);
    }

    return Visit(playerId, *destination);
  }

  std::expected<bool, std::string> Player::Attack(size_t playerId, Attack_t&)
  {
    auto map = m_playerMap.Get();
    if(map->enemies.inSight[playerId].empty())
    {
      auto currentMap = m_playerMap.Lock();
      auto newMap = currentMap->Clone();
      newMap->enemies.killed++;
      currentMap = newMap;
      return true;
    }
    auto stateArray = m_state.Lock();
    auto& state = (*stateArray)[playerId];
    if(state.health <= 1)
    {
      std::println("Health low. Giving up");
      return true;
    }

    auto destinationPredicate = [&](Offset p) { return map->enemies.inSight[playerId].contains(p); };
    auto navigationParameters = map->NavigationParameters();
    navigationParameters.avoidEnemies = false;
    auto weights = WeightMap(playerId, *map, map->enemies, navigationParameters, destinationPredicate);

    auto [dist, destination] = DistanceMap(weights, state.position, destinationPredicate);
    if(!destination)
      return std::unexpected("Enemies are unreachable?");

    auto distance = dist[*destination];
    if(distance != 2)
    {
      state.reversedPath = ReversedPath(weights, state.position, destinationPredicate);
      state.pathLength = state.reversedPath.size();
      std::expected<bool, std::string> used = StepAlongPathOrUse(state);
      if(!used)
        return std::unexpected(used.error());
    }
    else
      state.next = DirectedAction::DIRECTED_ACTION_NONE;
    return false;
  }

  std::expected<bool, std::string> Player::HuntEnemies(size_t playerId, Bot::HuntEnemies& huntEnemies)
  {
    auto stateArray = m_state.Get();
    auto map = m_playerMap.Get();

    for(auto& state: stateArray)
    {
      if(state.active)
      {
        auto& view = state.view;
        MapViewCoordinateConverter convert(state.position, state.visibility, state.view);
        for(auto it = huntEnemies.remainingToCheck.begin(); it != huntEnemies.remainingToCheck.end();)
        {
          auto viewPosition = convert.ToView(*it);
          if(view.IsInRange(viewPosition) && view[viewPosition] != Tile::TILE_UNKNOWN && view[viewPosition] != Tile::TILE_ENEMY)
            it = huntEnemies.remainingToCheck.erase(it);
          else
            it++;
        }
      }
    }
    OffsetSet destinations = map->enemies.locations;
    destinations.insert(huntEnemies.remainingToCheck.begin(), huntEnemies.remainingToCheck.end());

    if(destinations.empty())
      return true;

    return Visit(playerId, destinations);
  }

  std::expected<bool, std::string> Player::Explore(size_t playerId)
  {
    std::set tiles{Tile::TILE_UNKNOWN, Tile::TILE_HEALTH};
    auto stateArray = m_state.Get();
    auto& state = stateArray[playerId];
    if(!state.hasSword)
      tiles.insert(Tile::TILE_SWORD);

    return VisitTiles(playerId, tiles);
  }

  std::expected<void, std::string> Player::Run()
  {
    // Game loop
    while(m_game->state().status() == GameStatus::GAME_STATUS_ACTIVE)
    {
      auto level = m_game->state().level();
      if(level != m_level)
      {
        m_callbacks.LevelReached(level);
        m_level = level;
        InitializeLevel();
      }

      if(UpdateMap())
      {
        m_callbacks.MapUpdated();
      }
      auto updateResult = UpdatePlan(0).and_then([&] { return UpdatePlan(1); });
      if(!updateResult)
      {
        return std::unexpected(updateResult.error());
      }

      PrintMap();

      if(m_terminateRequested)
      {
        std::println("Player: Terminating");
        return {};
      }

      auto stateArray = m_state.Lock();
      auto& state0 = (*stateArray)[0];
      auto& state1 = (*stateArray)[1];
      auto result = m_game->act(state0.GetAction(), state1.GetAction());
      m_lastCommandTime = std::chrono::steady_clock::now();
      if(!result)
      {
        std::println("Player: Action failed: {}", result.error());
        return std::unexpected("Action failed");
      }
    }
    return InterpretGameState(m_game->state().status());
  }

} // namespace Bot
