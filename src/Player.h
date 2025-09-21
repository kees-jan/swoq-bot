#pragma once

#include <expected>

#include "GameCallbacks.h"
#include "Swoq.hpp"

namespace Bot
{

  class Player
  {
  public:
    Player(int id, GameCallbacks& callbacks, std::unique_ptr<Swoq::Game> game);
    std::expected<void, std::string> Run();

  private:
    int                         m_id;
    GameCallbacks&              m_callbacks;
    std::unique_ptr<Swoq::Game> m_game;
    int                         m_level = -1;
  };

} // namespace Bot
