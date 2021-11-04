cmake_minimum_required(VERSION 3.0)
project(highscoreserver C)

# Optional definitions set:
# OPTION_SOCKET         to use rhc socketsble

#
# NDEBUG                is used at some points, too
# RHC_LOG_DO_NOT_PRINT_TIME_FILE disable time and file in logs

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

# all platforms:
add_definitions(-DOPTION_SOCKET)

# check platform
if (MINGW)
    message("Platform: Windows MinGW")
    target_link_libraries(highscoreserver ws2_32)
else()
    message("Platform: Unix")
   
endif()

