#include "Map.h"

#include <algorithm>
#include <cassert>
#include <print>

#include "Dijkstra.h"
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
      bool const result = viewTile == Tile::TILE_UNKNOWN || destinationTile == Tile::TILE_UNKNOWN || destinationTile == viewTile
                          || IsKey(destinationTile) || IsDoor(destinationTile) || viewTile == Tile::TILE_PLAYER;

      if(!result)
        std::println("Tiles are not consistent: view {}, destination {}", viewTile, destinationTile);

      return result;
    }
  } // namespace

  Vector2d<Swoq::Interface::Tile> ViewFromState(int visibility, const Swoq::Interface::PlayerState& state)
  {
    int visibility_dimension = 2 * visibility + 1;
    assert(state.surroundings_size() == visibility_dimension * visibility_dimension);
    return Vector2d(visibility_dimension,
                    visibility_dimension,
                    state.surroundings() | std::views::transform([](int t) { return static_cast<Tile>(t); })
                      | std::ranges::to<std::vector<Tile>>());
  }

  void Print(const Vector2d<char>& chars)
  {
    std::print("+");
    for(int x = 0; x < chars.Width(); ++x)
    {
      std::print("-");
    }
    std::print("+\n");

    for(int y = 0; y < chars.Height(); ++y)
    {
      std::print("|");
      for(int x = 0; x < chars.Width(); ++x)
      {
        std::print("{}", chars[Offset(x, y)]);
      }
      std::print("|\n");
    }

    std::print("+");
    for(int x = 0; x < chars.Width(); ++x)
    {
      std::print("-");
    }
    std::print("+\n");
  }

  void Print(const Vector2d<Tile>& tiles)
  {
    Print(tiles.Map([](Tile t) { return CharFromTile(t); }));
  }

  void Print(const Vector2d<int>& ints)
  {
    for(int y = 0; y < ints.Height(); ++y)
    {
      for(int x = 0; x < ints.Width(); ++x)
      {
        std::print("{}, ", ints[Offset(x, y)]);
      }
      std::print("\n");
    }
  }

  Map::Map()
    : Vector2d(0, 0)
  {
  }

  Map::Map(const Map& other, Offset newSize)
    : Vector2d(newSize.x, newSize.y, NewMapData(other, newSize))
    , m_exit(other.m_exit)
    , m_doorData(other.m_doorData)
  {
  }

  std::shared_ptr<Map> Map::Update(Offset pos, int visibility, const Vector2d<Tile>& view) const
  {
    Offset offset(visibility, visibility);
    assert(view.Size() == 2 * offset + One);

    const Offset newSize = max(pos + offset + 2 * One, Size());
    const auto   result  = std::make_shared<Map>(*this, newSize);
    result->Update(pos, view, offset);

    return result;
  }

  const DoorMap& Map::DoorData() const { return m_doorData; }

  void Map::Update(Offset pos, const Vector2d<Tile>& view, Offset offset)
  {
    auto& me = *this;

    for(const auto& p: OffsetsInRectangle(view.Size()))
    {
      const auto destination = pos + p - offset;
      if(IsInRange(destination))
      {
        assert(AreTilesConsistent(view[p], me[destination]));

        if(view[p] == Tile::TILE_UNKNOWN || IsDoor(me[destination]) || IsKey(me[destination]))
        {
          continue;
        }
        if(view[p] == Tile::TILE_EXIT)
        {
          m_exit = destination;
        }
        if(IsDoor(view[p]))
        {
          m_doorData[DoorKeyColor(view[p])].doorPosition = destination;
        }
        if(IsKey(view[p]))
        {
          m_doorData[DoorKeyColor(view[p])].keyPosition = destination;
        }
        if(view[p] != Tile::TILE_PLAYER)
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

} // namespace Bot
