#pragma once

#include <map>

#include "Swoq.pb.h"

namespace Bot
{
  using Swoq::Interface::Tile;

  struct TileProperties_t
  {
    bool canBePickedUp;
    bool canBeDropped;
    bool isPotentiallyWalkable;
    bool mustBeMapped;
    bool isDoor;
    bool canMove;

    constexpr static TileProperties_t Default()
    {
      return {
        .canBePickedUp = false,
        .canBeDropped = false,
        .isPotentiallyWalkable = true,
        .mustBeMapped = true,
        .isDoor = false,
        .canMove = false};
    }
    constexpr static TileProperties_t Player()
    {
      return {
        .canBePickedUp = false,
        .canBeDropped = false,
        .isPotentiallyWalkable = false,
        .mustBeMapped = false,
        .isDoor = false,
        .canMove = false};
    }
    constexpr static TileProperties_t Wall()
    {
      return {
        .canBePickedUp = false,
        .canBeDropped = false,
        .isPotentiallyWalkable = false,
        .mustBeMapped = true,
        .isDoor = false,
        .canMove = false};
    }
    constexpr static TileProperties_t Door()
    {
      return {
        .canBePickedUp = false,
        .canBeDropped = false,
        .isPotentiallyWalkable = true,
        .mustBeMapped = true,
        .isDoor = true,
        .canMove = false};
    }
    constexpr static TileProperties_t Item()
    {
      return {
        .canBePickedUp = true,
        .canBeDropped = true,
        .isPotentiallyWalkable = true,
        .mustBeMapped = true,
        .isDoor = false,
        .canMove = false};
    }
    constexpr static TileProperties_t Enemy()
    {
      return {
        .canBePickedUp = false,
        .canBeDropped = false,
        .isPotentiallyWalkable = false,
        .mustBeMapped = false,
        .isDoor = false,
        .canMove = true};
    }
    constexpr static TileProperties_t Yada() { return {}; }
  };

  const std::map<Tile, TileProperties_t> TileProperties{
    {Tile::TILE_UNKNOWN, TileProperties_t::Default()},
    {Tile::TILE_EMPTY, TileProperties_t::Default()},
    {Tile::TILE_PLAYER, TileProperties_t::Player()},
    {Tile::TILE_WALL, TileProperties_t::Wall()},
    {Tile::TILE_EXIT, TileProperties_t::Default()},
    {Tile::TILE_DOOR_RED, TileProperties_t::Door()},
    {Tile::TILE_KEY_RED, TileProperties_t::Item()},
    {Tile::TILE_DOOR_GREEN, TileProperties_t::Door()},
    {Tile::TILE_KEY_GREEN, TileProperties_t::Item()},
    {Tile::TILE_DOOR_BLUE, TileProperties_t::Door()},
    {Tile::TILE_KEY_BLUE, TileProperties_t::Item()},
    {Tile::TILE_BOULDER, TileProperties_t::Item()},
    {Tile::TILE_PRESSURE_PLATE_RED, TileProperties_t::Default()},
    {Tile::TILE_PRESSURE_PLATE_GREEN, TileProperties_t::Default()},
    {Tile::TILE_PRESSURE_PLATE_BLUE, TileProperties_t::Default()},
    {Tile::TILE_ENEMY, TileProperties_t::Enemy()},
    {Tile::TILE_SWORD, TileProperties_t::Item()},
    {Tile::TILE_HEALTH, TileProperties_t::Item()},
  };

  constexpr bool IsKey(Tile tile)
  {
    return tile == Tile::TILE_KEY_RED || tile == Tile::TILE_KEY_GREEN || tile == Tile::TILE_KEY_BLUE;
  }

  constexpr bool IsPressurePlate(Tile tile)
  {
    return tile == Tile::TILE_PRESSURE_PLATE_RED || tile == Tile::TILE_PRESSURE_PLATE_GREEN
        || tile == Tile::TILE_PRESSURE_PLATE_BLUE;
  }

  inline bool IsPotentiallyWalkable(Tile tile) { return TileProperties.at(tile).isPotentiallyWalkable; }
  inline bool CanBeDropped(Tile tile) { return TileProperties.at(tile).canBeDropped; }
  inline bool CanBePickedUp(Tile tile) { return TileProperties.at(tile).canBePickedUp; }
  inline bool CanMove(Tile tile) { return TileProperties.at(tile).canMove; }
  inline bool IsDoor(Tile tile) { return TileProperties.at(tile).isDoor; }
} // namespace Bot