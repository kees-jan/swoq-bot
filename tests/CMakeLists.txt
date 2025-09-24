# CMakeLists.txt

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_bot_dummy
  ThreadSafeTests.cpp
  TypeTraitsTests.cpp
        OffsetTests.cpp
  Vector2dTests.cpp
)
set_target_properties(test_bot_dummy PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

target_link_libraries(test_bot_dummy PRIVATE GTest::gtest_main bot_lib)

gtest_discover_tests(test_bot_dummy)
