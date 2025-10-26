#pragma once

#include <memory>

#include "Dijkstra.h"
#include "LoggingAndDebugging.h"
#include "Map.h"
#include "Swoq.pb.h"
#include "TileProperties.h"
#include "Vector2d.h"

namespace Bot
{
  class DungeonMap
    : public Vector2d<Tile>
    , public std::enable_shared_from_this<DungeonMap>
  {
  public:
    using Ptr = std::shared_ptr<const DungeonMap>;

    explicit DungeonMap(Offset size);
    DungeonMap(const DungeonMap& other, Offset newSize);
    [[nodiscard]] static Ptr Create(Offset size = {0, 0});
    [[nodiscard]] static Ptr Create(const DungeonMap& other, Offset newSize);
    [[nodiscard]] Ptr Update(Offset pos, int visibility, const Vector2d<Tile>& view) const;

    [[nodiscard]] int GetVersion();

  private:
    struct ComparisonResult
    {
      Offset newMapSize;
      bool needsUpdate = false;

      constexpr explicit ComparisonResult(const Vector2dBase& map)
        : newMapSize(map.Size())
      {
      }

      constexpr void Update(bool needsUpdate_) { needsUpdate |= needsUpdate_; }
    };

    [[nodiscard]] ComparisonResult Compare(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert) const;
    void Update(const Vector2d<Tile>& view, const MapViewCoordinateConverter& convert);

    int m_version = 0;
  };

} // namespace Bot
