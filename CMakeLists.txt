cmake_minimum_required(VERSION 3.10)
project(cpp-web-servers)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)

include_directories(${PROJECT_SOURCE_DIR}/src/multithread-server/threadpool)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system unit_test_framework)

add_executable(multi-server
    src/multithread-server/main.cpp
    src/multithread-server/threadpool/ThreadPool.cpp
)

target_link_libraries(multi-server
    Boost::system
    Threads::Threads
)

add_executable(single-server
    src/single-thread-server/main.cpp
)

target_link_libraries(single-server
    Boost::system
    Threads::Threads
)

add_executable(unit-tests
    test/unit-tests.cpp
)

target_link_libraries(unit-tests
    Boost::system
    Boost::unit_test_framework
    Threads::Threads
)