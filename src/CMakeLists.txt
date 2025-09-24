add_library(bot_lib STATIC
        Commands.h
        Dijkstra.h
        Dotenv.cpp
        Game.cpp
        Game.h
        GameCallbacks.h
        Map.cpp
        Map.h
        Offset.h
        Player.cpp
        Player.h
        Swoq.cpp
        Swoq.proto
        ThreadSafe.h
        TypeTraits.h
        Vector2d.h
)
target_link_libraries(bot_lib PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++)

add_executable(bot
        Main.cpp
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
        ${CMAKE_SOURCE_DIR}/src
)

set_property(TARGET bot PROPERTY CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
set_property(TARGET bot_lib PROPERTY CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET bot_lib LANGUAGE cpp)
protobuf_generate(TARGET bot_lib LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")
