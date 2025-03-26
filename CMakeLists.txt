cmake_minimum_required(VERSION 3.10)
project(TcpServer)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(include)

# Source files
file(GLOB SOURCES "src/*.cpp")

# Create executable
add_executable(server ${SOURCES})

# On Linux, we need to link against pthread
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)