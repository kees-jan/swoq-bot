#include "Map.h"

#include <algorithm>
#include <cassert>
#include <print>

#include "LoggingAndDebugging.h"
#include "Swoq.hpp"

namespace Bot
{
  namespace
  {
    std::vector<Tile> NewMapData(const Map& other, Offset newSize)
    {
      assert(newSize.x >= other.Width());
      assert(newSize.y >= other.Height());

      std::vector<Tile> tiles(static_cast<std::size_t>(newSize.x * newSize.y), Tile::TILE_UNKNOWN);
      const auto&       original = other.Data();
      for(int y = 0; y < other.Height(); ++y)
      {
        std::copy_n(original.begin() + y * other.Width(), other.Width(), tiles.begin() + y * newSize.x);
      }
      return tiles;
    }

    bool AreTilesConsistent(Tile viewTile, Tile destinationTile)
    {
      bool const result = viewTile == Tile::TILE_UNKNOWN || destinationTile == Tile::TILE_UNKNOWN
                          || viewTile == Tile::TILE_BOULDER || destinationTile == Tile::TILE_BOULDER
                          || destinationTile == viewTile || IsKey(destinationTile) || IsDoor(destinationTile) || IsKey(viewTile)
                          || viewTile == Tile::TILE_PLAYER || viewTile == Tile::TILE_ENEMY;

      if(!result)
        std::println("Tiles are not consistent: view {}, destination {}", viewTile, destinationTile);

      return result;
    }

    constexpr TileComparisonResult CompareTiles(Tile map, Tile view)
    {
      assert(map != Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_ && map != Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_);
      assert(view != Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_ && view != Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_);

      TileComparisonResult result;

      if(view == Tile::TILE_ENEMY)
      {
        result = TileComparisonResult::IsEnemy();
      }
      else
      {
        switch(map)
        {
        case Tile::TILE_UNKNOWN:
          switch(view)
          {
          case Tile::TILE_UNKNOWN:
            result = TileComparisonResult::NoChange();
            break;
          case Tile::TILE_BOULDER:
            result = TileComparisonResult::NewBoulder();
            break;
          default:
            result = TileComparisonResult::NeedsUpdate();
            break;
          }
          break;
        case Tile::TILE_EMPTY:
          switch(view)
          {
          case Tile::TILE_UNKNOWN:
          case Tile::TILE_EMPTY:
          case Tile::TILE_PLAYER:
          case Tile::TILE_ENEMY:
            result = TileComparisonResult::NoChange();
            break;
          case Tile::TILE_BOULDER:
            result = TileComparisonResult::StuffHasMoved();
            break;
          case Tile::TILE_KEY_BLUE:
          case Tile::TILE_KEY_GREEN:
          case Tile::TILE_KEY_RED:
            result = TileComparisonResult::NeedsUpdate();
            break;
          default:
            assert(false);
          }
          break;
        case Tile::TILE_WALL:
          assert(view == Tile::TILE_WALL || view == Tile::TILE_UNKNOWN);
          result = TileComparisonResult::NoChange();
          break;
        case Tile::TILE_EXIT:
          assert(view == Tile::TILE_EXIT || view == Tile::TILE_UNKNOWN || view == Tile::TILE_PLAYER);
          result = TileComparisonResult::NoChange();
          break;
        case Tile::TILE_ENEMY:
        case Tile::TILE_PLAYER:
          assert(false);

        case Tile::TILE_DOOR_RED:
        case Tile::TILE_DOOR_GREEN:
        case Tile::TILE_DOOR_BLUE:
        case Tile::TILE_KEY_RED:
        case Tile::TILE_KEY_GREEN:
        case Tile::TILE_KEY_BLUE:
        case Tile::TILE_HEALTH:
        case Tile::TILE_SWORD:
          switch(view)
          {
          case Tile::TILE_EMPTY:
          case Tile::TILE_BOULDER:
            result = TileComparisonResult::StuffHasMoved();
            break;
          case Tile::TILE_PLAYER:
          case Tile::TILE_ENEMY:
          case Tile::TILE_UNKNOWN:
            result = TileComparisonResult::NoChange();
            break;
          default:
            assert(view == map);
            result = TileComparisonResult::NoChange();
            break;
          }
          break;
        case Tile::TILE_PRESSURE_PLATE_RED:
        case Tile::TILE_PRESSURE_PLATE_GREEN:
        case Tile::TILE_PRESSURE_PLATE_BLUE:
          switch(view)
          {
          case Tile::TILE_BOULDER:
            result = TileComparisonResult::StuffHasMoved();
            break;
          case Tile::TILE_PLAYER:
          case Tile::TILE_ENEMY:
          case Tile::TILE_UNKNOWN:
            result = TileComparisonResult::NoChange();
            break;
          default:
            assert(view == map);
            result = TileComparisonResult::NoChange();
            break;
          }
          break;
        case Tile::TILE_BOULDER:
          switch(view)
          {
          case Tile::TILE_UNKNOWN:
          case Tile::TILE_PLAYER:
          case Tile::TILE_ENEMY:
          case Tile::TILE_BOULDER:
            result = TileComparisonResult::NoChange();
            break;
          case Tile::TILE_EMPTY:
            result = TileComparisonResult::StuffHasMoved();
            break;
          default:
            assert(false);
          }
          break;
        case Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_:
        case Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_:
          assert(false);
        }
      }

      if constexpr(Debugging::PrintIncorporatingMovedStuff)
      {
        if(result.stuffHasMoved)
        {
          std::println("CompareTiles: Map has {}, but view has {}", map, view);
        }
      }

      return result;
    }
  } // namespace

