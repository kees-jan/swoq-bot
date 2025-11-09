#pragma once

#include "Dijkstra.h"
#include "LoggingAndDebugging.h"
#include "Map.h"
#include "Swoq.pb.h"
#include "TileProperties.h"
#include "Vector2d.h"

namespace Bot
{
  enum class DoorColor : std::uint8_t
  {
    Red,
    Green,
    Blue
  };

  constexpr std::initializer_list<DoorColor> DoorColors{DoorColor::Red, DoorColor::Green, DoorColor::Blue};
  constexpr int EnemyPenalty = 15;

  struct DoorData
  {
    std::optional<Offset> keyPosition;
    std::optional<Offset> pressurePlatePosition;
    OffsetSet doorPosition;

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
  using DoorOpenMap = std::map<DoorColor, bool>;

  struct NavigationParameters
  {
    DoorParameterMap doorParameters{
      DoorColors | std::views::transform([](auto color) { return std::pair(color, DoorParameters{}); })
      | std::ranges::to<DoorParameterMap>()};
    bool avoidEnemies = true;
  };

  struct Enemies
  {
    OffsetMap<int> locations{};
    std::array<OffsetSet, 2> inSight{};
  };

  struct TileComparisonResult
  {
    bool needsUpdate = false;
    bool newBoulder = false;
    bool isEnemy = false;

    constexpr static TileComparisonResult NoChange() { return {}; }
    constexpr static TileComparisonResult NeedsUpdate() { return {.needsUpdate = true}; }
    constexpr static TileComparisonResult NewBoulder() { return {.needsUpdate = true, .newBoulder = true}; }
    constexpr static TileComparisonResult Enemy() { return {.isEnemy = true}; }
  };

  struct MapComparisonResult
  {
    Offset newMapSize;
    bool needsUpdate = false;
    OffsetSet newBoulders;
    OffsetSet enemies;
    OffsetSet disappearedEnemies;

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
      if(tileComparison.newBoulder)
      {
        newBoulders.insert(position);
      }
      if(tileComparison.isEnemy)
      {
        enemies.insert(position);
      }
    }
  };

  class PlayerMap;

  class PlayerMap
    : public Vector2d<Tile>
    , public std::enable_shared_from_this<PlayerMap>
  {
  public:
    using Ptr = std::shared_ptr<const PlayerMap>;

    explicit PlayerMap(Offset size = {0, 0});
    PlayerMap(const PlayerMap& other, Offset newSize);
    std::shared_ptr<PlayerMap> Clone() const;
    [[nodiscard]] Ptr Update(size_t playerId, Offset pos, int visibility, const Vector2d<Tile>& view) const;

    [[nodiscard]] std::optional<Offset> Exit() const { return m_exit; }
    [[nodiscard]] const DoorMap& DoorData() const;
    [[nodiscard]] bool IsBadBoulder(Offset position) const;
    [[nodiscard]] bool IsGoodBoulder(Offset position) const;

    Bot::NavigationParameters& NavigationParameters();
    const Bot::NavigationParameters& NavigationParameters() const;

    OffsetSet uncheckedBoulders{};
    OffsetSet usedBoulders{};
    Enemies enemies;

  private:
    [[nodiscard]] MapComparisonResult Compare(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert) const;
    void Update(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert);

    Bot::NavigationParameters m_navigationParameters;
    std::optional<Offset> m_exit;
    DoorMap m_doorData{
      DoorColors | std::views::transform([](auto color) { return std::pair(color, Bot::DoorData{}); })
      | std::ranges::to<DoorMap>()};
  };

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

  constexpr DoorColor DoorKeyPlateColor(Tile t)
  {
    assert(IsDoor(t) || IsKey(t) || IsPressurePlate(t));

    switch(t)
    {
    case Tile::TILE_DOOR_RED:
    case Tile::TILE_KEY_RED:
    case Tile::TILE_PRESSURE_PLATE_RED:
      return DoorColor::Red;

    case Tile::TILE_DOOR_GREEN:
    case Tile::TILE_KEY_GREEN:
    case Tile::TILE_PRESSURE_PLATE_GREEN:
      return DoorColor::Green;

    case Tile::TILE_DOOR_BLUE:
    case Tile::TILE_KEY_BLUE:
    case Tile::TILE_PRESSURE_PLATE_BLUE:
      return DoorColor::Blue;

    default:
      assert(false);
      break;
    }
  }

