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
    enum class PlayerState : std::uint8_t
    {
      Idle,
      Exploring,
      OpeningDoor,
      MovingToExit
    };

    Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game, std::optional<int> expectedLevel);

    std::expected<void, std::string> Run();

  private:
    void LevelReached(int reportingPlayer, int level) override;
    void MapUpdated(int id) override;
    void PrintMap() override;
    void Finished(int m_id) override;

    std::optional<DoorColor> DoorToOpen(const std::shared_ptr<const Map>& map, int id);

    Swoq::GameConnection                   m_gameConnection;
    int                                    m_seed;
    int                                    m_level;
    ThreadSafe<std::shared_ptr<const Map>> m_map;
    Player                                 m_player;
    PlayerState                            m_playerState = PlayerState::Idle;
    std::optional<int>                     m_expectedLevel;
  };

} // namespace Bot

template <>
struct std::formatter<Bot::Game::PlayerState>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const Bot::Game::PlayerState& state, std::format_context& ctx) const
  {
    using enum Bot::Game::PlayerState;
    std::string s;
    switch(state)
    {
    case Idle:
      s = "Idle";
      break;
    case Exploring:
      s = "Exploring";
      break;
    case OpeningDoor:
      s = "OpeningDoor";
      break;
    case MovingToExit:
      s = "MovingToExit";
      break;
    }

    if(s.empty())
      s = "<UNKNOWN>";

    return std::format_to(ctx.out(), "{}", s);
  }
};
