cmake_minimum_required(VERSION 3.20)
project(note_seq_re CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(note_seq_re STATIC
  shared/engine-core/EngineCoreLib/src/nodes/ClipTimeNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/CountInNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/MetronomeNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/MidiCombinerNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/MidiPlayerContent.cpp
  shared/engine-core/EngineCoreLib/src/nodes/MidiPlayerNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/MidiSchedulerNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/NoteRepeatNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/OneShotNoteNode.cpp
  shared/engine-core/EngineCoreLib/src/nodes/TransportFunctions.cpp
  shared/engine-core/EngineCoreLib/src/nodes/TransportNode.cpp
  shared/engine/EngineLib/src/layers/MidiTrackEventStreamReader.cpp
  shared/engine/EngineLib/src/layers/SyncClipSlots.cpp
  shared/engine/EngineLib/src/layers/recorders/MidiClipsRecorder.cpp
  shared/engine/EngineLib/src/layers/recorders/NotesRecorder.cpp
  shared/engine/EngineLib/src/nodes/LauncherNode.cpp
  shared/engine/EngineLib/src/nodes/StepArrangementNode.cpp
  shared/engine/EngineLib/src/nodes/TrackStepArrangementNode.cpp
)
target_include_directories(note_seq_re PUBLIC
  support
  shared/base/MathLib/include
  shared/engine-core/EngineCoreLib/include
  shared/engine/EngineLib/src)


option(NOTE_SEQ_RE_BUILD_TESTS "Build the note-seq-re tests" ON)

if (NOTE_SEQ_RE_BUILD_TESTS)
  if (NOT TARGET GTest::gtest_main)
    include(FetchContent)
    FetchContent_Declare(googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG v1.17.0)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif ()

  enable_testing()
  add_executable(note_seq_re_tests
    tests/LauncherNodeTest.cpp
    tests/TransportFunctionsTest.cpp
  )
  target_link_libraries(note_seq_re_tests PRIVATE note_seq_re GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(note_seq_re_tests)
endif ()
