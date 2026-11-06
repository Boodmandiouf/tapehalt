cmake_minimum_required(VERSION 3.22)
project(TapeHalt VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(FORMATS VST3)
if(APPLE)
  list(APPEND FORMATS AU)
endif()

juce_add_plugin(TapeHalt
  COMPANY_NAME "Demo"
  PLUGIN_MANUFACTURER_CODE Demo
  PLUGIN_CODE Thlt
  FORMATS ${FORMATS}
  PRODUCT_NAME "TapeHalt"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TapeHalt PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(TapeHalt PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(TapeHalt PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
