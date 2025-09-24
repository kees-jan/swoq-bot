#pragma once

#include <variant>

namespace Bot
{
  constexpr struct Explore_t
  {
  } Explore;

  using Command = std::variant<Explore_t>;

} // namespace Bot