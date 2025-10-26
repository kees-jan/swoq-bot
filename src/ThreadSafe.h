#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <type_traits>

#include "TypeTraits.h"

namespace Bot
{

  template <typename T>
  class ThreadSafeProxy;

  template <typename T>
  class ThreadSafe
  {
  public:
    ThreadSafe(T initial = T())
      : m_value(initial)
    {
    }

    T Get()
    {
      std::unique_lock lock(m_mutex);
      return m_value;
    }

    ThreadSafeProxy<T> Lock()
    {
      auto result = ThreadSafeProxy<T>(this);
      m_condition.notify_all();
      return result;
    }

  private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    T m_value;

    friend class ThreadSafeProxy<T>;
  };

  template <typename T>
  class ThreadSafeProxy
  {
  public:
    ThreadSafeProxy(ThreadSafe<T>* me)
      : m_me(me)
      , m_lock(m_me->m_mutex)
    {
    }

    T& operator=(T&& other)
      requires(std::is_move_constructible_v<T>)
    {
      m_me->m_value = std::move(other);
      return Get();
    }

    T& operator=(const T& other)
    {
      m_me->m_value = other;
      return Get();
    }

    T& Get() { return m_me->m_value; }
    const T& Get() const { return m_me->m_value; }

    decltype(auto) operator*()
    {
      if constexpr(has_dereference_v<T>)
      {
        return (*Get());
      }
      else
      {
        return (Get());
      }
    }

    decltype(auto) operator*() const
    {
      if constexpr(has_dereference_v<T>)
      {
        return (*Get());
      }
      else
      {
        return (Get());
      }
    }

    decltype(auto) operator->()
    {
      if constexpr(std::is_pointer_v<T>)
      {
        return Get();
      }
      else if constexpr(has_arrow_v<T>)
      {
        return Get().operator->();
      }
      else
      {
        return std::addressof(Get());
      }
    }

    decltype(auto) operator->() const
    {
      if constexpr(std::is_pointer_v<T>)
      {
        return Get();
      }
      else if constexpr(has_arrow_v<T>)
      {
        return Get().operator->();
      }
      else
      {
        return std::addressof(Get());
      }
    }

    template <typename Predicate>
    bool WaitUntil(std::chrono::steady_clock::time_point timePoint, Predicate&& predicate)
    {
      return m_me->m_condition.wait_until(m_lock, timePoint, std::forward<Predicate>(predicate));
    }

  private:
    ThreadSafe<T>* m_me;
    std::unique_lock<std::mutex> m_lock;
  };

} // namespace Bot
