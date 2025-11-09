#pragma once

#include <expected>

#include "DungeonMap.h"
#include "GameCallbacks.h"
#include "Player.h"
#include "PlayerMap.h"
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
      ReconsideringUncheckedBoulders,
      MovingBoulder,
      MovingToExit,
      Terminating,
      PeekingBelowEnemy,
      AttackingEnemy,
      Inactive,
    };

    Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game, std::optional<int> expectedLevel);

    std::expected<void, std::string> Run();

  private:
    void LevelReached(int level) override;
    void MapUpdated() override;
    void PrintDungeonMap() override;
    void Finished(size_t playerId) override;

    size_t LeadPlayer() const;
    size_t OtherPlayer() const;
    PlayerState& GetPlayerState(size_t id);
    bool IsAvailable(size_t playerId);
    void MapUpdated(size_t playerId);
    void SwapPlayers();
    void CheckPlayerPresence();

    std::optional<DoorColor> DoorToOpen(const std::shared_ptr<const PlayerMap>& map, int id);
    std::optional<DoorColor> PressurePlateToActivate(const std::shared_ptr<const PlayerMap>& map, int id);
    OffsetSet BouldersToMove(const std::shared_ptr<const PlayerMap>& map, int id);
    Offset ClosestUncheckedBoulder(const PlayerMap& map, size_t id);
    std::optional<Offset> ClosestUnusedBoulder(const PlayerMap& map, Offset currentLocation, size_t id);
    bool ExitIsReachable(const PlayerMap& map);

    Swoq::GameConnection m_gameConnection;
    int m_seed;
    int m_level;
    Offset m_mapSize;
    ThreadSafe<DungeonMap::Ptr> m_dungeonMap;
    ThreadSafe<PlayerMap::Ptr> m_playerMap;
    Player m_player;
    size_t m_leadPlayerId = 0;
    PlayerState m_leadPlayerState = PlayerState::Idle;
    PlayerState m_otherPlayerState = PlayerState::Idle;
    std::optional<int> m_expectedLevel;
  };

} // namespace Bot

template <>
struct std::formatter<Bot::Game::PlayerState>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const Bot::Game::PlayerState& state, std::format_context& ctx) const
  {
    using enum Bot::Game::PlayerState;
    std::string s = "<UNKNOWN>";
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
    case ReconsideringUncheckedBoulders:
      s = "ReconsiderUncheckedBoulders";
      break;
    case MovingBoulder:
      s = "MovingBoulder";
      break;
    case MovingToExit:
      s = "MovingToExit";
      break;
    case Terminating:
      s = "Terminating";
      break;
    case PeekingBelowEnemy:
      s = "PeekingBelowEnemy";
      break;
    case AttackingEnemy:
      s = "AttackingEnemy";
      break;
    case Inactive:
      s = "Inactive";
      break;
    }

    return std::format_to(ctx.out(), "{}", s);
  }
};
