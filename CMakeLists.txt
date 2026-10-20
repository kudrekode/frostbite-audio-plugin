cmake_minimum_required(VERSION 3.22)
project(Frostbite VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
option(FROSTBITE_BUILD_PLUGIN "Build JUCE plugin, renderer and host checks" ON)
include(CTest)

add_library(frostbite_dsp STATIC Source/dsp/Engine.cpp)
target_include_directories(frostbite_dsp PUBLIC Source)
set_target_properties(frostbite_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(frostbite_dsp PRIVATE /W4 /permissive-)
else()
  target_compile_options(frostbite_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(frostbite_dsp_tests Tests/DspTests.cpp)
target_link_libraries(frostbite_dsp_tests PRIVATE frostbite_dsp)
add_test(NAME DSP COMMAND frostbite_dsp_tests)

if(FROSTBITE_BUILD_PLUGIN)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
  else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 9.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
  endif()
  juce_add_plugin(Frostbite
    COMPANY_NAME "Frostbite Audio" BUNDLE_ID "audio.frostbite.effect"
    PLUGIN_MANUFACTURER_CODE Frba PLUGIN_CODE Frbt
    FORMATS VST3 Standalone PRODUCT_NAME "FROSTBITE"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Fx Reverb Delay Distortion
    DESCRIPTION "Frozen textures, damaged echoes, infinite distances")
  target_sources(Frostbite PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Parameters.cpp)
  target_include_directories(Frostbite PUBLIC Source)
  target_compile_definitions(Frostbite PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(Frostbite PRIVATE frostbite_dsp juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

  juce_add_console_app(frostbite_tests PRODUCT_NAME "Frostbite Integration Tests")
  target_sources(frostbite_tests PRIVATE Tests/PluginTests.cpp)
  target_link_libraries(frostbite_tests PRIVATE Frostbite juce::juce_audio_utils)
  add_test(NAME Plugin COMMAND frostbite_tests)
  juce_add_console_app(frostbite_render PRODUCT_NAME "Frostbite Renderer")
  target_sources(frostbite_render PRIVATE Tools/Render.cpp)
  target_link_libraries(frostbite_render PRIVATE Frostbite juce::juce_audio_utils)

  juce_add_console_app(frostbite_host_check PRODUCT_NAME "Frostbite VST3 Host Check")
  target_sources(frostbite_host_check PRIVATE Tests/HostCheck.cpp)
  target_compile_definitions(frostbite_host_check PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(frostbite_host_check PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
