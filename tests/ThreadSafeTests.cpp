#include "ThreadSafe.h"

#include <memory>
#include <type_traits>

struct StarTS
{
  int operator*() const { return 0; }
};
struct ArrowTS
{
  int  x{0};
  int* operator->() { return &x; }
};

using ProxyInt     = Bot::ThreadSafeProxy<int>;
using ProxyPtrInt  = Bot::ThreadSafeProxy<int*>;
using ProxyUPtrInt = Bot::ThreadSafeProxy<std::unique_ptr<int>>;
using ProxyStar    = Bot::ThreadSafeProxy<StarTS>;
using ProxyArrow   = Bot::ThreadSafeProxy<ArrowTS>;

static_assert(std::is_same_v<decltype(*std::declval<ProxyInt&>()), int&>);
static_assert(std::is_same_v<decltype(*std::declval<const ProxyInt&>()), const int&>);
static_assert(std::is_same_v<decltype(std::declval<ProxyInt&>().operator->()), int*>);
static_assert(std::is_same_v<decltype(std::declval<const ProxyInt&>().operator->()), const int*>);

static_assert(std::is_same_v<decltype(*std::declval<ProxyPtrInt&>()), int&>);
static_assert(std::is_same_v<decltype(std::declval<ProxyPtrInt&>().operator->()), int*&>);
static_assert(std::is_same_v<decltype(std::declval<const ProxyPtrInt&>().operator->()), int* const&>);

static_assert(std::is_same_v<decltype(*std::declval<ProxyUPtrInt&>()), int&>);
static_assert(std::is_same_v<decltype(std::declval<ProxyUPtrInt&>().operator->()), int*>);

static_assert(std::is_same_v<decltype(*std::declval<ProxyStar&>()), int>);
static_assert(std::is_same_v<decltype(std::declval<ProxyStar&>().operator->()), StarTS*>);

static_assert(std::is_same_v<decltype(*std::declval<ProxyArrow&>()), ArrowTS&>);
static_assert(std::is_same_v<decltype(std::declval<ProxyArrow&>().operator->()), int*>);
