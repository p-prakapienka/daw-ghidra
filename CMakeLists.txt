cmake_minimum_required(VERSION 3.16)
project(daw_ghidra)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

# Fetch the SoundTouch library from the provided repository
FetchContent_Declare(
    soundtouch
    GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
)
FetchContent_MakeAvailable(soundtouch)

add_executable(daw_ghidra
    main.cpp
    components/ADSR.cpp
)

# Add SoundTouch internal headers (for AAFilter) to our include path
# Note: AAFilter.h resides under soundtouch/source/SoundTouch which is not installed as a public header.
target_include_directories(daw_ghidra PRIVATE ${soundtouch_SOURCE_DIR}/source/SoundTouch)

# Link SoundTouch to the executable; the target provided by the library is typically 'SoundTouch'
# If build fails due to target name mismatch, we will adjust accordingly.
target_link_libraries(daw_ghidra PRIVATE SoundTouch)