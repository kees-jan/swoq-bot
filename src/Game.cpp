#include "Game.h"

#include <print>

#include "Dijkstra.h"
#include "LoggingAndDebugging.h"

namespace Bot
{
  Game::Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game, std::optional<int> expectedLevel)
    : m_gameConnection(gameConnection)
    , m_seed(game->seed())
    , m_level(0)
    , m_mapSize(game->map_width(), game->map_height())
    , m_map(std::make_shared<Map>(m_mapSize))
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
      m_map.Lock() = std::make_shared<Map>(m_mapSize);
    }
    else
    {
      assert(m_level == level);
    }
  }

  void Game::MapUpdated(int) {}

  void Game::PrintMap()
  {
    if constexpr(Debugging::PrintGameMaps)
    {
      auto   characterMap = m_map.Get()->Vector2d::Map([](Tile t) { return CharFromTile(t); });
      auto   p0state      = m_player.State();
      Offset p0           = p0state.position;
      characterMap[p0]    = 'a';
      for(const auto& step: p0state.reversedPath)
      {
        if(characterMap[step] == '.' || characterMap[step] == ' ')
        {
          characterMap[step] = '*';
        }
      }
      std::println("Game map:");
      Print(characterMap);
      std::println();
    }
  }

  void Game::Finished(int id)
  {
    std::println("Game: Player {} finished task {}", id, m_playerState);

    if(id == 0)
    {
      auto                     map            = m_map.Get();
      std::optional<DoorColor> doorToOpen     = DoorToOpen(map, id);
      OffsetSet                bouldersToMove = BouldersToMove(map, id);
      std::println("Player {}: Playerstate: {}, exit: {}, door to open: {}, boulders to check: {}",
                   id,
                   m_playerState,
                   map->Exit(),
                   doorToOpen,
                   bouldersToMove);


      if(m_playerState == PlayerState::MovingBoulder)
      {
        m_playerState = PlayerState::Idle;
      }
      else if(m_playerState == PlayerState::ReconsideringUncheckedBoulders)
      {
        if(!bouldersToMove.empty())
        {
          auto destination = ClosestUncheckedBoulder(*map, id);
          std::println("Player {}: Planning move boulder at {}", id, destination);
          Commands commands;
          commands.emplace(FetchBoulder(destination));
          commands.emplace(DropBoulder);
          m_player.SetCommands(commands);

          m_playerState = PlayerState::MovingBoulder;
        }
        else
        {
          m_playerState = PlayerState::Idle;
        }
      }

      if(m_playerState != PlayerState::MovingBoulder)
      {
        if(m_playerState != PlayerState::Exploring)
        {
          std::println("Player {}: Resume exploration", id);
          m_player.SetCommand(Explore);
          m_playerState = PlayerState::Exploring;
        }
        else
        {
          if(!bouldersToMove.empty())
          {
            std::println("Player {}: Reconsidering unchecked boulders", id);
            Commands commands;
            commands.emplace(ReconsiderUncheckedBoulders);
            m_player.SetCommands(commands);

            m_playerState = PlayerState::ReconsideringUncheckedBoulders;
          }
          else if(doorToOpen)
          {
            std::println("Player {}: Planning to open {} door", id, *doorToOpen);
            auto     doorData = map->DoorData().at(*doorToOpen);
            Commands commands;
            commands.emplace(FetchKey(*doorData.keyPosition));
            commands.emplace(OpenDoor(*doorData.doorPosition, *doorToOpen));
            m_player.SetCommands(commands);

            m_playerState = PlayerState::OpeningDoor;
          }
          else if(map->Exit())
          {
            std::println("Going to the exit");
            m_player.SetCommand(Visit(*map->Exit()));
            m_playerState = PlayerState::MovingToExit;
          }
          else
          {
            std::println("Terminating player {}", id);
            m_player.SetCommand(Terminate);
            m_playerState = PlayerState::Terminating;
          }
        }
      }
    }
  }

  std::optional<DoorColor> Game::DoorToOpen(const std::shared_ptr<const Map>& map, int)
  {
    auto playerState = m_player.State();
    for(auto color: DoorColors)
    {
      auto doorData = map->DoorData().at(color);
      if(doorData.doorPosition && doorData.keyPosition && playerState.navigationParameters.doorParameters.at(color).avoidDoor)
      {
        return color;
      }
    }

    return std::nullopt;
  }

  OffsetSet Game::BouldersToMove(const std::shared_ptr<const Map>&, int)
  {
    return m_player.State().navigationParameters.uncheckedBoulders;
  }

  Offset Game::ClosestUncheckedBoulder(const Map& map, int)
  {
    auto                 state                = m_player.State();
    NavigationParameters navigationParameters = state.navigationParameters;
    navigationParameters.currentBoulders.clear();
    navigationParameters.avoidBoulders = false;

    auto weights = WeightMap(map, navigationParameters);
    auto [dist, destination] =
      DistanceMap(weights, state.position, [&](Offset p) { return state.navigationParameters.uncheckedBoulders.contains(p); });

    assert(destination);

    return *destination;
  }

} // namespace Bot