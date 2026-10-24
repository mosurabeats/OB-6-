cmake_minimum_required(VERSION 3.16)
project(ob6clone VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Portable, dependency-free synth engine (desktop, MPC-side host, plugins).
add_library(ob6engine STATIC
  engine/src/params.cpp
  engine/src/voice.cpp
  engine/src/synth.cpp
)
target_include_directories(ob6engine PUBLIC engine/include)

# Offline renderer: patches -> MPC Keygroup programs for USB loading.
add_executable(ob6render
  tools/ob6render/main.cpp
  tools/ob6render/keygroup.cpp
  tools/ob6render/wav.cpp
  tools/ob6render/xpm.cpp
)
target_link_libraries(ob6render PRIVATE ob6engine)

include(CTest)
if(BUILD_TESTING)
  add_executable(ob6tests
    tests/test_main.cpp
    tools/ob6render/keygroup.cpp
    tools/ob6render/wav.cpp
    tools/ob6render/xpm.cpp
  )
  target_include_directories(ob6tests PRIVATE tools/ob6render)
  target_link_libraries(ob6tests PRIVATE ob6engine)
  add_test(NAME ob6tests COMMAND ob6tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
