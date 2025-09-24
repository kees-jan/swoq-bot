#include "TypeTraits.h"

struct Star
{
  int operator*() const { return 0; }
};
struct Arrow
{
  int  x{0};
  int* operator->() { return &x; }
};
struct ConstArrow
{
  int        x{0};
  const int* operator->() const { return &x; }
};

static_assert(Bot::has_dereference_v<int*>);
static_assert(Bot::has_arrow_v<int*>);
static_assert(Bot::has_dereference_v<Star>);
static_assert(!Bot::has_arrow_v<Star>);
static_assert(Bot::has_arrow_v<Arrow>);
static_assert(Bot::has_arrow_v<ConstArrow>);
static_assert(!Bot::has_dereference_v<int>);
static_assert(!Bot::has_arrow_v<int>);
static_assert(Bot::has_dereference_v<std::unique_ptr<int>>);
static_assert(Bot::has_arrow_v<std::unique_ptr<int>>);
