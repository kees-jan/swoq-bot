#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "Dijkstra.h"

using Bot::DistanceMap;
using Bot::ReversedPath;
namespace
{

  Vector2d<int> MakeUniform(int w, int h, int value) { return Vector2d<int>(w, h, value); }

  int PathWeightSum(const std::vector<Offset>& path, const Vector2d<int>& weights)
  {
    int sum = 0;
    for(auto& o: path)
      sum += weights[o];
    return sum;
  }

} // namespace

TEST(ReversedPath, EmptyWhenPredicateMatchesNothing)
{
  auto weights = MakeUniform(3, 3, 1);
  auto path    = ReversedPath(weights, Offset(0, 0), [](Offset) { return false; });
  EXPECT_TRUE(path.empty());
}

TEST(ReversedPath, EmptyWhenPredicateMatchesStart)
{
  auto weights = MakeUniform(4, 4, 1);
  auto path    = ReversedPath(weights, Offset(1, 1), [](Offset o) { return o == Offset(1, 1); });
  EXPECT_TRUE(path.empty());
}

TEST(ReversedPath, SimpleCornerPathUniformWeights)
{
  auto   weights = MakeUniform(3, 3, 1);
  Offset start(0, 0);
  Offset target(2, 2);
  auto   path = ReversedPath(weights, start, [&](Offset o) { return o == target; });
  ASSERT_EQ(path.size(), 4u); // Manhattan distance
  EXPECT_EQ(path.front(), target);
  // Last element should be adjacent to start
  auto last = path.back();
  EXPECT_TRUE((std::abs(last.x - start.x) + std::abs(last.y - start.y)) == 1);
  // Distances strictly decreasing along path (since weights all 1)
  auto [dist, _] = DistanceMap(weights, start, [](Offset) { return false; });
  int prev       = dist[path.front()];
  for(size_t i = 1; i < path.size(); ++i)
  {
    int d = dist[path[i]];
    EXPECT_EQ(d, prev - 1);
    prev = d;
  }
}

TEST(ReversedPath, ChoosesLowerCostDetour)
{
  // Layout 3x3 (row-major):
  // 1 100 1
  // 1 100 1
  // 1   1 1
  std::vector<int> raw{1, 100, 1, 1, 100, 1, 1, 1, 1};
  Vector2d<int>    weights(3, 3, std::move(raw));
  Offset           start(0, 0);
  Offset           target(2, 0);
  auto             path = ReversedPath(weights, start, [&](Offset o) { return o == target; });
  // Optimal path cost = 6 (down to bottom row, across, then up twice)
  ASSERT_EQ(path.size(), 6u); // cost equals number of nodes excluding start
  EXPECT_EQ(path.front(), target);
  // Verify weight sum equals distance stored
  auto [dist, _] = DistanceMap(weights, start, [](Offset) { return false; });
  int sum        = PathWeightSum(path, weights);
  EXPECT_EQ(sum, dist[target]);
}

TEST(ReversedPath, UnreachableTargetReturnsEmpty)
{
  // Use very large weights as wall (simulate by making them enormous so not chosen)
  // Force predicate to look for a coordinate outside grid indirectly by never matching inside.
  auto weights = MakeUniform(2, 2, 1);
  auto path    = ReversedPath(weights, Offset(0, 0), [](Offset o) { return o == Offset(3, 3); });
  EXPECT_TRUE(path.empty());
}
