#pragma once

#include <format>
#include <initializer_list>
#include <queue>
#include <set>
#include <variant>

#include "Offset.h"
#include "PlayerMap.h"
#include "Swoq.pb.h"

namespace Bot
{
  using Swoq::Interface::Tile;

  struct MoveThenUse
  {
    bool done = false;
  };

  struct MoveToGoalThenUse : public MoveThenUse
  {
    constexpr explicit MoveToGoalThenUse(Offset position_)
      : position{position_}
    {
    }

    Offset position;
  };

  struct HuntEnemies
  {
    constexpr explicit HuntEnemies(OffsetSet originalLocations)
      : remainingToCheck{std::move(originalLocations)}
    {
    }

    OffsetSet remainingToCheck;
  };

  constexpr struct Explore_t
  {
  } Explore;

  constexpr struct Terminate_t
  {
  } Terminate;

  constexpr struct Attack_t
  {
  } Attack;

  constexpr struct DropBoulder_t : public MoveThenUse
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

  struct OpenDoor : public MoveToGoalThenUse
  {
    constexpr OpenDoor(Offset doorPosition, DoorColor color_)
      : MoveToGoalThenUse(doorPosition)
      , color{color_}
    {
    }

    DoorColor color;
  };

  struct PlaceBoulderOnPressurePlate : public MoveToGoalThenUse
  {
    constexpr PlaceBoulderOnPressurePlate(Offset pressurePlatePosition, DoorColor color_)
      : MoveToGoalThenUse(pressurePlatePosition)
      , color{color_}
    {
    }

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

  struct FetchBoulder : public MoveToGoalThenUse
  {
    using MoveToGoalThenUse::MoveToGoalThenUse;
  };

  constexpr struct ReconsiderUncheckedBoulders_t
  {
  } ReconsiderUncheckedBoulders;

  constexpr struct Wait_t
  {
  } Wait;

  constexpr struct LeaveSquare_t
  {
    std::optional<Offset> originalSquare;
  } LeaveSquare;

  struct DropDoorOnEnemy
  {
    OffsetSet doorLocations;
    bool waiting = true;

    DropDoorOnEnemy(OffsetSet doorLocations_)
      : doorLocations{std::move(doorLocations_)}
    {
    }
  };

  struct PeekUnderEnemies
  {
    OffsetSet tileLocations;

    PeekUnderEnemies(OffsetSet tileLocations_)
      : tileLocations{std::move(tileLocations_)}
    {
    }
  };

  using Command = std::variant<
    Explore_t,
    VisitTiles,
    Terminate_t,
    Visit,
    FetchKey,
    OpenDoor,
    FetchBoulder,
    DropBoulder_t,
    ReconsiderUncheckedBoulders_t,
    PlaceBoulderOnPressurePlate,
    Wait_t,
    LeaveSquare_t,
    DropDoorOnEnemy,
    Attack_t,
    PeekUnderEnemies,
    HuntEnemies>;
  using Commands = std::queue<Command>;

} // namespace Bot

template <typename... Callable>
struct Visitor : Callable...
{
  using Callable::operator()...;
};
