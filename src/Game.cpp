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
    , m_player(*this, std::move(game), m_dungeonMap, m_playerMap)
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

  void Game::LevelReached(int level)
  {
    PrintDungeonMap();

    std::print("Reached level {}!\n", level);
    m_level = level;
    m_playerMap.Lock() = std::make_shared<PlayerMap>(m_mapSize);
    m_dungeonMap.Lock() = DungeonMap::Create(m_mapSize);
  }

  void Game::MapUpdated(size_t playerId)
  {
    if(!IsAvailable(playerId))
      return;

    auto stateArray = m_player.State();
    auto& state = stateArray[playerId];
    auto& playerState = GetPlayerState(playerId);

    if(!IsEngagingEnemy(playerState))
    {
      auto map = m_playerMap.Get();
      auto& enemiesInSight = map->enemies.inSight[playerId];

      if(state.hasSword && state.health >= 6 && !enemiesInSight.empty())
      {
        std::println("Player {}: Enemies in sight at {}. Attacking", playerId, enemiesInSight);
        m_player.SetCommand(playerId, Attack);

        playerState = PlayerState::AttackingEnemy;
      }
      else if(auto unknownSquares = enemiesInSight
                                  | std::views::filter([&, map = *map](Offset pos) { return map[pos] == Tile::TILE_UNKNOWN; })
                                  | std::ranges::to<OffsetSet>();
              !unknownSquares.empty())
      {
        std::println("Player {}: Enemies are obscuring {}. Luring them away", playerId, unknownSquares);
        m_player.SetCommand(playerId, PeekUnderEnemies(unknownSquares));

        playerState = PlayerState::PeekingBelowEnemy;
      }
    }
  }

  void Game::SwapPlayers()
  {
    m_leadPlayerId = OtherPlayer();
    std::swap(m_leadPlayerState, m_otherPlayerState);
    std::println("Game: Swapped players. New lead player is {}", m_leadPlayerId);
  }

  void Game::CheckPlayerPresence()
  {
    auto state = m_player.State();
    const auto leadPlayer = LeadPlayer();
    const auto otherPlayer = OtherPlayer();

    if(state[leadPlayer].active != (m_leadPlayerState != PlayerState::Inactive))
    {
      std::println(
        "Game: Lead player {} changed active state from {} to {}", leadPlayer, m_leadPlayerState, state[leadPlayer].active);
      m_leadPlayerState = state[leadPlayer].active ? PlayerState::Idle : PlayerState::Inactive;
    }
    if(state[otherPlayer].active != (m_otherPlayerState != PlayerState::Inactive))
    {
      std::println(
        "Game: Other player {} changed active state from {} to {}", otherPlayer, m_otherPlayerState, state[otherPlayer].active);
      m_otherPlayerState = state[otherPlayer].active ? PlayerState::Idle : PlayerState::Inactive;
    }

    if(!state[LeadPlayer()].active && state[OtherPlayer()].active)
    {
      SwapPlayers();
    }
  }

  void Game::MapUpdated()
  {
    CheckPlayerPresence();
    std::println("Game: Player updated the map while doing  {}, {}", m_leadPlayerState, m_otherPlayerState);
    MapUpdated(LeadPlayer());
    MapUpdated(OtherPlayer());
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

  void Game::Finished(size_t playerId)
  {
    CheckPlayerPresence();
    if(!IsAvailable(playerId))
    {
      std::println("Game: Player {} is inactive, ignoring finished callback", playerId);
      return;
    }

    PlayerState& playerState = GetPlayerState(playerId);
    std::println("Game: Player {} finished task {}", playerId, playerState);

    if(playerId == LeadPlayer() && playerState != PlayerState::Exploring && m_otherPlayerState == PlayerState::Idle)
    {
      std::println("Game: Player {} resumes exploring", OtherPlayer());
      m_player.SetCommand(OtherPlayer(), Explore);
      m_otherPlayerState = PlayerState::Exploring;
    }

    if(playerId == LeadPlayer())
    {
      auto map = m_playerMap.Get();
      std::optional<DoorColor> doorToOpen = DoorToOpen(map, playerId);
      std::optional<DoorColor> pressurePlateToActivate = PressurePlateToActivate(map, playerId);
      OffsetSet bouldersToMove = BouldersToMove(map, playerId);
      std::println(
        "Player {}: Playerstate: {}, exit: {}, door to open: {}, pressureplate to activate: {}, boulders to check: {}",
        playerId,
        playerState,
        map->Exit(),
        doorToOpen,
        pressurePlateToActivate,
        bouldersToMove);

      if(playerState == PlayerState::MovingBoulder)
      {
        playerState = PlayerState::Idle;
      }
      else if(playerState == PlayerState::ReconsideringUncheckedBoulders)
      {
        if(!bouldersToMove.empty())
        {
          auto destination = ClosestUncheckedBoulder(*map, playerId);
          std::println("Player {}: Planning move boulder at {}", playerId, destination);
          Commands commands;
          commands.emplace(FetchBoulder(destination));
          commands.emplace(DropBoulder);
          m_player.SetCommands(playerId, commands);

          playerState = PlayerState::MovingBoulder;
        }
        else
        {
          playerState = PlayerState::Idle;
        }
      }

      if(playerState != PlayerState::MovingBoulder)
      {
        if(playerState != PlayerState::Exploring)
        {
          std::println("Player {}: Resume exploration", playerId);
          m_player.SetCommand(playerId, Explore);
          playerState = PlayerState::Exploring;
        }
        else
        {
          if(!bouldersToMove.empty())
          {
            std::println("Player {}: Reconsidering unchecked boulders", playerId);
            m_player.SetCommand(playerId, ReconsiderUncheckedBoulders);

            playerState = PlayerState::ReconsideringUncheckedBoulders;
          }
          else if(doorToOpen)
          {
            auto doorData = map->DoorData().at(*doorToOpen);
            std::println(
              "Player {}: Planning to open {} door. Key is at {}, door is at {}",
              playerId,
              *doorToOpen,
              doorData.keyPosition,
              doorData.doorPosition);
            Commands commands;
            commands.emplace(FetchKey(*doorData.keyPosition));
            commands.emplace(OpenDoor(*doorData.doorPosition.begin(), *doorToOpen));
            m_player.SetCommands(playerId, commands);

            playerState = PlayerState::OpeningDoor;
          }
          else if(pressurePlateToActivate)
          {
            auto doorData = map->DoorData().at(*pressurePlateToActivate);
            auto pressurePlatePosition = *doorData.pressurePlatePosition;
            auto boulder = ClosestUnusedBoulder(*map, pressurePlatePosition, playerId);
            if(boulder)
            {
              std::println(
                "Player {}: Planning to move boulder at {} to {} pressureplate at {}",
                playerId,
                *boulder,
                *pressurePlateToActivate,
                pressurePlatePosition);
              Commands commands;
              commands.emplace(FetchBoulder(*boulder));
              commands.emplace(PlaceBoulderOnPressurePlate(pressurePlatePosition, *pressurePlateToActivate));
              m_player.SetCommands(playerId, commands);

              playerState = PlayerState::OpeningDoor;
            }
            else
            {
              std::println(
                "Player {}: No boulder found to put on {} pressureplate at {}. Going there myself",
                playerId,
                *pressurePlateToActivate,
                pressurePlatePosition);
              Commands commands;
              commands.emplace(Visit(pressurePlatePosition));
              commands.emplace(DropDoorOnEnemy(doorData.doorPosition));
              m_player.SetCommands(playerId, commands);
            }
          }
          else if(map->Exit())
          {
            std::println("Going to the exit");
            m_player.SetCommand(playerId, Visit(*map->Exit()));
            playerState = PlayerState::MovingToExit;
          }
          else
          {
            std::println("Terminating player {}", playerId);
            m_player.SetCommand(playerId, Terminate);
            playerState = PlayerState::Terminating;
          }
        }
      }
    }
    else
    {
      std::println("Player {}: Waiting for other to make progress", playerId);
      m_player.SetCommand(playerId, Wait);
      playerState = PlayerState::Idle;
    }
  }

  size_t Game::LeadPlayer() const { return m_leadPlayerId; }
  size_t Game::OtherPlayer() const { return 1 - m_leadPlayerId; }
  Game::PlayerState& Game::GetPlayerState(size_t id) { return id == LeadPlayer() ? m_leadPlayerState : m_otherPlayerState; }
  bool Game::IsAvailable(size_t playerId) { return m_player.State()[playerId].active; }

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

  Offset Game::ClosestUncheckedBoulder(const PlayerMap& map, size_t id)
  {
    auto stateArray = m_player.State();
    auto& state = stateArray[id];

    auto destinationPredicate = [&](Offset p) { return map.uncheckedBoulders.contains(p); };
    auto weights = WeightMap(id, map, map.enemies, map.NavigationParameters(), destinationPredicate);
    auto [dist, destination] = DistanceMap(weights, state.position, destinationPredicate);

    assert(destination);

    return *destination;
  }

  std::optional<Offset> Game::ClosestUnusedBoulder(const PlayerMap& map, Offset currentLocation, size_t id)
  {
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

    auto weights = WeightMap(id, map, map.enemies, navigationParameters, destination);
    auto [dist, boulderPosition] = DistanceMap(weights, currentLocation, destination);

    return boulderPosition;
  }

} // namespace Bot