#pragma once

namespace Bot
{
  class GameCallbacks
  {
  public:
    virtual ~GameCallbacks() = default;

    virtual void LevelReached(int reportingPlayer, int level) = 0;
    virtual void MapUpdated(int id)                           = 0;
    virtual void PrintMap()                                   = 0;
    virtual void Finished(int m_id)                           = 0;
  };
} // namespace Bot
