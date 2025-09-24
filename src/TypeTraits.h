#pragma once

#include <type_traits>
#include <utility>
#include <memory>

namespace Bot {

// has_dereference: detects presence of unary operator* (built-in or overloaded)

template <typename T, typename = void>
struct has_dereference : std::false_type {};

template <typename T>
struct has_dereference<T, std::void_t<decltype(*std::declval<T&>())>> : std::true_type {};

template <typename T>
inline constexpr bool has_dereference_v = has_dereference<T>::value;

// has_arrow: detects if T supports the -> operator syntax.
// Pointers inherently support ->, so specialize for them. For class/struct types we
// detect a user-defined operator->.

template <typename T, typename = void>
struct has_arrow : std::false_type {};

template <typename T>
struct has_arrow<T*, void> : std::true_type {};

template <typename T>
struct has_arrow<const T*, void> : std::true_type {};

template <typename T>
struct has_arrow<volatile T*, void> : std::true_type {};

template <typename T>
struct has_arrow<const volatile T*, void> : std::true_type {};

// User-defined operator-> detection
// Note: We use declval<T&>().operator->() so that proxy operator-> chains are also acceptable.
// We avoid "->" syntax directly to circumvent infinite recursion potential in exotic proxies.

template <typename T>
struct has_arrow<T, std::void_t<decltype(std::declval<T&>().operator->())>> : std::true_type {};

template <typename T>
inline constexpr bool has_arrow_v = has_arrow<T>::value;

#if defined(__cpp_concepts)
template <typename T>
concept Dereferencable = has_dereference_v<T>;

template <typename T>
concept Arrowable = has_arrow_v<T>;
#endif

} // namespace Bot

