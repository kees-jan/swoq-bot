#include "Game.h"

#include <print>
#include <ranges>

#include "Dijkstra.h"
#include "LoggingAndDebugging.h"

namespace Bot
{
  namespace
  {
    constexpr bool IsEngagingEnemy(Game::PlayerState playerState)
    {
      return playerState == Game::PlayerState::PeekingBelowEnemy || playerState == Game::PlayerState::AttackingEnemy;
    }
  } // namespace


  Game::Game(const Swoq::GameConnection& gameConnection, std::unique_ptr<Swoq::Game> game, std::optional<int> expectedLevel)
    : m_gameConnection(gameConnection)
    , m_seed(game->seed())
    , m_level(0)
    , m_mapSize(game->map_width(), game->map_height())
    , m_dungeonMap(DungeonMap::Create(m_mapSize))
    , m_playerMap(std::make_shared<PlayerMap>(m_mapSize))
    , m_player(0, *this, std::move(game), m_dungeonMap, m_playerMap)
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
      PrintDungeonMap();

      std::print("Reached level {}!\n", level);
      m_level = level;
      m_playerMap.Lock() = std::make_shared<PlayerMap>(m_mapSize);
      m_dungeonMap.Lock() = DungeonMap::Create(m_mapSize);
    }
    else
    {
      assert(m_level == level);
    }
  }

  void Game::MapUpdated(int id)
  {
    std::println("Game: Player {} updated the map while doing  {}", id, m_playerState);

    auto p0state = m_player.State();
    auto map = m_playerMap.Get();
    auto& enemiesInSight = map->enemies.inSight;
    auto unknownSquares = enemiesInSight
                        | std::views::filter([&, map = *map](Offset pos) { return map[pos] == Tile::TILE_UNKNOWN; })
                        | std::ranges::to<OffsetSet>();

    if(!IsEngagingEnemy(m_playerState))
    {
      if(p0state.hasSword && !enemiesInSight.empty())
      {
        std::println("Player {}: Enemies in sight at {}. Attacking", id, enemiesInSight);
        Commands commands;
        commands.emplace(Attack);
        m_player.SetCommands(commands);

        m_playerState = PlayerState::AttackingEnemy;
      }
      else if(!unknownSquares.empty())
      {
        std::println("Player {}: Enemies are obscuring {}. Luring them away", id, unknownSquares);
        Commands commands;
        commands.emplace(PeekUnderEnemies(unknownSquares));
        m_player.SetCommands(commands);

        m_playerState = PlayerState::PeekingBelowEnemy;
      }
    }
  }
  void Game::PrintDungeonMap()
  {
    if constexpr(Debugging::PrintDungeonMaps)
    {
      auto characterMap = m_dungeonMap.Get()->Vector2d::Map([](Tile t) { return CharFromTile(t); });
      std::println("Dungeon map:");
      Print(characterMap);
      std::println();
    }
  }

  void Game::PrintMap()
  {
    if constexpr(Debugging::PrintGameMaps)
    {
      auto characterMap = m_playerMap.Get()->Vector2d::Map([](Tile t) { return CharFromTile(t); });
      auto p0state = m_player.State();
      Offset p0 = p0state.position;
      characterMap[p0] = 'a';
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
      auto map = m_playerMap.Get();
      std::optional<DoorColor> doorToOpen = DoorToOpen(map, id);
      std::optional<DoorColor> pressurePlateToActivate = PressurePlateToActivate(map, id);
      OffsetSet bouldersToMove = BouldersToMove(map, id);
      std::println(
        "Player {}: Playerstate: {}, exit: {}, door to open: {}, pressureplate to activate: {}, boulders to check: {}",
        id,
        m_playerState,
        map->Exit(),
        doorToOpen,
        pressurePlateToActivate,
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
            auto doorData = map->DoorData().at(*doorToOpen);
            std::println(
              "Player {}: Planning to open {} door. Key is at {}, door is at {}",
              id,
              *doorToOpen,
              doorData.keyPosition,
              doorData.doorPosition);
            Commands commands;
            commands.emplace(FetchKey(*doorData.keyPosition));
            commands.emplace(OpenDoor(*doorData.doorPosition.begin(), *doorToOpen));
            m_player.SetCommands(commands);

            m_playerState = PlayerState::OpeningDoor;
          }
          else if(pressurePlateToActivate)
          {
            auto doorData = map->DoorData().at(*pressurePlateToActivate);
            auto pressurePlatePosition = *doorData.pressurePlatePosition;
            auto boulder = ClosestUnusedBoulder(*map, pressurePlatePosition, id);
            if(boulder)
            {
              std::println(
                "Player {}: Planning to move boulder at {} to {} pressureplate at {}",
                id,
                *boulder,
                *pressurePlateToActivate,
                pressurePlatePosition);
              Commands commands;
              commands.emplace(FetchBoulder(*boulder));
              commands.emplace(PlaceBoulderOnPressurePlate(pressurePlatePosition, *pressurePlateToActivate));
              m_player.SetCommands(commands);

              m_playerState = PlayerState::OpeningDoor;
            }
            else
            {
              std::println(
                "Player {}: No boulder found to put on {} pressureplate at {}. Going there myself",
                id,
                *pressurePlateToActivate,
                pressurePlatePosition);
              Commands commands;
              commands.emplace(Visit(pressurePlatePosition));
              commands.emplace(DropDoorOnEnemy(doorData.doorPosition));
              m_player.SetCommands(commands);
            }
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

  std::optional<DoorColor> Game::DoorToOpen(const std::shared_ptr<const PlayerMap>& map, int)
  {
    for(auto color: DoorColors)
    {
      auto doorData = map->DoorData().at(color);
      if(!doorData.doorPosition.empty() && doorData.keyPosition && map->NavigationParameters().doorParameters.at(color).avoidDoor)
      {
        return color;
      }
    }

    return std::nullopt;
  }

  std::optional<DoorColor> Game::PressurePlateToActivate(const std::shared_ptr<const PlayerMap>& map, int)
  {
    for(auto color: DoorColors)
    {
      auto doorData = map->DoorData().at(color);
      if(
        !doorData.doorPosition.empty() && doorData.pressurePlatePosition
        && map->NavigationParameters().doorParameters.at(color).avoidDoor)
      {
        return color;
      }
    }

    return std::nullopt;
  }

  OffsetSet Game::BouldersToMove(const std::shared_ptr<const PlayerMap>& map, int) { return map->uncheckedBoulders; }

  Offset Game::ClosestUncheckedBoulder(const PlayerMap& map, int)
  {
    auto state = m_player.State();

    auto destinationPredicate = [&](Offset p) { return map.uncheckedBoulders.contains(p); };
    auto weights = WeightMap(map, map.enemies, map.NavigationParameters(), destinationPredicate);
    auto [dist, destination] = DistanceMap(weights, state.position, destinationPredicate);

    assert(destination);

    return *destination;
  }

  std::optional<Offset> Game::ClosestUnusedBoulder(const PlayerMap& map, Offset currentLocation, int)
  {
    auto state = m_player.State();
    NavigationParameters navigationParameters = map.NavigationParameters();

    OffsetSet unusedBoulders;
    for(auto p: OffsetsInRectangle(map.Size()))
    {
      if(map[p] == Tile::TILE_BOULDER && !map.usedBoulders.contains(p))
      {
        unusedBoulders.insert(p);
      }
    }

    auto destination = [&](Offset p) { return unusedBoulders.contains(p); };

    auto weights = WeightMap(map, map.enemies, navigationParameters, destination);
    auto [dist, boulderPosition] = DistanceMap(weights, currentLocation, destination);

    return boulderPosition;
  }

} // namespace Bot