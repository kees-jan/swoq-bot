#pragma once

#include <algorithm>
#include <cassert>
#include <optional>
#include <print>
#include <queue>
#include <ranges>
#include <tuple>
#include <utility>

#include <LoggingAndDebugging.h>

#include "Formatters.h"
#include "Offset.h"
#include "Vector2d.h"

namespace Bot
{
  namespace Detail
  {
    struct QueueEntry
    {
      int    distance;
      Offset offset;

      QueueEntry(int distance_, Offset offset_)
        : distance(distance_)
        , offset(offset_)
      {
      }
      bool operator<(const QueueEntry& other) const { return distance > other.distance; }
    };

    const std::vector<std::initializer_list<Offset>> MixedDirections{
      {Up, Right, Down, Left},
      {Left, Down, Right, Up},
    };
  } // namespace Detail

  constexpr int Infinity(const Vector2dBase& v) { return 2 * v.Width() * v.Height() * 100; }

  template <typename Callable>
  std::tuple<Vector2d<int>, std::optional<Offset>> DistanceMap(const Vector2d<int>& weights, Offset start, Callable&& c)
  {
    assert(weights.IsInRange(start));

    std::optional<Offset> destination;
    Vector2d<int>         dist(weights.Width(), weights.Height(), Infinity(weights));
    dist[start] = 0;
    std::priority_queue<Detail::QueueEntry> pq;
    pq.emplace(0, start);
    while(!pq.empty())
    {
      auto [d, p] = pq.top();
      pq.pop();
      assert(dist[p] == d);

      if(std::invoke(std::forward<Callable>(c), p))
      {
        destination = p;
        break;
      }

      for(const auto& dir: Directions)
      {
        const auto np = p + dir;
        if(!dist.IsInRange(np))
        {
          continue;
        }
        const int nd = d + weights[np];
        if(nd < dist[np])
        {
          dist[np] = nd;
          pq.emplace(nd, np);
        }
      }
    }
    if constexpr(Debugging::PrintDistanceMap)
    {
      std::println("Distance map:");
      Print(dist);
    }
    return {dist, destination};
  }

  inline Vector2d<int> DistanceMap(const Vector2d<int>& weights, Offset start)
  {
    return std::get<0>(DistanceMap(weights, start, [](Offset) { return false; }));
  }

  template <typename Callable>
  std::vector<Offset> ReversedPath(const Vector2d<int>& weights, Offset start, Callable&& c)
  {

    auto [dist, destination] = DistanceMap(weights, start, std::forward<Callable>(c));
    std::vector<Offset> path;
    if(destination)
    {
      auto d = *destination;
      if(dist[d] < Infinity(weights))
      {
        bool toggle = false;
        while(d != start)
        {
          path.push_back(d);

          auto possibilities = Detail::MixedDirections[toggle] | std::views::transform([&](Offset o) { return d + o; })
                               | std::views::filter([&](Offset p) { return dist.IsInRange(p); });

          auto it = std::ranges::min_element(possibilities, [&](Offset a, Offset b) { return dist[a] < dist[b]; });
          assert(it != possibilities.end());
          d      = *it;
          toggle = !toggle;
        }
      }
    }

    return path;
  }


} // namespace Bot