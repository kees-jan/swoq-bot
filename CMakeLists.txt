cmake_minimum_required(VERSION 3.14)

project(bot)

enable_language(CXX)

find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using Protobuf ${Protobuf_VERSION}")

find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")

# Enable new Boost policy to avoid deprecation warning
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

# Add Boost
find_package(Boost REQUIRED)
message(STATUS "Found Boost: ${Boost_INCLUDE_DIRS}")

add_subdirectory(src)

# Enable testing
include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
