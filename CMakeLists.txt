cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
project(synclibcpp VERSION 1.0)

# Dependencies
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/dependencies")

# GTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# JsonCPP
find_package(Jsoncpp REQUIRED)
find_package( Boost 1.40 COMPONENTS program_options REQUIRED )
message(JSONCPP_INCLUDE_DIR\ =\ ${JSONCPP_INCLUDE_DIR})

# Build Test
enable_testing()
add_executable(diff_test
  test/diff_test.cpp
  src/diff.cpp
)
target_link_libraries(
  diff_test
  GTest::gtest_main
  jsoncpp
)
include(GoogleTest)
gtest_discover_tests(diff_test)


# Build server
add_executable(syncserver 
  src/main.cpp
  src/diff.cpp
  src/statevar.cpp
)
target_link_libraries(syncserver 
  PRIVATE 
  jsoncpp 
  ${Boost_LIBRARIES} 
  pthread
)


include_directories(
    src
  ${JSONCPP_INCLUDE_DIR}
  ${Boost_INCLUDE_DIR}
)


