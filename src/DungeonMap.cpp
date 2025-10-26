#include "DungeonMap.h"

#include <algorithm>
#include <cassert>
#include <print>

#include "LoggingAndDebugging.h"
#include "Swoq.hpp"
#include "TileProperties.h"

namespace Bot
{
  namespace
  {
    bool AreTilesConsistent(Tile viewTile, Tile destinationTile)
    {
      bool const result = viewTile == Tile::TILE_UNKNOWN || destinationTile == Tile::TILE_UNKNOWN || viewTile == destinationTile
                       || CanBeDropped(viewTile) || CanBePickedUp(destinationTile) || CanMove(viewTile)
                       || CanMove(destinationTile) || IsDoor(destinationTile);

      if(!result)
        std::println("DungeonMap: Tiles are not consistent: view {}, destination {}", viewTile, destinationTile);

      return result;
    }

    constexpr bool CompareTiles(Tile map, Tile view)
    {
      assert(map != Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_ && map != Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_);
      assert(view != Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_ && view != Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_);

      if(view == Tile::TILE_PLAYER)
        return false;
      if(map == Tile::TILE_UNKNOWN && view != Tile::TILE_UNKNOWN)
        return true;

      return false;
    }
  } // namespace

  DungeonMap::DungeonMap(Offset size)
    : Vector2d(size.x, size.y)
  {
  }

  DungeonMap::DungeonMap(const DungeonMap& other, Offset newSize)
    : Vector2d(newSize.x, newSize.y, NewMapData(other, newSize))
    , m_version(other.m_version + 1)
  {
  }

  DungeonMap::Ptr DungeonMap::Create(Offset size) { return std::make_shared<DungeonMap>(size); }

  DungeonMap::Ptr DungeonMap::Create(const DungeonMap& other, Offset newSize)
  {
    return std::make_shared<DungeonMap>(other, newSize);
  }

  DungeonMap::Ptr DungeonMap::Update(Offset pos, int visibility, const Vector2d<Tile>& view) const
  {
    MapViewCoordinateConverter const convert(pos, visibility, view);

    auto compareResult = Compare(view, convert);

    if(compareResult.needsUpdate)
    {
      const auto result = std::make_shared<DungeonMap>(*this, compareResult.newMapSize);
      result->Update(view, convert);
      return result;
    }

    return shared_from_this();
  }

  DungeonMap::ComparisonResult DungeonMap::Compare(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert) const
  {
    const auto& me = *this;
    ComparisonResult result(me);

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = convert.ToMap(p);

      if(IsInRange(destination))
      {
        assert(AreTilesConsistent(view[p], me[destination]));
        result.Update(CompareTiles(me[destination], view[p]));
      }
      else if(view[p] != Tile::TILE_UNKNOWN)
      {
        result.newMapSize = max(result.newMapSize, destination + One);
        result.Update(true);
      }
    }

    assert(result.needsUpdate || me.Size() == result.newMapSize);

    return result;
  }

  void DungeonMap::Update(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert)
  {
    auto& me = *this;

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = convert.ToMap(p);
      if(IsInRange(destination))
      {
        assert(AreTilesConsistent(view[p], me[destination]));

        if(view[p] == Tile::TILE_UNKNOWN || view[p] == Tile::TILE_PLAYER || me[destination] != Tile::TILE_UNKNOWN)
        {
          continue;
        }
        me[destination] = view[p];
      }
      else
      {
        assert(view[p] == Tile::TILE_UNKNOWN);
      }
    }
  }

} // namespace Bot
