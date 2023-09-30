cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/dependencies")

project(synclibcpp VERSION 1.0)
add_executable(syncserver 
  src/main.cpp
  src/diff.cpp)

find_package(Jsoncpp REQUIRED)

message(JSONCPP_INCLUDE_DIR="${JSONCPP_INCLUDE_DIR}")
message(JSONCPP_LIBRARIES="${JSONCPP_LIBRARIES}")

target_include_directories(
  syncserver 
  PRIVATE
  ${JSONCPP_INCLUDE_DIR}
)
target_link_libraries(syncserver PRIVATE jsoncpp)





