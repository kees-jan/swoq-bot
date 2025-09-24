#pragma once

#include "Swoq.pb.h"
#include "Vector2d.h"

namespace Bot
{
  using Swoq::Interface::Tile;

  Vector2d<Tile> ViewFromState(int visibility, const Swoq::Interface::PlayerState& state);
  void           Print(const Vector2d<char>& chars);
  void           Print(const Vector2d<Tile>& tiles);
  void           Print(const Vector2d<int>& ints);

  constexpr char CharFromTile(Tile tile)
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
    case Tile::TILE_DOOR_RED:
      return 'R';
    case Tile::TILE_DOOR_GREEN:
      return 'G';
    case Tile::TILE_DOOR_BLUE:
      return 'B';
    case Tile::TILE_KEY_RED:
      return 'r';
    case Tile::TILE_KEY_GREEN:
      return 'g';
    case Tile::TILE_KEY_BLUE:
      return 'b';
    case Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_:
    case Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_:
      break;
    }
    std::terminate();
  }

  class Map : public Vector2d<Tile>
  {
  public:
    Map();
    Map(const Map& other, Offset newSize);
    [[nodiscard]] std::shared_ptr<Map> Update(Offset pos, int visibility, const Vector2d<Tile>& view) const;

    [[nodiscard]] std::optional<Offset> Exit() const { return m_exit; }

  private:
    void Update(Offset pos, const Vector2d<Tile>& view, Offset offset);

    std::optional<Offset> m_exit;
  };

  Vector2d<int> WeightMap(const Vector2d<Tile>& map);


} // namespace Bot
