cmake_minimum_required(VERSION 3.0)
project(highscoreserver C)

set(CMAKE_C_STANDARD 11)

#set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -march=native)  # march=native for best performance
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wno-long-long -Wno-unused-function -Wno-unused-variable -Wno-missing-braces")

include_directories(
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
file(GLOB SRCS
        "${PROJECT_SOURCE_DIR}/include/*"
        "${PROJECT_SOURCE_DIR}/src/*"
        )

add_executable(highscoreserver ${SRCS})

find_library(LIB_MHD microhttpd required)
target_link_libraries(highscoreserver ${LIB_MHD})

if(HIGHSCORE_SECRET_KEY)
    message(-DHIGHSCORE_SECRET_KEY=${HIGHSCORE_SECRET_KEY})
    add_definitions(-DHIGHSCORE_SECRET_KEY=${HIGHSCORE_SECRET_KEY})
else()
    message("No HIGHSCORE_SECRET_KEY")
endif()