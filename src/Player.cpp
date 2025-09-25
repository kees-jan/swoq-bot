#include "Player.h"

#include <print>

#include <Dijkstra.h>

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

    constexpr std::chrono::seconds delay(8);
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

    m_commands.Lock()->push(Bot::VisitTiles({Tile::TILE_EXIT, Tile::TILE_UNKNOWN}));
  }

  bool Player::UpdateMap()
  {
    const int visibility = m_game->visibility_range();
    auto      state      = m_game->state().playerstate();
    auto      pos        = state.position();
    auto      view       = ViewFromState(visibility, state);
    auto      map        = m_map.Lock();
    auto      newMap     = map->Update(pos, visibility, view);

    bool updated = newMap->Exit() && !map->Exit();

    map                      = newMap;
    m_state.Lock()->position = pos;

    return updated;
  }

  std::expected<bool, std::string> Player::DoCommandIfAny()
  {
    auto commands = m_commands.Lock();

    std::expected<bool, std::string> result = true;
    while(result && *result && !commands->empty())
    {
      result = std::visit(Visitor{[&](Explore_t) { return VisitTiles({Tile::TILE_UNKNOWN}); },
                                  [&](const Bot::VisitTiles& visitTiles) { return VisitTiles(visitTiles.tiles); },
                                  [&](Terminate_t) { return TerminateRequested(); }},
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


  std::expected<bool, std::string> Player::VisitTiles(const std::set<Tile>& tiles)
  {
    auto map     = m_map.Get();
    auto state   = m_state.Lock();
    auto weights = WeightMap(*map);
    state->reversedPath =
      ReversedPath(weights, state->position, [&map = *map, &tiles](Offset p) { return tiles.contains(map[p]); });
    state->pathLength = state->reversedPath.size();

    if(!state->reversedPath.empty())
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
    }

    return state->pathLength == 0;
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
        std::println(std::cerr, "Player {}: Action failed: {}", m_id, result.error());
        return std::unexpected("Action failed");
      }
    }

    return {};
  }

} // namespace Bot