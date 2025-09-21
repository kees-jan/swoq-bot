#pragma once

#include <expected>

#include "GameCallbacks.h"
#include "Player.h"
#include "Swoq.hpp"

namespace Bot
{

  class Game : private GameCallbacks
  {
  public:
    Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game);

    std::expected<void, std::string> Run();

  private:
    void LevelReached(int reportingPlayer, int level) override;

  private:
    Swoq::GameConnection m_gameConnection;
    int                  m_seed;
    int                  m_level;
    Player               m_player;
  };

} // namespace Bot