  Vector2d<int> WeightMap(const Vector2d<Tile>&        map,
                          const NavigationParameters&  navigationParameters,
                          const std::optional<Offset>& destination)
  {
    if(destination)
      return WeightMap(map, navigationParameters, *destination);

    return WeightMap(map, navigationParameters);
  }

  Vector2d<int> WeightMap(const Vector2d<Tile>& map, const NavigationParameters& navigationParameters)
  {
    return WeightMap(map, navigationParameters, [](Offset) { return false; });
  }

  Vector2d<int> WeightMap(const Vector2d<Tile>& map, const NavigationParameters& navigationParameters, Offset destination)
  {
    return WeightMap(map, navigationParameters, [destination](Offset p) { return p == destination; });
  }

  Vector2d<Swoq::Interface::Tile> ViewFromState(int visibility, const Swoq::Interface::PlayerState& state)
  {
    int visibility_dimension = 2 * visibility + 1;
    assert(state.surroundings_size() == visibility_dimension * visibility_dimension);
    return Vector2d(visibility_dimension,
                    visibility_dimension,
                    state.surroundings() | std::views::transform([](int t) { return static_cast<Tile>(t); })
                      | std::ranges::to<std::vector<Tile>>());
  }


  void Print(const Vector2d<Tile>& tiles)
  {
    Print(tiles.Map([](Tile t) { return CharFromTile(t); }));
  }

  Map::Map(Offset size)
    : Vector2d(size.x, size.y)
  {
  }

  Map::Map(const Map& other, Offset newSize)
    : Vector2d(newSize.x, newSize.y, NewMapData(other, newSize))
    , m_exit(other.m_exit)
    , m_doorData(other.m_doorData)
  {
  }

  MapUpdateResult Map::Update(Offset pos, int visibility, const Vector2d<Tile>& view) const
  {
    const Offset offset(visibility, visibility);
    assert(view.Size() == 2 * offset + One);

    auto compareResult = Compare(pos, view, offset);

    if(compareResult.needsUpdate)
    {
      const auto result = std::make_shared<Map>(*this, compareResult.newMapSize);
      result->Update(pos, view, offset);
      return {result, std::move(compareResult)};
    }

    return {nullptr, std::move(compareResult)};
  }

  std::shared_ptr<Map> Map::IncludeLocalView(Offset pos, int visibility, const Vector2d<Tile>& view, bool silently) const
  {
    const Offset offset(visibility, visibility);
    assert(view.Size() == 2 * offset + One);

    const auto result = std::make_shared<Map>(*this);
    result->IncludeLocalView(pos, view, offset, silently);
    return result;
  }

