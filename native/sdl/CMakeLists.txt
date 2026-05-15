cmake_minimum_required(VERSION 3.20)

project(BallgamesSnooker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(SDL2 QUIET sdl2)
  pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
endif()

add_executable(snooker_sdl src/main.cpp)

if(SDL2_FOUND AND SDL2_TTF_FOUND)
  set(SNOOKER_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
  set(SNOOKER_EXISTING_INCLUDE_DIRS)
  foreach(include_dir IN LISTS SNOOKER_INCLUDE_DIRS)
    if(EXISTS "${include_dir}")
      list(APPEND SNOOKER_EXISTING_INCLUDE_DIRS "${include_dir}")
    endif()
  endforeach()

  target_include_directories(snooker_sdl PRIVATE ${SNOOKER_EXISTING_INCLUDE_DIRS})
  target_link_directories(snooker_sdl PRIVATE ${SDL2_LIBRARY_DIRS} ${SDL2_TTF_LIBRARY_DIRS})
  target_link_libraries(snooker_sdl PRIVATE ${SDL2_TTF_LIBRARIES} ${SDL2_LIBRARIES})
  target_compile_options(snooker_sdl PRIVATE ${SDL2_CFLAGS_OTHER} ${SDL2_TTF_CFLAGS_OTHER})
else()
  find_package(SDL2 CONFIG REQUIRED)
  find_package(SDL2_ttf CONFIG REQUIRED)

  if(TARGET SDL2::SDL2main)
    target_link_libraries(snooker_sdl PRIVATE SDL2::SDL2main)
  endif()

  if(TARGET SDL2::SDL2)
    target_link_libraries(snooker_sdl PRIVATE SDL2::SDL2)
  elseif(TARGET SDL2::SDL2-static)
    target_link_libraries(snooker_sdl PRIVATE SDL2::SDL2-static)
  endif()

  if(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(snooker_sdl PRIVATE SDL2_ttf::SDL2_ttf)
  elseif(TARGET SDL2_ttf::SDL2_ttf-static)
    target_link_libraries(snooker_sdl PRIVATE SDL2_ttf::SDL2_ttf-static)
  endif()
endif()
