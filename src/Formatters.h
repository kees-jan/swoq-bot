#pragma once

#include <format>
#include <map>
#include <optional>
#include <set>

template <typename T>
struct std::formatter<std::optional<T>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const std::optional<T>& item, std::format_context& ctx) const
  {
    if(item)
      return std::format_to(ctx.out(), "{}", *item);

    return std::format_to(ctx.out(), "(none)");
  }
};

template <typename Key, typename Compare, typename Allocator>
struct std::formatter<std::set<Key, Compare, Allocator>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const std::set<Key, Compare, Allocator>& item, std::format_context& ctx) const
  {
    auto cur = item.begin();
    auto end = item.end();
    if(item.empty())
    {
      return std::format_to(ctx.out(), "(empty)");
    }

    auto it = std::format_to(ctx.out(), "{{");

    if(cur != end)
    {
      it = std::format_to(it, "{}", *cur++);
    }
    while(cur != end)
    {
      it = std::format_to(it, ", {}", *cur++);
    }

    return std::format_to(it, "}}");
  }
};

template <typename Key, typename T, typename Compare, typename Allocator>
struct std::formatter<std::map<Key, T, Compare, Allocator>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const std::map<Key, T, Compare, Allocator>& item, std::format_context& ctx) const
  {
    auto cur = item.begin();
    auto end = item.end();
    if(item.empty())
    {
      return std::format_to(ctx.out(), "(empty)");
    }

    auto it = std::format_to(ctx.out(), "{{");

    if(cur != end)
    {
      it = std::format_to(it, "{}", *cur++);
    }
    while(cur != end)
    {
      it = std::format_to(it, ", {}", *cur++);
    }

    return std::format_to(it, "}}");
  }
};

template <typename Left, typename Right>
struct std::formatter<std::pair<Left, Right>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto           format(const std::pair<Left, Right>& item, std::format_context& ctx) const
  {
    return std::format_to(ctx.out(), "{{{}, {}}}", item.first, item.second);
  }
};