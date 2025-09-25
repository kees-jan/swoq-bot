#pragma once

#include <initializer_list>
#include <queue>
#include <set>
#include <variant>

#include "Offset.h"
#include "Swoq.pb.h"

namespace Bot
{
  using Swoq::Interface::Tile;

  constexpr struct Explore_t
  {
  } Explore;

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

  constexpr struct Wait_t
  {
  } Wait;


  using Command  = std::variant<Explore_t, VisitTiles>;
  using Commands = std::queue<Command>;

} // namespace Bot

template <typename... Callable>
struct Visitor : Callable...
{
  using Callable::operator()...;
};