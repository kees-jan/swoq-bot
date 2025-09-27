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
    case Tile::TILE_PRESSURE_PLATE_RED:
      return static_cast<char>(174);
    case Tile::TILE_PRESSURE_PLATE_GREEN:
      return static_cast<char>(247);
    case Tile::TILE_PRESSURE_PLATE_BLUE:
      return static_cast<char>(223);
    case Tile::TILE_BOULDER:
      return 'o';
    case Tile::Tile_INT_MAX_SENTINEL_DO_NOT_USE_:
    case Tile::Tile_INT_MIN_SENTINEL_DO_NOT_USE_:
      break;
    }
    std::terminate();
  }

  constexpr bool IsPotentiallyWalkable(Tile tile)
  {
    switch(tile)
    {
    case Tile::TILE_UNKNOWN:
    case Tile::TILE_EMPTY:
    case Tile::TILE_EXIT:
    case Tile::TILE_PLAYER:
    case Tile::TILE_DOOR_RED:
    case Tile::TILE_DOOR_GREEN:
    case Tile::TILE_DOOR_BLUE:
    case Tile::TILE_KEY_RED:
    case Tile::TILE_KEY_GREEN:
    case Tile::TILE_KEY_BLUE:
    case Tile::TILE_PRESSURE_PLATE_RED:
    case Tile::TILE_PRESSURE_PLATE_GREEN:
    case Tile::TILE_PRESSURE_PLATE_BLUE:
    case Tile::TILE_BOULDER:
      return true;

    case Tile::TILE_WALL:
      return false;

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

  struct DoorParameters
  {
    bool avoidDoor = true;
  };

  using DoorParameterMap = std::map<DoorColor, DoorParameters>;
  using DoorOpenMap      = std::map<DoorColor, bool>;

  struct NavigationParameters
  {
    DoorParameterMap             doorParameters{DoorColors
                                    | std::views::transform([](auto color) { return std::pair(color, DoorParameters{}); })
                                    | std::ranges::to<DoorParameterMap>()};
    bool                         avoidBoulders = true;
    std::set<Offset, OffsetLess> badBoulders{};
    std::set<Offset, OffsetLess> goodBoulders{};
  };

  Vector2d<int> WeightMap(const Vector2d<Tile>&        map,
                          const NavigationParameters&  navigationParameters,
                          const std::optional<Offset>& destination = std::nullopt);

  struct TileComparisonResult
  {
    bool needsUpdate   = false;
    bool newBoulder    = false;
    bool stuffHasMoved = false;

    constexpr static TileComparisonResult NoChange() { return TileComparisonResult{}; }
    constexpr static TileComparisonResult NeedsUpdate() { return TileComparisonResult{true, false, false}; }
    constexpr static TileComparisonResult NewBoulder() { return TileComparisonResult{true, true, false}; }
    constexpr static TileComparisonResult StuffHasMoved() { return TileComparisonResult{false, false, true}; }
  };

  struct MapComparisonResult
  {
    Offset                       newMapSize;
    bool                         needsUpdate = false;
    std::set<Offset, OffsetLess> newBoulders;
    bool                         stuffHasMoved = false;

    constexpr explicit MapComparisonResult(Offset newMapSize_)
      : newMapSize(newMapSize_)
    {
    }

    constexpr explicit MapComparisonResult(const Vector2dBase& map)
      : newMapSize(map.Size())
    {
    }

    constexpr void Update(TileComparisonResult tileComparison, Offset position)
    {
      needsUpdate |= tileComparison.needsUpdate;
      stuffHasMoved |= tileComparison.stuffHasMoved;
      if(tileComparison.newBoulder)
      {
        newBoulders.insert(position);
      }
    }
  };

  class Map;

  struct MapUpdateResult
  {
    std::shared_ptr<Map>         map;
    std::set<Offset, OffsetLess> newBoulders;
    bool                         stuffHasMoved = false;

    MapUpdateResult(std::shared_ptr<Map>  map_,
                    MapComparisonResult&& comparisonResult) // NOLINT(*-rvalue-reference-param-not-moved)
      : map(std::move(map_))
      , newBoulders(std::move(comparisonResult.newBoulders))
      , stuffHasMoved(comparisonResult.stuffHasMoved)
    {
    }
  };

  class Map : public Vector2d<Tile>
  {
  public:
    explicit Map(Offset size = {0, 0});
    Map(const Map& other, Offset newSize);
    [[nodiscard]] MapUpdateResult      Update(Offset pos, int visibility, const Vector2d<Tile>& view) const;
    [[nodiscard]] std::shared_ptr<Map> IncludeLocalView(Offset pos, int visibility, const Vector2d<Tile>& view) const;

    [[nodiscard]] std::optional<Offset> Exit() const { return m_exit; }
    [[nodiscard]] const DoorMap&        DoorData() const;
    [[nodiscard]] bool                  IsBadBoulder(Offset position) const;
    [[nodiscard]] bool                  IsGoodBoulder(Offset position) const;

  private:
    [[nodiscard]] MapComparisonResult Compare(Offset pos, const Vector2d<Tile>& view, Offset offset) const;
    void                              Update(Offset pos, const Vector2d<Tile>& view, Offset offset);
    void                              IncludeLocalView(Offset pos, const Vector2d<Tile>& view, Offset offset);
    void                              HandleUnknownBoulder(Offset playerPosition, Offset boulderPosition);

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

  constexpr Tile DoorForColor(DoorColor color)
  {
    switch(color)
    {
    case DoorColor::Red:
      return Tile::TILE_DOOR_RED;
    case DoorColor::Green:
      return Tile::TILE_DOOR_GREEN;
    case DoorColor::Blue:
      return Tile::TILE_DOOR_BLUE;
    }
    assert(false);
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

  std::optional<Offset>
    ReachablePositionNextTo(const Vector2d<Tile>& map, Offset from, Offset to, const NavigationParameters& navigationParameters);

  template <typename Callable>
  std::optional<Offset> ClosestReachable(const Vector2d<Tile>&       map,
                                         Offset                      from,
                                         const NavigationParameters& navigationParameters,
                                         Callable&&                  callable)
  {
    auto weights             = WeightMap(map, navigationParameters);
    auto [dist, destination] = DistanceMap(weights, from, std::forward<Callable>(callable));
    return destination;
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