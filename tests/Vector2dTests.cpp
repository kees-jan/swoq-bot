#include "Vector2d.h"

#include <string>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

TEST(Vector2dMap, DoublesValues)
{
  std::vector<int> raw{1, 2, 3, 4};
  Vector2d<int>    grid(2, 2, std::move(raw));
  auto             doubled = grid.Map([](const int& v) { return v * 2; });
  static_assert(std::is_same_v<decltype(doubled), Vector2d<int>>);
  ASSERT_EQ(doubled.Width(), 2);
  ASSERT_EQ(doubled.Height(), 2);
  EXPECT_EQ(doubled[0], 2);
  EXPECT_EQ(doubled[1], 4);
  EXPECT_EQ(doubled[2], 6);
  EXPECT_EQ(doubled[3], 8);
}

TEST(Vector2dMap, ChangesTypeToString)
{
  std::vector<int>    raw{5, 6, 7, 8};
  const Vector2d<int> grid(2, 2, std::move(raw));
  auto                asStrings = grid.Map([](const int& v) { return std::to_string(v); });
  static_assert(std::is_same_v<decltype(asStrings), Vector2d<std::string>>);
  ASSERT_EQ(asStrings.Width(), 2);
  ASSERT_EQ(asStrings.Height(), 2);
  EXPECT_EQ(asStrings[0], std::string("5"));
  EXPECT_EQ(asStrings[1], std::string("6"));
  EXPECT_EQ(asStrings[2], std::string("7"));
  EXPECT_EQ(asStrings[3], std::string("8"));
}

TEST(Vector2dMap, EmptyGrid)
{
  Vector2d<int> empty(0, 0, 0);
  auto          mapped = empty.Map([](const int& v) { return v + 1; });
  ASSERT_EQ(mapped.Width(), 0);
  ASSERT_EQ(mapped.Height(), 0);
}
