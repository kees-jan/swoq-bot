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
    auto   p0state      = m_player.State();
    Offset p0           = p0state.position;
    characterMap[p0]    = 'a';
    for(const auto& step: p0state.reversedPath)
    {
      if(characterMap[step] == '.')
      {
        characterMap[step] = '*';
      }
    }
    Print(characterMap);
    std::println();
  }

  void Game::Finished(int m_id)
  {
    if(m_id == 0)
    {
      std::println("Terminating player 0");
      m_player.SetCommand(Terminate);
    }
  }

} // namespace Bot