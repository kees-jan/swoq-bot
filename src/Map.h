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

  enum class DoorColor : std::uint8_t
  {
    Red,
    Green,
    Blue
  };

  constexpr std::initializer_list<DoorColor> DoorColors{DoorColor::Red, DoorColor::Green, DoorColor::Blue};

  struct DoorData
  {
    std::optional<Offset> keyPosition;
    std::optional<Offset> doorPosition;

    bool operator==(const DoorData& other) const
    {
      return keyPosition == other.keyPosition && doorPosition == other.doorPosition;
    }

    bool operator!=(const DoorData& other) const { return !(*this == other); }
  };

  using DoorMap = std::map<DoorColor, DoorData>;

  class Map : public Vector2d<Tile>
  {
  public:
    Map();
    Map(const Map& other, Offset newSize);
    [[nodiscard]] std::shared_ptr<Map> Update(Offset pos, int visibility, const Vector2d<Tile>& view) const;

    [[nodiscard]] std::optional<Offset> Exit() const { return m_exit; }
    const DoorMap&                      DoorData() const;

  private:
    void Update(Offset pos, const Vector2d<Tile>& view, Offset offset);

    std::optional<Offset> m_exit;
    DoorMap m_doorData{DoorColors | std::views::transform([](auto color) { return std::pair(color, Bot::DoorData{}); })
                       | std::ranges::to<DoorMap>()};
  };

  constexpr bool IsKey(Tile tile)
  {
    return tile == Tile::TILE_KEY_RED || tile == Tile::TILE_KEY_GREEN || tile == Tile::TILE_KEY_BLUE;
  }

  constexpr bool IsDoor(Tile tile)
  {
    return tile == Tile::TILE_DOOR_RED || tile == Tile::TILE_DOOR_GREEN || tile == Tile::TILE_DOOR_BLUE;
  }

  constexpr DoorColor DoorKeyColor(Tile t)
  {
    assert(IsDoor(t) || IsKey(t));

    switch(t)
    {
    case Tile::TILE_DOOR_RED:
    case Tile::TILE_KEY_RED:
      return DoorColor::Red;

    case Tile::TILE_DOOR_GREEN:
    case Tile::TILE_KEY_GREEN:
      return DoorColor::Green;

    case Tile::TILE_DOOR_BLUE:
    case Tile::TILE_KEY_BLUE:
      return DoorColor::Blue;

    default:
      assert(false);
      break;
    }
  }

} // namespace Bot

template <>
struct std::formatter<Bot::DoorColor>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const Bot::DoorColor& color, std::format_context& ctx) const
  {
    using enum Bot::DoorColor;
    std::string s;
    switch(color)
    {
    case Red:
      s = "Red";
      break;
    case Green:
      s = "Green";
      break;
    case Blue:
      s = "Blue";
      break;
    }

    if(s.empty())
      s = "<UNKNOWN>";

    return std::format_to(ctx.out(), "{}", s);
  }
};

template <typename T>
struct std::formatter<std::optional<T>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const std::optional<T>& item, std::format_context& ctx) const
  {
    if(item)
      return std::format_to(ctx.out(), "{}", *item);

    return std::format_to(ctx.out(), "(none)");
  }
};