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

  std::expected<bool, std::string> Player::UpdatePlan()
  {
    auto commands = m_commands.Lock();
    if(commands->empty())
    {
      return true;
    }
    auto command = commands->front();
    return std::visit(Visitor{[&](Explore_t) { return VisitTiles({Tile::TILE_UNKNOWN}); },
                              [&](const Bot::VisitTiles& visitTiles) { return VisitTiles(visitTiles.tiles); }},
                      command);
  }

  std::expected<bool, std::string> Player::VisitTiles(const std::set<Tile>& tiles)
  {
    auto map     = m_map.Get();
    auto state   = m_state.Lock();
    auto weights = WeightMap(*map);
    state->reversedPath =
      ReversedPath(weights, state->position, [&map = *map, &tiles](Offset p) { return tiles.contains(map[p]); });
    state->pathLength = state->reversedPath.size();

    return state->pathLength == 0;
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
      if(*finished)
      {
        return {};
      }

      m_callbacks.PrintMap();

      auto state = m_state.Get();
      if(!state.reversedPath.empty())
      {
        auto direction = state.reversedPath.back() - state.position;
        auto action    = ActionFromDirection(direction);
        if(!action)
        {
          return std::unexpected(action.error());
        }

        std::println("tick: {}, action: {} because position is {} and next is {}",
                     m_game->state().tick(),
                     *action,
                     state.position,
                     state.reversedPath.back());

        auto result = m_game->act(*action);
        if(!result)
        {
          std::println(std::cerr, "Action failed: {}", result.error());
          return std::unexpected("Action failed");
        }
      }
      else
      {
        return std::unexpected("No path found");
      }
    }

    return {};
  }

} // namespace Bot