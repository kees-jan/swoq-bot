#pragma once

#include <initializer_list>
#include <queue>
#include <set>
#include <variant>

#include "Map.h"
#include "Offset.h"
#include "Swoq.pb.h"

namespace Bot
{
  using Swoq::Interface::Tile;

  constexpr struct Explore_t
  {
  } Explore;

  constexpr struct Terminate_t
  {
  } Terminate;

  constexpr struct DropBoulder_t
  {
  } DropBoulder;

  struct VisitTiles
  {
    constexpr explicit VisitTiles(Tile tile)
      : tiles{tile}
    {
    }
    constexpr VisitTiles(std::initializer_list<Tile> tiles_)
      : tiles{tiles_}
    {
    }

    std::set<Tile> tiles;
  };

  struct Visit
  {
    constexpr explicit Visit(Offset position_)
      : position{position_}
    {
    }

    Offset position;
  };

  struct OpenDoor
  {
    constexpr OpenDoor(Offset doorPosition, DoorColor color_)
      : position{doorPosition}
      , color{color_}
    {
    }

    Offset    position;
    DoorColor color;
  };

  struct FetchKey
  {
    constexpr FetchKey(Offset keyPosition)
      : position{keyPosition}
    {
    }

    Offset position;
  };

  struct FetchBoulder
  {
    constexpr FetchBoulder(Offset keyPosition)
      : position{keyPosition}
    {
    }

    Offset position;
  };

  constexpr struct ReconsiderUncheckedBoulders_t
  {
  } ReconsiderUncheckedBoulders;


  using Command  = std::variant<Explore_t,
                                VisitTiles,
                                Terminate_t,
                                Visit,
                                FetchKey,
                                OpenDoor,
                                FetchBoulder,
                                DropBoulder_t,
                                ReconsiderUncheckedBoulders_t>;
  using Commands = std::queue<Command>;

} // namespace Bot

template <typename... Callable>
struct Visitor : Callable...
{
  using Callable::operator()...;
};