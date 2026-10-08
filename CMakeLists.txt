cmake_minimum_required(VERSION 3.22)

project(JerzyAudioQuantizer2 VERSION 1.1.0)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
)
FetchContent_MakeAvailable(JUCE)


juce_add_plugin(JerzyAudioQuantizer2
    COMPANY_NAME "Jerzy"
    BUNDLE_ID "pl.jerzy.audioquantizer2"
    PLUGIN_MANUFACTURER_CODE Jrzy
    PLUGIN_CODE JaQ2
    FORMATS VST3 Standalone
    PRODUCT_NAME "JERZY AUDIO QUANTIZER 2"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(JerzyAudioQuantizer2)

target_sources(JerzyAudioQuantizer2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/TransientDetector.h
    Source/QuantizerEngine.h
    Source/StudioSegmentEngine.h
    Source/DynamicsLeveler.h
)

target_compile_features(JerzyAudioQuantizer2 PRIVATE cxx_std_20)

target_compile_definitions(JerzyAudioQuantizer2
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(JerzyAudioQuantizer2
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if (MSVC)
    target_compile_options(JerzyAudioQuantizer2 PRIVATE /utf-8)
endif()
