//
// Created by kees-jan on 26 Oct 2025.
//

#include "Map.h"

namespace Bot
{
  std::vector<Tile> NewMapData(const Vector2d<Tile>& other, Offset newSize)
  {
    assert(newSize.x >= other.Width());
    assert(newSize.y >= other.Height());

    std::vector<Tile> tiles(static_cast<std::size_t>(newSize.x * newSize.y), Tile::TILE_UNKNOWN);
    const auto& original = other.Data();
    for(int y = 0; y < other.Height(); ++y)
    {
      std::copy_n(original.begin() + y * other.Width(), other.Width(), tiles.begin() + y * newSize.x);
    }
    return tiles;
  }

  Vector2d<Swoq::Interface::Tile> ViewFromState(int visibility, const Swoq::Interface::PlayerState& state)
  {
    int visibility_dimension = 2 * visibility + 1;
    assert(state.surroundings_size() == visibility_dimension * visibility_dimension);
    return Vector2d(
      visibility_dimension,
      visibility_dimension,
      state.surroundings() | std::views::transform([](int t) { return static_cast<Tile>(t); })
        | std::ranges::to<std::vector<Tile>>());
  }

  void Print(const Vector2d<Tile>& tiles)
  {
    Print(tiles.Map([](Tile t) { return CharFromTile(t); }));
  }

} // namespace Bot