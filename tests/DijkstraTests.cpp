#include "Dijkstra.h"

#include <limits>
#include <vector>

#include <gtest/gtest.h>

using Bot::DistanceMap;

TEST(Dijkstra, SingleCell)
{
  const Vector2d<int> weights(1, 1, 7);
  auto                dist = DistanceMap(weights, Offset(0, 0));
  ASSERT_EQ(dist.Width(), 1);
  ASSERT_EQ(dist.Height(), 1);
  EXPECT_EQ(dist[0], 0); // start cost is zero, start cell weight not added
}

TEST(Dijkstra, Uniform3x3CenterStart)
{
  const Vector2d<int> weights(3, 3, 1);
  auto                dist = DistanceMap(weights, Offset(1, 1));
  std::vector<int>    expected{2, 1, 2, 1, 0, 1, 2, 1, 2};
  ASSERT_EQ(dist.Width(), 3);
  ASSERT_EQ(dist.Height(), 3);
  for(std::size_t i = 0; i < 9; ++i)
  {
    EXPECT_EQ(dist[i], expected[i]);
  }
}

TEST(Dijkstra, ChoosesCheaperDetour)
{
  // Grid 3x2 (width=3,height=2). Layout (row-major):
  // y=0: 1 100 1
  // y=1: 1   1 1
  // Best path to (2,0) avoids 100 cost.
  const Vector2d<int> weights(3, 2, {1, 100, 1, 1, 1, 1});
  auto                dist = DistanceMap(weights, Offset(0, 0));
  // Expected distances row-major: 0,100,4, 1,2,3
  EXPECT_EQ(dist[0], 0);
  EXPECT_EQ(dist[1], Bot::Infinity(weights));
  EXPECT_EQ(dist[2], 4);
  EXPECT_EQ(dist[3], 1);
  EXPECT_EQ(dist[4], 2);
  EXPECT_EQ(dist[5], 3);
}

TEST(Dijkstra, StartNotTopLeft)
{
  // 4x3 with varying costs. Start inside.
  const Vector2d<int> weights(4, 3, {1, 2, 3, 4, 5, 1, 5, 1, 2, 2, 2, 2});
  auto                dist = DistanceMap(weights, Offset(2, 1)); // start at weight 5 (index (2,1))
  // We just validate a few spot checks relative to start.
  EXPECT_EQ(dist[Offset(2, 1)], 0);
  // Neighbor costs: (1,1)=1, (3,1)=1, (2,0)=3, (2,2)=2
  EXPECT_EQ(dist[Offset(1, 1)], 1);
  EXPECT_EQ(dist[Offset(3, 1)], 1);
  EXPECT_EQ(dist[Offset(2, 0)], 3);
  EXPECT_EQ(dist[Offset(2, 2)], 2);
  // A longer path: to (0,0) minimal path (2,1)->(1,1)->(0,1)->(0,0)
  EXPECT_EQ(dist[Offset(0, 0)], 4);
}
