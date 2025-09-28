#include "Vector2d.h"

#include <print>

#include "Swoq.hpp"

void Print(const Vector2d<char>& chars)
{
  std::print("+");
  for(int x = 0; x < chars.Width(); ++x)
  {
    std::print("-");
  }
  std::print("+\n");

  for(int y = 0; y < chars.Height(); ++y)
  {
    std::print("|");
    for(int x = 0; x < chars.Width(); ++x)
    {
      std::print("{}", chars[Offset(x, y)]);
    }
    std::print("|\n");
  }

  std::print("+");
  for(int x = 0; x < chars.Width(); ++x)
  {
    std::print("-");
  }
  std::print("+\n");
}

void Print(const Vector2d<int>& ints)
{
  for(int y = 0; y < ints.Height(); ++y)
  {
    for(int x = 0; x < ints.Width(); ++x)
    {
      std::print("{}, ", ints[Offset(x, y)]);
    }
    std::print("\n");
  }
}

void PrintEnum(const Vector2d<Swoq::Interface::Tile>& tiles)
{
  for(int y = 0; y < tiles.Height(); ++y)
  {
    for(int x = 0; x < tiles.Width(); ++x)
    {
      std::print("{}, ", tiles[Offset(x, y)]);
    }
    std::print("\n");
  }
}