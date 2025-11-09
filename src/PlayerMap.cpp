#include "PlayerMap.h"

#include <algorithm>
#include <cassert>
#include <print>

#include "LoggingAndDebugging.h"
#include "Swoq.hpp"

namespace Bot
{
  namespace
  {
    bool AreTilesConsistent(Tile viewTile, Tile destinationTile)
    {
      bool const result = viewTile == Tile::TILE_UNKNOWN || destinationTile == Tile::TILE_UNKNOWN || viewTile == destinationTile
                       || CanBeDropped(viewTile) || CanBePickedUp(destinationTile) || CanMove(viewTile)
                       || CanMove(destinationTile) || IsDoor(destinationTile) || IsDoor(viewTile);

      if(!result)
        std::println("Tiles are not consistent: view {}, destination {}", viewTile, destinationTile);

      return result;
    }

    constexpr TileComparisonResult CompareTiles(Tile map, Tile view)
    {
      assert(map != Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_ && map != Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_);
      assert(view != Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_ && view != Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_);

      TileComparisonResult result;

      if(map == Tile::TILE_WALL)
      {
        assert(view == Tile::TILE_WALL || view == Tile::TILE_UNKNOWN);
        result = TileComparisonResult::NoChange();
      }
      else if(map == Tile::TILE_EXIT)
      {
        assert(view == Tile::TILE_EXIT || view == Tile::TILE_UNKNOWN || view == Tile::TILE_PLAYER);
        result = TileComparisonResult::NoChange();
      }
      else if(view == Tile::TILE_ENEMY)
      {
        result = TileComparisonResult::Enemy();
      }
      else if(view == Tile::TILE_PLAYER)
      {
        if(CanBePickedUp(map))
          result = TileComparisonResult::NeedsUpdate();
        else
          result = TileComparisonResult::NoChange();
      }
      else if(view == Tile::TILE_BOULDER && map == Tile::TILE_UNKNOWN)
      {
        result = TileComparisonResult::NewBoulder();
      }
      else if(view != Tile::TILE_UNKNOWN && map != view)
      {
        result = TileComparisonResult::NeedsUpdate();
      }
      else
      {
        result = TileComparisonResult::NoChange();
      }

      return result;
    }
  } // namespace

  Vector2d<int> WeightMap(
    size_t index,
    const Vector2d<Tile>& map,
    const Enemies& enemies,
    const NavigationParameters& navigationParameters,
    const std::optional<Offset>& destination)
  {
    if(destination)
      return WeightMap(index, map, enemies, navigationParameters, *destination);

    return WeightMap(index, map, enemies, navigationParameters);
  }

  Vector2d<int>
    WeightMap(size_t index, const Vector2d<Tile>& map, const Enemies& enemies, const NavigationParameters& navigationParameters)
  {
    return WeightMap(index, map, enemies, navigationParameters, [](Offset) { return false; });
  }

  Vector2d<int> WeightMap(
    size_t playerId,
    const Vector2d<Tile>& map,
    const Enemies& enemies,
    const NavigationParameters& navigationParameters,
    Offset destination)
  {
    return WeightMap(playerId, map, enemies, navigationParameters, [destination](Offset p) { return p == destination; });
  }

  PlayerMap::PlayerMap(Offset size)
    : Vector2d(size.x, size.y)
  {
  }

  PlayerMap::PlayerMap(const PlayerMap& other, Offset newSize)
    : Vector2d(newSize.x, newSize.y, NewMapData(other, newSize))
    , uncheckedBoulders(other.uncheckedBoulders)
    , usedBoulders(other.usedBoulders)
    , enemies(other.enemies)
    , m_navigationParameters(other.m_navigationParameters)
    , m_exit(other.m_exit)
    , m_doorData(other.m_doorData)
  {
  }

  std::shared_ptr<PlayerMap> PlayerMap::Clone() const { return std::make_shared<PlayerMap>(*this); }

  PlayerMap::Ptr PlayerMap::Update(size_t playerId, Offset pos, int visibility, const Vector2d<Tile>& view) const
  {
    MapViewCoordinateConverter const convert(pos, visibility, view);

    auto compareResult = Compare(view, convert);

    if(compareResult.needsUpdate || enemies.inSight[playerId] != compareResult.enemies || !enemies.locations.empty())
    {
      const auto result = std::make_shared<PlayerMap>(*this, compareResult.newMapSize);
      result->Update(view, convert);

      for(auto it = result->enemies.locations.begin(); it != result->enemies.locations.end();)
      {
        if(--it->second <= 0)
        {
          it = result->enemies.locations.erase(it);
        }
        else
        {
          ++it;
        }
      }
      for(Offset missingEnemy: compareResult.disappearedEnemies)
      {
        result->enemies.locations.erase(missingEnemy);
      }
      for(Offset enemy: compareResult.enemies)
      {
        result->enemies.locations[enemy] = EnemyPenalty;
      }
      result->enemies.inSight[playerId] = compareResult.enemies;
      result->uncheckedBoulders.merge(compareResult.newBoulders);

      return result;
    }

    return shared_from_this();
  }

  const DoorMap& PlayerMap::DoorData() const { return m_doorData; }

