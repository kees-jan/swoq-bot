#pragma once

namespace Bot
{
  class GameCallbacks
  {
  public:
    virtual ~GameCallbacks() = default;

    virtual void LevelReached(int level) = 0;
    virtual void MapUpdated() = 0;
    virtual void PrintDungeonMap() = 0;
    virtual void Finished(size_t m_id) = 0;
  };
} // namespace Bot
