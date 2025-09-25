#pragma once

#include <expected>

#include "GameCallbacks.h"
#include "Map.h"
#include "Player.h"
#include "Swoq.hpp"
#include "ThreadSafe.h"

namespace Bot
{
  class Game : private GameCallbacks
  {
  public:
    Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game);

    std::expected<void, std::string> Run();

  private:
    void LevelReached(int reportingPlayer, int level) override;
    void MapUpdated(int id) override;
    void PrintMap() override;
    void Finished(int m_id) override;

  private:
    Swoq::GameConnection                   m_gameConnection;
    int                                    m_seed;
    int                                    m_level;
    ThreadSafe<std::shared_ptr<const Map>> m_map;
    Player                                 m_player;
  };

} // namespace Bot