  const DoorMap& Map::DoorData() const { return m_doorData; }

  MapComparisonResult Map::Compare(Offset pos, const Vector2d<Tile>& view, Offset offset) const
  {
    const auto&         me = *this;
    MapComparisonResult result(me);

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = pos + p - offset;

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

    assert(result.needsUpdate || result.newBoulders.empty());
    assert(result.needsUpdate || me.Size() == result.newMapSize);

    return result;
  }

  void Map::Update(Offset pos, const Vector2d<Tile>& view, Offset offset)
  {
    auto& me = *this;

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = pos + p - offset;
      if(IsInRange(destination))
      {
        assert(AreTilesConsistent(view[p], me[destination]));

        if(view[p] == Tile::TILE_UNKNOWN || IsDoor(me[destination]) || IsKey(me[destination])
           || me[destination] == Tile::TILE_BOULDER)
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
        if(view[p] != Tile::TILE_PLAYER && view[p] != Tile::TILE_ENEMY
           && (me[destination] == Tile::TILE_UNKNOWN || (view[p] != Tile::TILE_BOULDER && !IsKey(view[p]))))
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

  void Map::IncludeLocalView(Offset pos, const Vector2d<Tile>& view, Offset offset, bool silently)
  {
    auto& me         = *this;
    bool  foundDelta = false;

    if constexpr(Debugging::PrintIncorporatingMovedStuff)
    {
      if(!silently)
      {
        std::println("Incorporating moved stuff at position {}:", pos);
        std::println("View:");
        Print(view);
        std::println("Map:");
        Print(me);
      }
    }

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = pos + p - offset;
      if(IsInRange(destination))
      {
        assert(AreTilesConsistent(view[p], me[destination]));

        if(view[p] == Tile::TILE_UNKNOWN)
        {
          continue;
        }
        if(view[p] != Tile::TILE_PLAYER && view[p] != Tile::TILE_ENEMY)
        {
          foundDelta |= (me[destination] != view[p]);
          me[destination] = view[p];
        }
      }
      else
      {
        assert(view[p] == Tile::TILE_UNKNOWN);
      }
    }

    assert(foundDelta);
  }

  bool Map::IsGoodBoulder(Offset position) const
  {
    const auto IsEmpty = [&me = *this](Offset p) { return me.IsInRange(p) && IsPotentiallyWalkable(me[p]); };

    bool previousEmpty     = IsEmpty(position + NorthWest);
    bool currentEmpty      = IsEmpty(position + North);
    int  partiallyIsolated = 0;
    int  doublyIsolated    = 0;

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
      currentEmpty  = nextEmpty;
    }

    bool result = (doublyIsolated == 0 && partiallyIsolated <= 2) || (doublyIsolated == 1 && partiallyIsolated == 0);

    if constexpr(Debugging::PrintFindingBoulderLocation)
    {
      const auto MyCharFromTile = [&me = *this](Offset p) { return me.IsInRange(p) ? CharFromTile(me[p]) : '@'; };

      std::println("IsGoodBoulder at position {}: doublyIsolated: {}, partiallyIsolated: {}, result: {}",
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

  bool Map::IsBadBoulder(Offset position) const
  {
    const auto& me = *this;
    return std::ranges::any_of(AllDirections, [&](Offset direction) { return me[position + direction] == Tile::TILE_UNKNOWN; });
  }

  std::optional<Offset>
    ReachablePositionNextTo(const Vector2d<Tile>& map, Offset from, Offset to, const NavigationParameters& navigationParameters)
  {
    const auto weights      = WeightMap(map, navigationParameters, to);
    const auto reversedPath = ReversedPath(weights, from, [&](Offset p) { return p == to; });

    if(reversedPath.empty())
    {
      return std::nullopt;
    }
    if(reversedPath.size() == 1)
    {
      assert(reversedPath[0] == to);
      return from;
    }
    return reversedPath[1];
  }

} // namespace Bot
