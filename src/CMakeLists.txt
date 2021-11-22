cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ENABLE_EXPORTS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) 

set(CMAKE_BUILD_TYPE Debug) #Release\Debug

set(CurrentProject c5)
project(${CurrentProject} CXX)

find_package(Protobuf REQUIRED)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    transport_catalog.proto
    address.proto
    company.proto
    database.proto
    name.proto
    phone.proto
    rubric.proto
    sphere.proto
    url.proto
    working_time.proto
    graph.proto
    transport_router.proto
)

#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -fsanitize=leak")
#set(LSAN_OPTIONS "${LSAN_OPTIONS} verbosity=1:log_threads=1")

add_executable(${CurrentProject} ${PROTO_SRCS} ${PROTO_HDRS} main.cpp json.cpp)

target_link_libraries(${CurrentProject} ${Protobuf_LIBRARIES})
