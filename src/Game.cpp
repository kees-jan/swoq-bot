#include "Game.h"

#include <print>

#include "LoggingAndDebugging.h"

namespace Bot
{
  Game::Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game, std::optional<int> expectedLevel)
    : m_gameConnection(gameConnection)
    , m_seed(game->seed())
    , m_level(0)
    , m_map(std::make_shared<Map>())
    , m_player(0, *this, std::move(game), m_map)
    , m_expectedLevel(expectedLevel)
  {
  }

  std::expected<void, std::string> Game::Run()
  {
    auto result = m_player.Run();
    if(result && m_expectedLevel && *m_expectedLevel != m_level)
    {
      return std::unexpected(std::format("Expected level {}, but reached {}", *m_expectedLevel, m_level));
    }
    return result;
  }

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

  void Game::MapUpdated(int id)
  {
    auto map = m_map.Get();
    if(map->Exit())
    {
      std::println("Going to the exit");
      m_player.SetCommand(Visit(*map->Exit()));
      m_playerState = PlayerState::MovingToExit;
    }
    else
    {
      std::optional<DoorColor> doorToOpen = DoorToOpen(map, id);
      std::println("Exit not visible yet. Playerstate: {}, door to open: {}", m_playerState, doorToOpen);

      if(m_playerState != PlayerState::OpeningDoor && doorToOpen)
      {
        std::println("Player {}: Planning to open {} door", id, *doorToOpen);
        auto     doorData = map->DoorData().at(*doorToOpen);
        Commands commands;
        commands.emplace(FetchKey(*doorData.keyPosition, *doorToOpen));
        commands.emplace(OpenDoor(*doorData.doorPosition, *doorToOpen));
        m_player.SetCommands(commands);

        m_playerState = PlayerState::OpeningDoor;
      }
      else if(m_playerState == PlayerState::Idle || m_playerState == PlayerState::Exploring)
      {
        m_player.SetCommand(Explore);
        m_playerState = PlayerState::Exploring;
      }
    }
  }

  void Game::PrintMap()
  {
    if constexpr(Debugging::PrintMaps)
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
  }

  void Game::Finished(int m_id)
  {
    if(m_id == 0)
    {
      if(m_playerState == PlayerState::Exploring)
      {
        std::println("Terminating player 0");
        m_player.SetCommand(Terminate);
      }
      else
      {
        m_playerState = PlayerState::Idle;
        MapUpdated(m_id);
      }
    }
  }

  std::optional<DoorColor> Game::DoorToOpen(const std::shared_ptr<const Map>& map, int)
  {
    auto playerState = m_player.State();
    for(auto color: DoorColors)
    {
      auto doorData = map->DoorData().at(color);
      std::println("DoorToOpen: Considering {}: door at {}, key at {}, avoidDoor: {}, avoidKey: {}",
                   color,
                   doorData.doorPosition,
                   doorData.keyPosition,
                   playerState.navigationParameters.doorParameters.at(color).avoidDoor,
                   playerState.navigationParameters.doorParameters.at(color).avoidKey);

      if(doorData.doorPosition && doorData.keyPosition && playerState.navigationParameters.doorParameters.at(color).avoidDoor)
      {
        return color;
      }
    }

    return std::nullopt;
  }

} // namespace Bot