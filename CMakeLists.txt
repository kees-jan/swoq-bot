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

add_library(bot_lib STATIC
        src/swoq.proto
        src/swoq.cpp
        src/dotenv.cpp
        src/Offset.h
        src/Vector2d.h
)
target_link_libraries(bot_lib PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++)

add_executable(bot
        src/main.cpp
)

# Apply warning flags only to bot
target_compile_options(bot_lib PUBLIC
        -Wall -Wextra -Wshadow -Wnon-virtual-dtor -pedantic
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wpedantic
        -Wsign-conversion
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wnull-dereference
        -Wuseless-cast
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
        -Werror
)

target_link_libraries(bot PUBLIC
        bot_lib
)

# Link Boost (header-only or compiled libs)
if (TARGET Boost::boost)
    target_link_libraries(bot PUBLIC Boost::boost)
else ()
    target_include_directories(bot PUBLIC ${Boost_INCLUDE_DIRS})
    if (Boost_LIBRARIES)
        target_link_libraries(bot PUBLIC ${Boost_LIBRARIES})
    endif ()
endif ()

target_include_directories(bot_lib PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
)

set_property(TARGET bot PROPERTY CXX_STANDARD 23)
set_property(TARGET bot_lib PROPERTY CXX_STANDARD 23)

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET bot_lib LANGUAGE cpp)
protobuf_generate(TARGET bot_lib LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")

# Enable testing
include(CTest)
if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
