#pragma once

#include <array>
#include <expected>
#include <format>
#include <map>
#include <optional>
#include <set>

template <typename ContainerIterator, typename OutputIterator>
OutputIterator formatRange(ContainerIterator cur, const ContainerIterator& end, OutputIterator it);

template <typename T>
struct std::formatter<std::optional<T>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const std::optional<T>& item, std::format_context& ctx) const
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
  auto format(const std::set<Key, Compare, Allocator>& item, std::format_context& ctx) const
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
  auto format(const std::map<Key, T, Compare, Allocator>& item, std::format_context& ctx) const -> decltype(ctx.out())
  {
    return formatRange(item.begin(), item.end(), ctx.out());
  }
};

template <typename T, size_t Count>
struct std::formatter<std::array<T, Count>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const std::array<T, Count>& item, std::format_context& ctx) const -> decltype(ctx.out())
  {
    return formatRange(item.begin(), item.end(), ctx.out());
  }
};


template <typename Left, typename Right>
struct std::formatter<std::pair<Left, Right>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const std::pair<Left, Right>& item, std::format_context& ctx) const
  {
    return std::format_to(ctx.out(), "{{{}, {}}}", item.first, item.second);
  }
};

template <typename T, typename E>
struct std::formatter<std::expected<T, E>>
{
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const std::expected<T, E>& item, std::format_context& ctx) const
  {
    if(item.has_value())
      return std::format_to(ctx.out(), "{}", item.value());
    return std::format_to(ctx.out(), "(error: {})", item.error());
  }
};

template <typename ContainerIterator, typename OutputIterator>
OutputIterator formatRange(ContainerIterator cur, const ContainerIterator& end, OutputIterator it)
{
  if(cur == end)
  {
    return std::format_to(it, "(empty)");
  }

  it = std::format_to(it, "{{");

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
