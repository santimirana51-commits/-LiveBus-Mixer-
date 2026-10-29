cmake_minimum_required(VERSION 3.22)
project(LiveBusMixer VERSION 0.4.0 LANGUAGES C CXX)
enable_testing()

if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Clone JUCE into ./JUCE or pass -DJUCE_DIR=C:/path/to/JUCE")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

# RNNoise is pinned and vendored so released builds do not need network access.
add_library(RNNoise STATIC
    ThirdParty/rnnoise/src/denoise.c
    ThirdParty/rnnoise/src/rnn.c
    ThirdParty/rnnoise/src/pitch.c
    ThirdParty/rnnoise/src/kiss_fft.c
    ThirdParty/rnnoise/src/celt_lpc.c
    ThirdParty/rnnoise/src/nnet.c
    ThirdParty/rnnoise/src/nnet_default.c
    ThirdParty/rnnoise/src/parse_lpcnet_weights.c
    ThirdParty/rnnoise/src/rnnoise_data_little.c
    ThirdParty/rnnoise/src/rnnoise_tables.c)
target_include_directories(RNNoise PUBLIC ThirdParty/rnnoise/include)
target_include_directories(RNNoise PRIVATE ThirdParty/rnnoise/src)
target_compile_features(RNNoise PRIVATE c_std_99)
target_compile_definitions(RNNoise PRIVATE DISABLE_DEBUG_FLOAT=1)

# -------------------------------------------------------------
# 1. Main Application: LiveBusMixer
# -------------------------------------------------------------
juce_add_gui_app(LiveBusMixer PRODUCT_NAME "LiveBusMixer"
    COMPANY_NAME "LiveBus" BUNDLE_ID "com.livebus.mixer"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/LiveBusMixerIcon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/LiveBusMixerIcon.png"
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE)
juce_generate_juce_header(LiveBusMixer)

target_sources(LiveBusMixer PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/PluginHostProtocol.h
    Source/SharedMemoryChannel.h
    Source/SharedMemoryChannel.cpp
    Source/PluginHostClient.h
    Source/PluginHostClient.cpp
    Source/ChannelStripDSP.h
    Source/RoutingMatrix.h
    Source/DeviceSupervisor.h
    Source/DeviceSupervisor.cpp
    Source/SceneManager.h
    Source/SceneManager.cpp
    Source/PuzuDSPComponent.h
    Source/PuzuDSPComponent.cpp
    Source/MonitorGraphicEQComponent.h
    Source/MonitorGraphicEQComponent.cpp
    Source/MainOutputEQComponent.h
    Source/MainOutputEQComponent.cpp
    Source/LV1MixerComponent.h
    Source/LV1MixerComponent.cpp
    Source/AudioPlaybackModel.h
    Source/WaveformThumbnailCache.h
    Source/WaveformThumbnailCache.cpp
    Source/TestSignalGenerator.h
    Source/TestSignalGenerator.cpp
    Source/AudioFilePlayer.h
    Source/AudioFilePlayer.cpp
    Source/PlaybackComponent.h
    Source/PlaybackComponent.cpp
    Source/MidiEngine.h
    Source/MidiEngine.cpp
    Source/MidiMappingModel.h
    Source/MidiParameterRegistry.h
    Source/MidiManager.h
    Source/MidiManager.cpp
    Source/MidiLearnComponent.h
    Source/MidiLearnComponent.cpp
    Source/PresetZipManager.h
    Source/PresetZipManager.cpp
    Source/EightBandEQ.h
    Source/EightBandEQ.cpp
    Source/EightBandEQComponent.h
    Source/EightBandEQComponent.cpp
    Source/LevelMeterProcessor.h
    Source/LevelMeterProcessor.cpp
    Source/LevelMeterComponents.h
    Source/LevelMeterComponents.cpp
    Source/VocalDSPProcessor.h
    Source/VocalDSPProcessor.cpp
    Source/VocalDSPRackComponent.h
    Source/VocalDSPRackComponent.cpp
    Source/AudioDeviceService.h
    Source/AudioDeviceService.cpp
    Source/AudioSettingsComponent.h
    Source/AudioSettingsComponent.cpp
    Source/RecallFilter.h
    Source/PresetManager.h
    Source/PresetManager.cpp
    Source/EQPresetCatalog.h
    Source/EQPresetCatalog.cpp
    Source/RecallFilterComponent.h
    Source/RecallFilterComponent.cpp
    Source/PresetManagerComponent.h
    Source/PresetManagerComponent.cpp
    Source/CopyToolsComponent.h
    Source/CopyToolsComponent.cpp
    Source/ScenesConsoleComponent.h
    Source/ScenesConsoleComponent.cpp
    Source/OBSIntegrationService.h
    Source/OBSIntegrationService.cpp
    Source/OBSWebSocketClient.h
    Source/OBSWebSocketClient.cpp
    Source/OBSGuideComponent.h
    Source/OBSGuideComponent.cpp
    Source/OBSMixerComponent.h
    Source/OBSMixerComponent.cpp
    Source/PluginLibraryModel.h
    Source/VST3HostEngine.h
    Source/VST3HostEngine.cpp
    Source/PluginEditorWindow.h
    Source/PluginEditorWindow.cpp
    Source/PluginManageComponent.h
    Source/PluginManageComponent.cpp
    Source/PluginLibraryComponent.h
    Source/PluginLibraryComponent.cpp
    Source/EffectsConsoleComponent.h
    Source/EffectsConsoleComponent.cpp
    Source/PerformanceDiagnosticsModel.h
    Source/AudioPerformanceMonitor.h
    Source/AudioPerformanceMonitor.cpp
    Source/DiagnosticsLogManager.h
    Source/DiagnosticsLogManager.cpp
    Source/PerformanceDiagnosticsComponent.h
    Source/PerformanceDiagnosticsComponent.cpp
    Source/AutoDucker.h
    Source/AutoDucker.cpp
    Source/DuckingComponent.h
    Source/DuckingComponent.cpp
    Source/AudioSafetyProcessor.h
    Source/AudioSafetyProcessor.cpp
    Source/AudioSafetyComponent.h
    Source/AudioSafetyComponent.cpp
    Source/MacroEngine.h
    Source/MacroEngine.cpp
    Source/MacroControlComponent.h
    Source/MacroControlComponent.cpp
    Source/RoutingGraphRouting.h
    Source/RoutingGraphComponent.h
    Source/RoutingGraphComponent.cpp
    Source/RoutingMatrixComponent.h
    Source/RoutingMatrixComponent.cpp
    Source/YouTubeAudioCaptureService.h
    Source/YouTubeAudioCaptureService.cpp
    Source/YouTubePlayerComponent.h
    Source/YouTubePlayerComponent.cpp
    Source/RNNoiseNoiseSuppressor.h
    Source/RNNoiseNoiseSuppressor.cpp
    Source/AsyncAudioWorker.h
    Source/RealtimeAudioBlockQueue.h
    Source/RNNoiseVST3Plugin.h
    Source/RNNoiseVST3Plugin.cpp
    Source/StudioVocalPluginEditor.h
    Source/StudioVocalPluginEditor.cpp)

