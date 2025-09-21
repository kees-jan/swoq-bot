#include "Game.h"

#include <print>

namespace Bot
{
  Game::Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game)
    : m_gameConnection(gameConnection)
    , m_seed(game->seed())
    , m_level(0)
    , m_player(0, *this, std::move(game))
  {
  }

  std::expected<void, std::string> Game::Run() { return m_player.Run(); }

  void Game::LevelReached(int reportingPlayer, int level)
  {
    if(reportingPlayer == 0)
    {
      std::print("Reached level {}!\n", level);
      m_level = level;
    }
    else
    {
      assert(m_level == level);
    }
  }

} // namespace Bot