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

# Define a dummy test executable with test discovery
add_executable(test_bot_dummy
  dummy_test.cpp
)
set_target_properties(test_bot_dummy PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

target_link_libraries(test_bot_dummy PRIVATE GTest::gtest_main)
set_property(TARGET test_bot_dummy PROPERTY CXX_STANDARD 23)

target_include_directories(test_bot_dummy PRIVATE ${CMAKE_SOURCE_DIR}/src)

gtest_discover_tests(test_bot_dummy)
