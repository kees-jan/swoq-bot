#pragma once

#include <cassert>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

#include "Offset.h"

class Vector2dBase
{
public:
  constexpr Vector2dBase(int width, int height) noexcept
    : m_width(width)
    , m_height(height)
  {
    assert(m_width >= 0 && m_height >= 0);
  }

  [[nodiscard]] constexpr int Width() const noexcept { return m_width; }
  [[nodiscard]] constexpr int Height() const noexcept { return m_height; }

  [[nodiscard]] constexpr bool IsInRange(const Offset& offset) const noexcept
  {
    return offset.x >= 0 && offset.x < m_width && offset.y >= 0 && offset.y < m_height;
  }

  [[nodiscard]] constexpr std::size_t ToIndex(const Offset& offset) const noexcept
  {
    assert(IsInRange(offset));
    return static_cast<std::size_t>(offset.y * m_width + offset.x);
  }

  [[nodiscard]] constexpr Offset ToOffset(std::size_t index) const noexcept
  {
    assert(index < static_cast<std::size_t>(m_width * m_height));
    const auto i = static_cast<int>(index);
    return Offset{i % m_width, i / m_width};
  }

  [[nodiscard]] constexpr Offset Size() const noexcept { return Offset{m_width, m_height}; }

private:
  int m_width;
  int m_height;
};

template <typename T>
class Vector2d : public Vector2dBase
{
public:
  constexpr Vector2d(int width, int height, T default_value = T()) noexcept
    : Vector2dBase(width, height)
    , data(static_cast<std::size_t>(width * height), default_value)
  {
  }

  constexpr Vector2d(int width, int height, std::vector<T>&& initial_data) noexcept
    : Vector2dBase(width, height)
    , data(std::move(initial_data))
  {
    assert(static_cast<int>(data.size()) == width * height);
  }

  constexpr Vector2d(int width, int height, std::initializer_list<T>&& initial_data) noexcept
    : Vector2dBase(width, height)
    , data(std::move(initial_data))
  {
    assert(static_cast<int>(data.size()) == width * height);
  }
  constexpr const T& operator[](std::size_t index) const noexcept
  {
    assert(index < data.size());
    return data[index];
  }

  constexpr const T& operator[](Offset offset) const noexcept
  {
    assert(IsInRange(offset));
    return data[ToIndex(offset)];
  }

  constexpr T& operator[](std::size_t index) noexcept
  {
    assert(index < data.size());
    return data[index];
  }

  constexpr T& operator[](Offset offset) noexcept
  {
    assert(IsInRange(offset));
    return data[ToIndex(offset)];
  }

  template <typename F>
  auto Map(F&& f) const
  {
    using R = std::invoke_result_t<F, const T&>;
    return Vector2d<R>(Width(), Height(), data | std::views::transform(f) | std::ranges::to<std::vector<R>>());
  }

  constexpr Offset Size() const { return Offset(Width(), Height()); }

  constexpr const std::vector<T>& Data() const noexcept { return data; }

private:
  std::vector<T> data;
};
