cmake_minimum_required(VERSION 3.22)

project(JerzyAudioQuantizer VERSION 0.1.0)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JerzyAudioQuantizer
    COMPANY_NAME "Jerzy"
    BUNDLE_ID "pl.jerzy.audioquantizer"
    PLUGIN_MANUFACTURER_CODE Jrzy
    PLUGIN_CODE JaQz
    FORMATS VST3 Standalone
    PRODUCT_NAME "JERZY AUDIO QUANTIZER"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(JerzyAudioQuantizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/TransientDetector.h
    Source/QuantizerEngine.h
    Source/WSOLAQuantizeWarper.h
)

target_compile_features(JerzyAudioQuantizer PRIVATE cxx_std_20)

target_compile_definitions(JerzyAudioQuantizer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(JerzyAudioQuantizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
