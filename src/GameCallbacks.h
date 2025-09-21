#pragma once

namespace Bot
{
  class GameCallbacks
  {
  public:
    virtual ~GameCallbacks() = default;

    virtual void LevelReached(int reportingPlayer, int level) = 0;
  };
} // namespace Bot