  template <typename Callable>
    requires std::is_invocable_v<Callable, Offset>
  void AvoidEnemies(const OffsetSet& enemyLocations, Vector2d<int>& weights, Callable&& callable)
  {
    for(auto location: enemyLocations)
    {
      if(!std::invoke(std::forward<Callable>(callable), location))
        weights[location] = Infinity(weights);

      auto positions =
        Directions | std::views::transform([&](Offset o) { return location + o; })
        | std::views::filter(
          [&](Offset o)
          { return weights.IsInRange(o) && !std::invoke(std::forward<Callable>(callable), o) && weights[o] < EnemyPenalty; });

      for(auto p: positions)
      {
        weights[p] = EnemyPenalty;
      }
    }
  }

  Vector2d<int>
    WeightMap(size_t index, const Vector2d<Tile>& map, const Enemies& enemies, const NavigationParameters& navigationParameters);
  Vector2d<int> WeightMap(
    size_t playerId,
    const Vector2d<Tile>& map,
    const Enemies& enemies,
    const NavigationParameters& navigationParameters,
    Offset destination);
  Vector2d<int> WeightMap(
    size_t index,
    const Vector2d<Tile>& map,
    const Enemies& enemies,
    const NavigationParameters& navigationParameters,
    const std::optional<Offset>& destination);

  template <typename Callable>
    requires std::is_invocable_v<Callable, Offset>
  Vector2d<int> WeightMap(
    size_t playerId,
    const Vector2d<Tile>& map,
    const Enemies& enemies,
    const NavigationParameters& navigationParameters,
    Callable&& callable)
  {
    const int Inf = Infinity(map);
    Vector2d weights(map.Width(), map.Height(), Inf);

    for(const auto offset: OffsetsInRectangle(map.Size()))
    {
      auto tile = map[offset];
      weights[offset] =
        (!std::invoke(std::forward<Callable>(callable), offset)
         && (tile == Tile::TILE_WALL || tile == Tile::TILE_BOULDER
             || tile == Tile::TILE_ENEMY
             || (IsDoor(tile) && navigationParameters.doorParameters.at(DoorKeyPlateColor(tile)).avoidDoor) || IsKey(tile)))
          ? Inf
          : 1;
    }
    if(navigationParameters.avoidEnemies)
      AvoidEnemies(enemies.inSight[playerId], weights, std::forward<Callable>(callable));

    if constexpr(Debugging::PrintWeightMap)
    {
      std::println("Weight map {}:", playerId);
      Print(weights);
    }
    return weights;
  }

  // std::optional<Offset>
  //   ReachablePositionNextTo(const Vector2d<Tile>& map, Offset from, Offset to, const NavigationParameters&
  //   navigationParameters);

  // template <typename Callable>
  //   requires std::is_invocable_v<Callable, Offset>
  // std::optional<Offset> ClosestReachable(
  //   const Vector2d<Tile>& map,
  //   Offset from,
  //   const NavigationParameters& navigationParameters,
  //   Callable&& callable)
  // {
  //   auto weights = WeightMap(map, TODO, navigationParameters);
  //   auto [dist, destination] = DistanceMap(weights, from, std::forward<Callable>(callable));
  //   return destination;
  // }

} // namespace Bot

template <>
struct std::formatter<Bot::DoorColor>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const Bot::DoorColor& color, std::format_context& ctx) const
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

template <>
struct std::formatter<Bot::DoorData>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const Bot::DoorData& data, std::format_context& ctx) const
  {
    return std::format_to(
      ctx.out(), "{{Key: {}, Plate: {}, Door: {}}}", data.keyPosition, data.pressurePlatePosition, data.doorPosition);
  }
};

template <>
struct std::formatter<Bot::NavigationParameters>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const Bot::NavigationParameters& parameters, std::format_context& ctx) const
  {
    return std::format_to(
      ctx.out(), "{{DoorParameters: {}, AvoidEnemies: {}}}", parameters.doorParameters, parameters.avoidEnemies);
  }
};

template <>
struct std::formatter<Bot::DoorParameters>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const Bot::DoorParameters& parameters, std::format_context& ctx) const
  {
    return std::format_to(ctx.out(), "{}", parameters.avoidDoor);
  }
};

template <>
struct std::formatter<Bot::Enemies>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const Bot::Enemies& enemies, std::format_context& ctx) const
  {
    return std::format_to(ctx.out(), "{{InSight: {}, Locations:{}}}", enemies.inSight, enemies.locations);
  }
};
