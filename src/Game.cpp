#include "Game.h"

#include <print>

#include "LoggingAndDebugging.h"

namespace Bot
{
  namespace
  {
    bool AvailableForTasks(Game::PlayerState playerState)
    {
      return playerState == Game::PlayerState::Idle || playerState == Game::PlayerState::Exploring;
    }
  } // namespace

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

  void Game::MapUpdated(int id)
  {
    std::println("Game: Map got updated by player {}!", id);

    if(AvailableForTasks(m_playerState))
    {
      auto                     map           = m_map.Get();
      std::optional<DoorColor> doorToOpen    = DoorToOpen(map, id);
      std::optional<Offset>    boulderToMove = BoulderToMove(map, id);
      std::println("Player {}: Playerstate: {}, exit: {}, door to open: {}, boulder to move: {}",
                   id,
                   m_playerState,
                   map->Exit(),
                   doorToOpen,
                   boulderToMove);

      if(map->Exit())
      {
        std::println("Going to the exit");
        m_player.SetCommand(Visit(*map->Exit()));
        m_playerState = PlayerState::MovingToExit;
      }
      else if(boulderToMove)
      {
        std::println("Player {}: Planning move boulder at {}", id, *boulderToMove);
        Commands commands;
        commands.emplace(FetchBoulder(*boulderToMove));
        commands.emplace(DropBoulder);
        m_player.SetCommands(commands);

        m_playerState = PlayerState::MovingBoulder;
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

      if(AvailableForTasks(m_playerState))
      {
        std::println("Player {}: Resume exploration", id);
        m_player.SetCommand(Explore);
        m_playerState = PlayerState::Exploring;
      }
    }
  }

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
        if(characterMap[step] == '.')
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
      if(m_playerState == PlayerState::Exploring)
      {
        std::println("Terminating player 0");
        m_player.SetCommand(Terminate);
      }
      else
      {
        m_playerState = PlayerState::Idle;
        MapUpdated(id);
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

  std::optional<Offset> Game::BoulderToMove(const std::shared_ptr<const Map>&, int)
  {
    // Todo: Maybe start with the closest one, using the map
    auto playerState = m_player.State();
    if(!playerState.navigationParameters.badBoulders.empty())
    {
      return *playerState.navigationParameters.badBoulders.begin();
    }

    return std::nullopt;
  }

} // namespace Bot