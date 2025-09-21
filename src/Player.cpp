#include "Player.h"

#include <print>

#include "Map.h"

namespace Bot
{
  using Swoq::Interface::DirectedAction;
  using Swoq::Interface::GameStatus;

  Player::Player(int id, GameCallbacks& callbacks, std::unique_ptr<Swoq::Game> game)
    : m_id(id)
    , m_callbacks(callbacks)
    , m_game(std::move(game))
  {
    // Show game stats
    std::println("Game {} started", m_game->game_id());
    std::println("- seed: {}", m_game->seed());
    std::println("- level: {}", m_level);
    std::println("- map size: {}x{}", m_game->map_height(), m_game->map_width());
    std::println("- visibility: {}", m_game->visibility_range());
  }

  std::expected<void, std::string> Player::Run()
  {
    const int visibility = m_game->visibility_range();

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

      auto state = m_game->state().playerstate();
      auto pos   = state.position();
      auto view  = ViewFromState(visibility, state);
      Print(view);
      std::println();

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