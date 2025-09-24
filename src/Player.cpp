#include "Player.h"

#include <print>

#include "Map.h"

namespace Bot
{
  using Swoq::Interface::DirectedAction;
  using Swoq::Interface::GameStatus;

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

  std::expected<void, std::string> Player::Run()
  {
    // Game loop
    auto move_east = true;
    while(m_game->state().status() == GameStatus::GAME_STATUS_ACTIVE)
    {
      auto level = m_game->state().level();
      if(level != m_level)
      {
        m_callbacks.LevelReached(m_id, level);
        m_level = level;
      }

      bool updated = UpdateMap();
      m_callbacks.PrintMap();
      if(updated)
      {
        m_callbacks.MapUpdated(m_id);
      }

      // Determine action
      auto action = move_east ? DirectedAction::DIRECTED_ACTION_MOVE_EAST : DirectedAction::DIRECTED_ACTION_MOVE_SOUTH;
      std::println("tick: {}, action: {}", m_game->state().tick(), action);

      // Act
      auto result = m_game->act(action);
      if(!result)
      {
        std::println(std::cerr, "Action failed: {}", result.error());
        return std::unexpected("Action failed");
      }

      // Prepare for next action
      move_east = !move_east;
    }

    return {};
  }

} // namespace Bot