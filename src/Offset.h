#pragma once

struct Offset
{
  int x;
  int y;

  constexpr Offset(int x, int y)
    : x(x)
    , y(y)
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
