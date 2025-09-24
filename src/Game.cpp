#include "Game.h"

#include <print>

namespace Bot
{
  Game::Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game)
    : m_gameConnection(gameConnection)
    , m_seed(game->seed())
    , m_level(0)
    , m_map(std::make_shared<Map>())
    , m_player(0, *this, std::move(game), m_map)
  {
  }

  std::expected<void, std::string> Game::Run() { return m_player.Run(); }

  void Game::LevelReached(int reportingPlayer, int level)
  {
    if(reportingPlayer == 0)
    {
      std::print("Reached level {}!\n", level);
      m_level      = level;
      m_map.Lock() = std::make_shared<Map>();
    }
    else
    {
      assert(m_level == level);
    }
  }

  void Game::MapUpdated(int) {}
  void Game::PrintMap()
  {
    auto   characterMap = m_map.Get()->Vector2d::Map([](Tile t) { return CharFromTile(t); });
    Offset p0           = m_player.State().position;
    characterMap[p0]    = 'a';
    Print(characterMap);
    std::println();
  }

} // namespace Bot