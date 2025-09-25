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

    constexpr std::chrono::seconds delay(8);

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

    if(const auto newMap = updateResult.map; newMap)
    {
      bool const updated =
        (newMap->Exit() && !map->Exit()) || newMap->DoorData() != map->DoorData() || !updateResult.newBoulders.empty();

      playerState->navigationParameters.badBoulders.merge(updateResult.newBoulders);

      map = newMap;

      return updated;
    }

    return false;
  }
  std::shared_ptr<const Map> Player::GetMap()
  {
    auto state = m_state.Get();
    auto map   = m_map.Get();
    if(state.currentView)
    {
      return map->IncludeLocalView(state.position, m_game->visibility_range(), *state.currentView);
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
          [&](Explore_t) { return VisitTiles({Tile::TILE_UNKNOWN}); },
          [&](const Bot::VisitTiles& visitTiles) { return VisitTiles(visitTiles.tiles); },
          [&](Terminate_t) { return TerminateRequested(); },
          [&](const Bot::Visit& visit) { return Visit(visit.position); },
          [&](const Bot::FetchKey& key) { return Visit(key.position); },
          [&](const Bot::OpenDoor& door) { return OpenDoor(door.position, door.color); },
          [&](const Bot::FetchBoulder& boulder) { return FetchBoulder(boulder.position); },
          [&](const Bot::DropBoulder_t&) { return DropBoulder(); },
        },
        commands->front());

      if(!result)
        return result;
      if(*result)
        commands->pop();
    }
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
      auto   characterMap = m_map.Get()->Vector2d::Map([](Tile t) { return CharFromTile(t); });
      auto   p0state      = m_state.Get();
      Offset p0           = p0state.position;
      characterMap[p0]    = 'a';
      for(const auto& step: p0state.reversedPath)
      {
        if(characterMap[step] == '.')
        {
          characterMap[step] = '*';
        }
      }
      std::println("Player {} map:", m_id);
      Print(characterMap);
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
      [&map = *map, destination](Offset p) { return p == destination; },
      [&](auto& state) { return MoveToDestination(state); });
  }

  std::expected<void, std::string> Player::StepAlongPathOrUse(ThreadSafeProxy<PlayerState>& state)
  {
    auto direction = state->reversedPath.back() - state->position;
    auto action    = state->pathLength == 1 ? UseFromDirection(direction) : ActionFromDirection(direction);
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

  std::expected<bool, std::string> Player::MoveAlongPathThenOpenDoor(ThreadSafeProxy<PlayerState>& state,
                                                                     Offset                        destination,
                                                                     DoorColor                     color,
                                                                     std::shared_ptr<const Map>    map)
  {
    if(!state->reversedPath.empty())
    {
      std::expected<void, std::string> result = StepAlongPathOrUse(state);
      if(!result)
      {
        return std::unexpected(result.error());
      }
    }

    if((*map)[destination] == Tile::TILE_EMPTY)
    {
      std::println("Player {}: Opened door of color {}", m_id, color);
      state->navigationParameters.doorParameters.at(color).avoidDoor = false;
      return true;
    }

    return false;
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

      std::expected<void, std::string> result = StepAlongPathOrUse(state);
      if(!result)
      {
        return std::unexpected(result.error());
      }
    }
    else
    {
      return std::unexpected("Destination unreachable?");
    }

    return false;
  }

  std::expected<bool, std::string> Player::OpenDoor(Offset destination, DoorColor color)
  {
    auto map = GetMap();
    return ComputePathAndThen(
      map,
      destination,
      [&map = *map, destination](Offset p) { return p == destination; },
      [&](auto& state) { return MoveAlongPathThenOpenDoor(state, destination, color, map); });
  }

  std::expected<bool, std::string> Player::FetchBoulder(Offset destination)
  {
    auto map = GetMap();
    return ComputePathAndThen(
      map,
      destination,
      [&map = *map, destination](Offset p) { return p == destination; },
      [&](auto& state)
      {
        auto result = MoveAlongPathThenUse(state, map, Tile::TILE_EMPTY, "Fetch boulder");
        if(result && *result)
        {
          // Boulder was picked up
          state->navigationParameters.badBoulders.erase(destination);
          state->navigationParameters.goodBoulders.insert(destination);
        }
        return result;
      });
  }

  std::expected<bool, std::string> Player::DropBoulder()
  {
    auto map = GetMap();
    return ComputePathAndThen(
      map,
      [&](Offset p) { return (*map)[p] == Tile::TILE_EMPTY && map->IsGoodBoulder(p); },
      [&](auto& state) -> std::expected<bool, std::string>
      {
        if(m_game->state().playerstate().inventory() == Swoq::Interface::INVENTORY_NONE)
        {
          return true;
        }
        return MoveAlongPathThenUse(state, map, Tile::TILE_BOULDER, "Drop boulder");
      });
  }

  std::expected<bool, std::string> Player::TerminateRequested()
  {
    std::println("Player {}: Terminate requested", m_id);
    m_state.Lock()->terminateRequested = true;
    return false;
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