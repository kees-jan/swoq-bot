#include "Offset.h"

#include <vector>

#include <gtest/gtest.h>

TEST(OffsetsUpTo, Single)
{
  std::vector<Offset> got;
  for(auto o: OffsetsInRectangle({1, 1}))
    got.push_back(o);
  ASSERT_EQ(got.size(), 1u);
  EXPECT_EQ(got[0], Offset(0, 0));
}

TEST(OffsetsUpTo, Rectangle)
{
  std::vector<Offset> got;
  for(auto o: OffsetsInRectangle({3, 2}))
    got.push_back(o);
  std::vector<Offset> expected{Offset(0, 0), Offset(1, 0), Offset(2, 0), Offset(0, 1), Offset(1, 1), Offset(2, 1)};
  ASSERT_EQ(got.size(), expected.size());
  for(size_t i = 0; i < expected.size(); ++i)
    EXPECT_EQ(got[i], expected[i]);
}

TEST(OffsetsUpTo, ZeroOrNegativeReturnsEmpty)
{
  std::vector<Offset> got;
  for(auto o: OffsetsInRectangle({0, 3}))
    got.push_back(o);
  EXPECT_TRUE(got.empty());
  got.clear();
  for(auto o: OffsetsInRectangle({3, 0}))
    got.push_back(o);
  EXPECT_TRUE(got.empty());
  got.clear();
  for(auto o: OffsetsInRectangle({-1, 2}))
    got.push_back(o);
  EXPECT_TRUE(got.empty());
  got.clear();
  for(auto o: OffsetsInRectangle({2, -5}))
    got.push_back(o);
  EXPECT_TRUE(got.empty());
}