  MapComparisonResult PlayerMap::Compare(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert) const
  {
    const auto& me = *this;
    MapComparisonResult result(me);

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = convert.ToMap(p);

      if(IsInRange(destination))
      {
        assert(AreTilesConsistent(view[p], me[destination]));
        result.Update(CompareTiles(me[destination], view[p]), destination);
      }
      else if(view[p] != Tile::TILE_UNKNOWN)
      {
        result.newMapSize = max(result.newMapSize, destination + One);
        result.Update(CompareTiles(Tile::TILE_UNKNOWN, view[p]), destination);
      }
    }

    result.disappearedEnemies = enemies.locations | std::views::transform([](auto l) { return l.first; })
                              | std::views::filter(
                                  [&](auto position)
                                  {
                                    auto positionInView = convert.ToView(position);
                                    assert(position == convert.ToMap(positionInView));

                                    return view.IsInRange(positionInView) && view[positionInView] != Tile::TILE_UNKNOWN
                                        && view[positionInView] != Tile::TILE_ENEMY;
                                  })
                              | std::ranges::to<OffsetSet>();

    result.needsUpdate |= !result.disappearedEnemies.empty();

    assert(result.needsUpdate || result.newBoulders.empty());
    assert(result.needsUpdate || me.Size() == result.newMapSize);
    assert(result.needsUpdate || result.disappearedEnemies.empty());

    return result;
  }

  void PlayerMap::Update(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert)
  {
    auto& me = *this;

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = convert.ToMap(p);
      if(IsInRange(destination))
      {
        assert(AreTilesConsistent(view[p], me[destination]));

        if(view[p] == Tile::TILE_UNKNOWN)
        {
          continue;
        }

        if(view[p] == Tile::TILE_EXIT)
        {
          m_exit = destination;
        }

        if(IsDoor(view[p]))
        {
          m_doorData[DoorKeyPlateColor(view[p])].doorPosition.insert(destination);
        }
        if(IsKey(view[p]))
        {
          m_doorData[DoorKeyPlateColor(view[p])].keyPosition = destination;
        }
        if(IsPressurePlate(view[p]))
        {
          m_doorData[DoorKeyPlateColor(view[p])].pressurePlatePosition = destination;
        }
        if(view[p] == Tile::TILE_PLAYER)
        {
          if(CanBePickedUp(me[destination]))
            me[destination] = Tile::TILE_EMPTY;
        }
        else if(view[p] != Tile::TILE_ENEMY)
        {
          me[destination] = view[p];
        }
      }
      else
      {
        assert(view[p] == Tile::TILE_UNKNOWN);
      }
    }
  }

  bool PlayerMap::IsGoodBoulder(Offset position) const
  {
    const auto IsEmpty = [&me = *this](Offset p) { return me.IsInRange(p) && IsPotentiallyWalkable(me[p]); };

    bool previousEmpty = IsEmpty(position + NorthWest);
    bool currentEmpty = IsEmpty(position + North);
    int partiallyIsolated = 0;
    int doublyIsolated = 0;

    for(auto d: {NorthEast, East, SouthEast, South, SouthWest, West, NorthWest, North})
    {
      const bool nextEmpty = IsEmpty(position + d);
      if(currentEmpty && !previousEmpty && !nextEmpty)
      {
        ++doublyIsolated;
      }
      else if(currentEmpty && (!previousEmpty || !nextEmpty))
      {
        ++partiallyIsolated;
      }

      previousEmpty = currentEmpty;
      currentEmpty = nextEmpty;
    }

    bool result = (doublyIsolated == 0 && partiallyIsolated <= 2) || (doublyIsolated == 1 && partiallyIsolated == 0);

    if constexpr(Debugging::PrintFindingBoulderLocation)
    {
      const auto MyCharFromTile = [&me = *this](Offset p) { return me.IsInRange(p) ? CharFromTile(me[p]) : '@'; };

      std::println(
        "IsGoodBoulder at position {}: doublyIsolated: {}, partiallyIsolated: {}, result: {}",
        position,
        doublyIsolated,
        partiallyIsolated,
        result);
      std::println(
        "{}{}{}", MyCharFromTile(position + NorthWest), MyCharFromTile(position + North), MyCharFromTile(position + NorthEast));
      std::println("{}{}{}", MyCharFromTile(position + West), MyCharFromTile(position), MyCharFromTile(position + East));
      std::println(
        "{}{}{}", MyCharFromTile(position + SouthWest), MyCharFromTile(position + South), MyCharFromTile(position + SouthEast));
    }
    return result;
  }

  Bot::NavigationParameters& PlayerMap::NavigationParameters() { return m_navigationParameters; }
  const Bot::NavigationParameters& PlayerMap::NavigationParameters() const { return m_navigationParameters; }

  bool PlayerMap::IsBadBoulder(Offset position) const
  {
    const auto& me = *this;
    return std::ranges::any_of(AllDirections, [&](Offset direction) { return me[position + direction] == Tile::TILE_UNKNOWN; });
  }

  // std::optional<Offset>
  //   ReachablePositionNextTo(const Vector2d<Tile>& map, Offset from, Offset to, const NavigationParameters&
  //   navigationParameters)
  // {
  //   const auto weights = WeightMap(map, TODO, navigationParameters, to);
  //   const auto reversedPath = ReversedPath(weights, from, [&](Offset p) { return p == to; });
  //
  //   if(reversedPath.empty())
  //   {
  //     return std::nullopt;
  //   }
  //   if(reversedPath.size() == 1)
  //   {
  //     assert(reversedPath[0] == to);
  //     return from;
  //   }
  //   return reversedPath[1];
  // }

} // namespace Bot
