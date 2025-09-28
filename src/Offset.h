#pragma once

#include <algorithm>
#include <coroutine>
#include <format>
#include <generator>
#include <initializer_list>
#include <set>

#include "Swoq.pb.h"

struct Offset
{
  int x;
  int y;

  constexpr Offset(int x_, int y_)
    : x(x_)
    , y(y_)
  {
  }

  constexpr Offset(Swoq::Interface::Position p)
    : x(p.x())
    , y(p.y())
  {
  }

  constexpr Offset& operator+=(const Offset& other) noexcept
  {
    x += other.x;
    y += other.y;
    return *this;
  }

  constexpr Offset  operator-() const noexcept { return Offset{-x, -y}; }
  constexpr Offset& operator-=(const Offset& other) noexcept { return *this += -other; }

  constexpr Offset& operator*=(int factor) noexcept
  {
    x *= factor;
    y *= factor;
    return *this;
  }
};

constexpr Offset operator+(const Offset& left, const Offset& right) noexcept
{
  Offset result{left};
  return result += right;
}

constexpr Offset operator-(const Offset& left, const Offset& right) noexcept
{
  Offset result{left};
  return result -= right;
}

constexpr Offset operator*(const Offset& offset, int factor) noexcept
{
  Offset result{offset};
  return result *= factor;
}

constexpr Offset operator*(int factor, const Offset& offset) noexcept { return offset * factor; }
constexpr bool   operator==(const Offset& left, const Offset& right) noexcept { return left.x == right.x && left.y == right.y; }
constexpr bool   operator!=(const Offset& left, const Offset& right) noexcept { return !(left == right); }
constexpr Offset max(Offset left, Offset right) { return Offset{std::max(left.x, right.x), std::max(left.y, right.y)}; }


constexpr Offset North(0, -1);
constexpr Offset South(0, 1);
constexpr Offset West(-1, 0);
constexpr Offset East(1, 0);
constexpr Offset NorthEast(North + East);
constexpr Offset SouthEast(South + East);
constexpr Offset SouthWest(South + West);
constexpr Offset NorthWest(North + West);

constexpr Offset Up(North);
constexpr Offset Down(South);
constexpr Offset Left(West);
constexpr Offset Right(East);

constexpr Offset One(SouthEast);

constexpr std::initializer_list<Offset> Directions{Up, Down, Left, Right};
constexpr std::initializer_list<Offset> AllDirections{North, NorthEast, East, SouthEast, South, SouthWest, West, NorthWest};

struct OffsetLess
{
  constexpr bool operator()(const Offset& a, const Offset& b) const noexcept
  {
    if(a.y < b.y)
    {
      return true;
    }
    if(a.y > b.y)
    {
      return false;
    }
    return a.x < b.x;
  }
};

inline std::generator<Offset> OffsetsInRectangle(Offset maxExclusive)
{
  if(maxExclusive.x <= 0 || maxExclusive.y <= 0)
  {
    co_return;
  }
  for(int y = 0; y < maxExclusive.y; ++y)
  {
    for(int x = 0; x < maxExclusive.x; ++x)
    {
      co_yield Offset{x, y};
    }
  }
}

using OffsetSet = std::set<Offset, OffsetLess>;

template <>
struct std::formatter<Offset>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const Offset& offset, std::format_context& ctx) const
  {
    return std::format_to(ctx.out(), "{{{}, {}}}", offset.x, offset.y);
  }
};
