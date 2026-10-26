cmake_minimum_required(VERSION 3.22)
project(LiveBusMixer VERSION 0.4.0 LANGUAGES CXX)
enable_testing()

if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Clone JUCE into ./JUCE or pass -DJUCE_DIR=C:/path/to/JUCE")
endif()
add_subdirectory("${JUCE_DIR}" JUCE)

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
    Source/YouTubePlayerComponent.cpp)

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
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
if(WIN32)
    target_link_libraries(LiveBusMixer PRIVATE bcrypt)
endif()

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
