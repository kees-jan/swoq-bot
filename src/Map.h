#pragma once

#include "Swoq.pb.h"
#include "Vector2d.h"

namespace Bot
{
  using Swoq::Interface::Tile;

  class MapViewCoordinateConverter
  {
  public:
    constexpr MapViewCoordinateConverter(Offset mapPosition, int visibility, const Vector2d<Tile>& view)
      : m_position(mapPosition)
      , m_offset(visibility, visibility)
    {
      assert(view.Size() == 2 * m_offset + One);
    }

    [[nodiscard]] constexpr Offset ToMap(Offset viewPosition) const { return m_position + viewPosition - m_offset; }
    [[nodiscard]] constexpr Offset ToView(Offset mapPosition) const { return mapPosition - m_position + m_offset; }
    [[nodiscard]] constexpr Offset MapPosition() const { return m_position; }

  private:
    Offset m_position;
    Offset m_offset;
  };

  std::vector<Tile> NewMapData(const Vector2d<Tile>& other, Offset newSize);

  Vector2d<Tile> ViewFromState(int visibility, const Swoq::Interface::PlayerState& state);
  void Print(const Vector2d<Tile>& tiles);

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
      return 'X';
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
    case Tile::TILE_PRESSURE_PLATE_RED:
      return static_cast<char>(174);
    case Tile::TILE_PRESSURE_PLATE_GREEN:
      return static_cast<char>(247);
    case Tile::TILE_PRESSURE_PLATE_BLUE:
      return static_cast<char>(223);
    case Tile::TILE_BOULDER:
      return 'o';
    case Tile::TILE_ENEMY:
      return 'E';
    case Tile::TILE_SWORD:
      return 'S';
    case Tile::TILE_HEALTH:
      return 'H';
    case Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_:
    case Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_:
      break;
    }
    std::terminate();
  }

} // namespace Bot
