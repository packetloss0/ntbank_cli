cmake_minimum_required(VERSION 3.20)
project(ntbank_cli VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(APPLE)
    find_library(COREAUDIO_LIB CoreAudio REQUIRED)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox REQUIRED)
    set(PLATFORM_LIBS ${COREAUDIO_LIB} ${AUDIOTOOLBOX_LIB})
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_LIBS pthread dl m)
endif()

# Packer executable
add_executable(ntbank_cli
        src/main.cpp)

target_link_libraries(ntbank_cli PRIVATE ${PLATFORM_LIBS})

# Unpacker / Inspector executable
add_executable(ntbank_unpack
        src/unpack.cpp)
target_link_libraries(ntbank_unpack PRIVATE ${PLATFORM_LIBS})