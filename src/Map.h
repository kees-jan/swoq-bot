#pragma once

#include "Swoq.pb.h"
#include "Vector2d.h"

namespace Bot
{
  using Swoq::Interface::PlayerState;
  using Swoq::Interface::Tile;

  inline Vector2d<Tile> ViewFromState(int visibility, const PlayerState& state)
  {
    int visibility_dimension = 2 * visibility + 1;
    assert(state.surroundings_size() == visibility_dimension * visibility_dimension);
    return Vector2d(visibility_dimension,
                    visibility_dimension,
                    state.surroundings() | std::views::transform([](int t) { return static_cast<Tile>(t); })
                      | std::ranges::to<std::vector<Tile>>());
  }

  inline char CharFromTile(Tile tile)
  {
    switch(tile)
    {
    case Tile::TILE_UNKNOWN:
      return ' ';
    case Tile::TILE_EMPTY:
      return '.';
    case Tile::TILE_WALL:
      return '#';
    case Tile::TILE_EXIT:
      return 'E';
    case Tile::TILE_PLAYER:
      return 'O';
    case Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_:
    case Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_:
      break;
    }
    std::terminate();
  }

  inline void Print(const Vector2d<char>& chars)
  {
    for(int y = 0; y < chars.Height(); ++y)
    {
      for(int x = 0; x < chars.Width(); ++x)
      {
        std::print("{}", chars[Offset(x, y)]);
      }
      std::print("\n");
    }
  }

  inline void Print(const Vector2d<Tile>& tiles)
  {
    Print(tiles.Map([](Tile t) { return CharFromTile(t); }));
  }


  class Map
  {
  };

} // namespace Bot