target_compile_features(LiveBusMixer PRIVATE cxx_std_17)
target_compile_definitions(LiveBusMixer PRIVATE
    JUCE_WEB_BROWSER=1 JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:LiveBusMixer,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")

set(ASIO_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/asiosdk/asio-main/common")
if(EXISTS "${ASIO_SDK_DIR}/iasiodrv.h")
    message(STATUS "ASIO SDK found at ${ASIO_SDK_DIR} - Enabling ASIO support")
    target_compile_definitions(LiveBusMixer PRIVATE JUCE_ASIO=1)
    target_include_directories(LiveBusMixer PRIVATE "${ASIO_SDK_DIR}")
else()
    message(STATUS "ASIO SDK not found - ASIO disabled")
endif()

target_link_libraries(LiveBusMixer PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_plugin_client
    juce::juce_gui_extra
    juce::juce_dsp
    RNNoise
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
if(WIN32)
    target_link_libraries(LiveBusMixer PRIVATE bcrypt)
endif()

# -------------------------------------------------------------
# 1a. Bundled RNNoise VST3 effect for the application plug-in host
# -------------------------------------------------------------
juce_add_plugin(LiveBusRNNoise
    COMPANY_NAME "LiveBus Audio"
    BUNDLE_ID "com.livebus.rnnoise"
    PLUGIN_MANUFACTURER_CODE LBus
    PLUGIN_CODE Rnns
    PRODUCT_NAME "LiveBus RNNoise"
    DESCRIPTION "Local RNNoise voice noise suppressor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3)
juce_generate_juce_header(LiveBusRNNoise)

target_sources(LiveBusRNNoise PRIVATE
    Source/RNNoiseVST3Plugin.h
    Source/RNNoiseVST3Plugin.cpp
    Source/AsyncAudioWorker.h
    Source/RealtimeAudioBlockQueue.h
    Source/RNNoiseNoiseSuppressor.h
    Source/RNNoiseNoiseSuppressor.cpp
    Source/VocalDSPProcessor.h
    Source/VocalDSPProcessor.cpp
    Source/EightBandEQ.h
    Source/EightBandEQ.cpp
    Source/VocalEnhancerDSP.h
    Source/AlesisVintageStudioDSP.h
    Source/VocalStudioPresets.h
    Source/StudioVocalPluginEditor.h
    Source/StudioVocalPluginEditor.cpp)
target_compile_features(LiveBusRNNoise PRIVATE cxx_std_17)
target_compile_definitions(LiveBusRNNoise PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0)
target_link_libraries(LiveBusRNNoise PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    RNNoise
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

get_target_property(LIVEBUS_RNNOISE_VST3_BUNDLE
    LiveBusRNNoise_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_command(TARGET LiveBusRNNoise_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:LiveBusMixer>/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIVEBUS_RNNOISE_VST3_BUNDLE}"
        "$<TARGET_FILE_DIR:LiveBusMixer>/Plugins/LiveBus RNNoise.vst3"
    VERBATIM)
add_dependencies(LiveBusMixer LiveBusRNNoise_VST3)

# -------------------------------------------------------------
# 1b. Bundled Alesis Vintage Studio VST3 effect (3630 & Microverb)
# -------------------------------------------------------------
juce_add_plugin(LiveBusAlesis
    COMPANY_NAME "LiveBus Audio"
    BUNDLE_ID "com.livebus.alesis"
    PLUGIN_MANUFACTURER_CODE LBus
    PLUGIN_CODE Ales
    PRODUCT_NAME "LiveBus Alesis"
    DESCRIPTION "Alesis 3630 Dual Compressor & Microverb Vintage Space"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3)
juce_generate_juce_header(LiveBusAlesis)

target_sources(LiveBusAlesis PRIVATE
    Source/AlesisStudioVST3Plugin.h
    Source/AlesisStudioVST3Plugin.cpp
    Source/AlesisStudioPluginEditor.h
    Source/AlesisStudioPluginEditor.cpp
    Source/AlesisVintageStudioDSP.h
    Source/VocalFXPro/PitchDetector.h
    Source/VocalFXPro/PitchCorrection.h)
target_compile_features(LiveBusAlesis PRIVATE cxx_std_17)
target_compile_definitions(LiveBusAlesis PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0)
target_link_libraries(LiveBusAlesis PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

get_target_property(LIVEBUS_ALESIS_VST3_BUNDLE
    LiveBusAlesis_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_command(TARGET LiveBusAlesis_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:LiveBusMixer>/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIVEBUS_ALESIS_VST3_BUNDLE}"
        "$<TARGET_FILE_DIR:LiveBusMixer>/Plugins/LiveBus Alesis.vst3"
    VERBATIM)
add_dependencies(LiveBusMixer LiveBusAlesis_VST3)

# -------------------------------------------------------------
# 1c. Bundled Vocal FX Pro VST3 effect
# -------------------------------------------------------------
juce_add_plugin(LiveBusVocalFXPro
    COMPANY_NAME "Vocal FX Pro"
    BUNDLE_ID "com.vocalfxpro.plugin"
    PLUGIN_MANUFACTURER_CODE Vfxp
    PLUGIN_CODE Vfx1
    PRODUCT_NAME "Vocal FX Pro"
    DESCRIPTION "Vocal FX Pro Real-Time Vocal Studio Suite"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3)
juce_generate_juce_header(LiveBusVocalFXPro)

target_sources(LiveBusVocalFXPro PRIVATE
    Source/VocalFXPro/PluginProcessor.h
    Source/VocalFXPro/PluginProcessor.cpp
    Source/VocalFXPro/PluginEditor.h
    Source/VocalFXPro/PluginEditor.cpp
    Source/VocalFXPro/PitchDetector.h
    Source/VocalFXPro/PitchCorrection.h)
target_compile_features(LiveBusVocalFXPro PRIVATE cxx_std_17)
target_compile_definitions(LiveBusVocalFXPro PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=0 JUCE_PLUGINHOST_AU=0)
target_link_libraries(LiveBusVocalFXPro PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

get_target_property(LIVEBUS_VOCALFXPRO_VST3_BUNDLE
    LiveBusVocalFXPro_VST3 JUCE_PLUGIN_ARTEFACT_FILE)
add_custom_command(TARGET LiveBusVocalFXPro_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:LiveBusMixer>/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${LIVEBUS_VOCALFXPRO_VST3_BUNDLE}"
        "$<TARGET_FILE_DIR:LiveBusMixer>/Plugins/Vocal FX Pro.vst3"
    VERBATIM)
add_dependencies(LiveBusMixer LiveBusVocalFXPro_VST3)



# -------------------------------------------------------------
# 2. Out-of-Process VST3 Helper: LiveBusPluginHost
# -------------------------------------------------------------
juce_add_gui_app(LiveBusPluginHost PRODUCT_NAME "LiveBusPluginHost"
    COMPANY_NAME "LiveBus" BUNDLE_ID "com.livebus.pluginhost")
juce_generate_juce_header(LiveBusPluginHost)

target_sources(LiveBusPluginHost PRIVATE
    Source/LiveBusPluginHostMain.cpp
    Source/PluginHostProtocol.h
    Source/SharedMemoryChannel.h
    Source/SharedMemoryChannel.cpp)

target_compile_features(LiveBusPluginHost PRIVATE cxx_std_17)
target_compile_definitions(LiveBusPluginHost PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:LiveBusPluginHost,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}")

target_link_libraries(LiveBusPluginHost PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# -------------------------------------------------------------
# 3. Automated Test Suite: LiveBusMixerTests
# -------------------------------------------------------------
juce_add_console_app(LiveBusMixerTests PRODUCT_NAME "LiveBus Mixer Tests")
juce_generate_juce_header(LiveBusMixerTests)

target_sources(LiveBusMixerTests PRIVATE
    Tests/LiveBusMixerTests.cpp
    Source/PluginHostProtocol.h
    Source/SharedMemoryChannel.h
    Source/SharedMemoryChannel.cpp
    Source/PluginHostClient.h
    Source/PluginHostClient.cpp
    Source/ChannelStripDSP.h
    Source/RealtimeAudioBlockQueue.h
    Source/AsyncAudioWorker.h
    Source/RNNoiseNoiseSuppressor.h
    Source/RNNoiseNoiseSuppressor.cpp
    Source/RoutingMatrix.h
    Source/RoutingGraphRouting.h
    Source/DeviceSupervisor.h
    Source/DeviceSupervisor.cpp
    Source/SceneManager.h
    Source/SceneManager.cpp
    Source/AudioPlaybackModel.h
    Source/WaveformThumbnailCache.h
    Source/WaveformThumbnailCache.cpp
    Source/TestSignalGenerator.h
    Source/TestSignalGenerator.cpp
    Source/AudioFilePlayer.h
    Source/AudioFilePlayer.cpp
    Source/MidiEngine.h
    Source/MidiEngine.cpp
    Source/MidiMappingModel.h
    Source/MidiParameterRegistry.h
    Source/MidiManager.h
    Source/MidiManager.cpp
    Source/PresetZipManager.h
    Source/PresetZipManager.cpp
    Source/EightBandEQ.h
    Source/EightBandEQ.cpp
    Source/LevelMeterProcessor.h
    Source/LevelMeterProcessor.cpp
    Source/VocalDSPProcessor.h
    Source/VocalDSPProcessor.cpp
    Source/AudioDeviceService.h
    Source/AudioDeviceService.cpp
    Source/RecallFilter.h
    Source/PresetManager.h
    Source/PresetManager.cpp
    Source/EQPresetCatalog.h
    Source/EQPresetCatalog.cpp
    Source/OBSIntegrationService.h
    Source/OBSIntegrationService.cpp
    Source/OBSWebSocketClient.h
    Source/OBSWebSocketClient.cpp
    Source/PluginLibraryModel.h
    Source/VST3HostEngine.h
    Source/VST3HostEngine.cpp
    Source/PerformanceDiagnosticsModel.h
    Source/AudioPerformanceMonitor.h
    Source/AudioPerformanceMonitor.cpp
    Source/DiagnosticsLogManager.h
    Source/DiagnosticsLogManager.cpp
    Source/AutoDucker.h
    Source/AutoDucker.cpp
    Source/AudioSafetyProcessor.h
    Source/AudioSafetyProcessor.cpp
    Source/MacroEngine.h
    Source/MacroEngine.cpp
    Source/YouTubeAudioCaptureService.h
    Source/YouTubeAudioCaptureService.cpp
    Source/YouTubePlayerComponent.h
    Source/YouTubePlayerComponent.cpp)

target_compile_features(LiveBusMixerTests PRIVATE cxx_std_17)
target_compile_definitions(LiveBusMixerTests PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0)

if(EXISTS "${ASIO_SDK_DIR}/iasiodrv.h")
    target_compile_definitions(LiveBusMixerTests PRIVATE JUCE_ASIO=1)
    target_include_directories(LiveBusMixerTests PRIVATE "${ASIO_SDK_DIR}")
endif()

target_link_libraries(LiveBusMixerTests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    RNNoise
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
if(WIN32)
    target_link_libraries(LiveBusMixerTests PRIVATE bcrypt)
endif()
add_test(NAME LiveBusMixerTests
    COMMAND $<TARGET_FILE:LiveBusMixerTests>)
set_tests_properties(LiveBusMixerTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
