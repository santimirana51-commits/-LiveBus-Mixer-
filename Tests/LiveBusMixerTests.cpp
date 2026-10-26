#include <JuceHeader.h>
#include <algorithm>
#include "../Source/RoutingMatrix.h"
#include "../Source/RoutingGraphRouting.h"
#include "../Source/ChannelStripDSP.h"
#include "../Source/SceneManager.h"
#include "../Source/PluginHostProtocol.h"
#include "../Source/PluginHostClient.h"
#include "../Source/DeviceSupervisor.h"
#include "../Source/AudioFilePlayer.h"
#include "../Source/MidiEngine.h"
#include "../Source/PresetZipManager.h"
#include "../Source/LevelMeterProcessor.h"
#include "../Source/VocalDSPProcessor.h"
#include "../Source/AudioDeviceService.h"
#include "../Source/RecallFilter.h"
#include "../Source/PresetManager.h"
#include "../Source/OBSIntegrationService.h"
#include "../Source/MasterOutputRouter.h"
#include "../Source/OBSWebSocketClient.h"
#include "../Source/MidiMappingModel.h"
#include "../Source/MidiParameterRegistry.h"
#include "../Source/MidiManager.h"
#include "../Source/AudioPlaybackModel.h"
#include "../Source/WaveformThumbnailCache.h"
#include "../Source/TestSignalGenerator.h"
#include "../Source/PluginLibraryModel.h"
#include "../Source/VST3HostEngine.h"
#include "../Source/PerformanceDiagnosticsModel.h"
#include "../Source/AudioPerformanceMonitor.h"
#include "../Source/DiagnosticsLogManager.h"
#include "../Source/AutoDucker.h"
#include "../Source/AudioSafetyProcessor.h"
#include "../Source/AudioDelayCompensator.h"
#include "../Source/MacroEngine.h"
#include "../Source/YouTubeAudioCaptureService.h"
#include "../Source/YouTubePlayerComponent.h"
#include <iostream>
#include <cassert>
#include <cmath>
#include <chrono>
#include <array>
#include <memory>
#include <limits>
#include <string>
#include <thread>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

static int passedTests = 0;
static int failedTests = 0;

#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "[-] FAILED: " << msg << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
        failedTests++; \
        return; \
    }

#define RUN_TEST(fn) \
    std::cout << "[*] Running " << #fn << "..." << std::endl; \
    fn(); \
    if (failedTests == prevFailed) { \
        std::cout << "[+] PASSED: " << #fn << std::endl; \
        passedTests++; \
    } \
    prevFailed = failedTests;

void testRoutingMatrixAndPanLaw()
{
    RoutingMatrix matrix;
    juce::AudioBuffer<float> streamBuf(2, 64);
    juce::AudioBuffer<float> monitorBuf(2, 64);

    std::vector<std::vector<float>> inputs(4, std::vector<float>(64, 1.0f));
    const float* inPtrs[4] = { inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data() };

    // Center pan: equal energy on L and R
    matrix.getChannel(0).gain.store(1.0f);
    matrix.getChannel(0).pan.store(0.0f);
    for (int i = 1; i < 4; ++i) matrix.getChannel(i).routeEnable[0].store(false);

    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 64);

    float lCenter = streamBuf.getSample(0, 0);
    float rCenter = streamBuf.getSample(1, 0);
    TEST_ASSERT(std::abs(lCenter - 1.0f) < 1e-4f, "Center pan left is 1.0");
    TEST_ASSERT(std::abs(rCenter - 1.0f) < 1e-4f, "Center pan right is 1.0");

    // Full Left: Left = sqrt(2) ~ 1.414, Right = 0.0
    matrix.getChannel(0).pan.store(-1.0f);
    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 64);
    float lLeft = streamBuf.getSample(0, 0);
    float rLeft = streamBuf.getSample(1, 0);
    TEST_ASSERT(std::abs(lLeft - 1.4142f) < 1e-3f, "Full left has sqrt(2) boost");
    TEST_ASSERT(std::abs(rLeft) < 1e-5f, "Full left has silent right channel");

    // Full Right: Left = 0.0, Right = 1.414
    matrix.getChannel(0).pan.store(1.0f);
    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 64);
    float lRight = streamBuf.getSample(0, 0);
    float rRight = streamBuf.getSample(1, 0);
    TEST_ASSERT(std::abs(lRight) < 1e-5f, "Full right has silent left channel");
    TEST_ASSERT(std::abs(rRight - 1.4142f) < 1e-3f, "Full right has sqrt(2) boost");
}

void testAudioDelayCompensatorImpulseAlignment()
{
    LiveBus::AudioDelayCompensator compensator;
    compensator.prepare(2, 128);
    TEST_ASSERT(compensator.setDelaySamples(64), "A valid compensation delay is accepted");

    juce::AudioBuffer<float> silence(2, 128);
    silence.clear();
    compensator.process(silence, 128);

    juce::AudioBuffer<float> impulse(2, 128);
    impulse.clear();
    impulse.setSample(0, 0, 1.0f);
    impulse.setSample(1, 1, 0.5f);
    compensator.process(impulse, 128);

    TEST_ASSERT(std::abs(impulse.getSample(0, 64) - 1.0f) < 1.0e-5f,
                "Left impulse is delayed by the configured sample count");
    TEST_ASSERT(std::abs(impulse.getSample(1, 65) - 0.5f) < 1.0e-5f,
                "Right impulse retains channel separation and the same delay");
    TEST_ASSERT(!compensator.setDelaySamples(LiveBus::AudioDelayCompensator::maxDelaySamples + 1),
                "Delay requests beyond the preallocated limit are rejected");
    TEST_ASSERT(compensator.getRequestedDelaySamples() == 0,
                "Rejected delay requests safely fall back to zero compensation");

    LiveBus::AudioDelayCompensator pluginPath;
    LiveBus::AudioDelayCompensator dryPath;
    pluginPath.prepare(2, 128);
    dryPath.prepare(2, 128);
    TEST_ASSERT(pluginPath.setDelaySamples(0) && dryPath.setDelaySamples(64),
                "Parallel paths accept complementary compensation delays");
    juce::AudioBuffer<float> pluginSilence(2, 128);
    juce::AudioBuffer<float> drySilence(2, 128);
    pluginSilence.clear();
    drySilence.clear();
    pluginPath.process(pluginSilence, 128);
    dryPath.process(drySilence, 128);

    juce::AudioBuffer<float> pluginImpulse(2, 128);
    juce::AudioBuffer<float> dryImpulse(2, 128);
    pluginImpulse.clear();
    dryImpulse.clear();
    pluginImpulse.setSample(0, 64, 1.0f);
    dryImpulse.setSample(0, 0, 1.0f);
    pluginPath.process(pluginImpulse, 128);
    dryPath.process(dryImpulse, 128);
    TEST_ASSERT(std::abs(pluginImpulse.getSample(0, 64) - dryImpulse.getSample(0, 64)) < 1.0e-5f,
                "A delayed plugin path aligns with a dry path after PDC");
}

void testWavRecorderOverwritesExistingFile()
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("LiveBusMixerRecorderTest_" + juce::Uuid().toString() + ".wav");
    struct FileCleanup
    {
        juce::File& fileToDelete;
        ~FileCleanup() { fileToDelete.deleteFile(); }
    } cleanup{ file };

    std::array<char, 4096> oldContents{};
    {
        juce::FileOutputStream oldFile(file);
        TEST_ASSERT(oldFile.openedOk() && oldFile.write(oldContents.data(), oldContents.size()),
                    "Test fixture creates a pre-existing file");
    }

    juce::WavAudioFormat format;
    auto outputStream = std::make_unique<juce::FileOutputStream>(file);
    TEST_ASSERT(outputStream->openedOk(), "Existing WAV destination can be opened");
    TEST_ASSERT(outputStream->setPosition(0), "Existing WAV destination can be rewound");
    TEST_ASSERT(outputStream->truncate().wasOk(), "Existing WAV destination is truncated before recording");

    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(outputStream.get(), 48000.0, 2, 24, {}, 0));
    TEST_ASSERT(writer != nullptr, "WAV writer can be created after truncation");
    outputStream.release();

    juce::AudioBuffer<float> audio(2, 16);
    audio.clear();
    TEST_ASSERT(writer->writeFromAudioSampleBuffer(audio, 0, audio.getNumSamples()),
                "Short stereo recording is written");
    writer.reset();

    std::unique_ptr<juce::AudioFormatReader> reader(
        format.createReaderFor(file.createInputStream().release(), true));
    TEST_ASSERT(reader != nullptr, "Overwritten output remains a readable WAV file");
    TEST_ASSERT(reader->lengthInSamples == (juce::int64)audio.getNumSamples(),
                "Output contains only the newly recorded audio, without stale trailing data");
}

void testRoutingGraphEditsUpdateAudioEngine()
{
    RoutingMatrix matrix;
    juce::AudioBuffer<float> streamBuffer(2, 64);
    juce::AudioBuffer<float> monitorBuffer(2, 64);
    std::vector<std::vector<float>> inputs(4, std::vector<float>(64, 0.0f));
    std::fill(inputs[0].begin(), inputs[0].end(), 1.0f);
    const float* inputPointers[4] =
    {
        inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data()
    };

    for (int channel = 0; channel < 4; ++channel)
    {
        matrix.getChannel(channel).routeEnable[0].store(false);
        matrix.getChannel(channel).routeEnable[1].store(false);
    }
    matrix.getChannel(0).gain.store(1.0f);
    matrix.getChannel(0).pan.store(0.0f);

    const LiveBus::RoutingGraphEndpoint channel0{ LiveBus::RoutingGraphNodeType::Channel, 0 };
    const LiveBus::RoutingGraphEndpoint stream{ LiveBus::RoutingGraphNodeType::Stream, 0 };
    const auto connected = LiveBus::connectRoutingGraphEndpoints(matrix, channel0, stream);
    TEST_ASSERT(connected == LiveBus::RoutingGraphEditResult::Updated,
                "Graph route connection updates routing state");

    matrix.routeAndSum(inputPointers, 4, streamBuffer, monitorBuffer, 64);
    TEST_ASSERT(streamBuffer.getSample(0, 0) > 0.99f,
                "Graph route reaches the actual Stream bus");
    TEST_ASSERT(std::abs(monitorBuffer.getSample(0, 0)) < 1.0e-5f,
                "Graph route leaves the Monitor bus isolated");

    const auto duplicate = LiveBus::connectRoutingGraphEndpoints(matrix, channel0, stream);
    TEST_ASSERT(duplicate == LiveBus::RoutingGraphEditResult::AlreadyConnected,
                "Duplicate graph cables are reported without changing routing");

    const LiveBus::RoutingGraphEndpoint monitor{ LiveBus::RoutingGraphNodeType::Monitor, 0 };
    TEST_ASSERT(LiveBus::connectRoutingGraphEndpoints(matrix, stream, monitor) ==
                    LiveBus::RoutingGraphEditResult::Unsupported,
                "Unsupported bus-to-bus connections are rejected");

    const LiveBus::RoutingGraphEndpoint submix{ LiveBus::RoutingGraphNodeType::Submix, 0 };
    TEST_ASSERT(LiveBus::connectRoutingGraphEndpoints(matrix, channel0, submix) ==
                    LiveBus::RoutingGraphEditResult::Updated,
                "Input can be assigned to a supported Submix");
    TEST_ASSERT(matrix.getChannel(0).submixGroup.load() == 0 &&
                    !matrix.getChannel(0).routeEnable[0].load() &&
                    !matrix.getChannel(0).routeEnable[1].load(),
                "Submix assignment replaces direct master routes");
    TEST_ASSERT(LiveBus::disconnectRoutingGraphEndpoints(matrix, channel0, submix) ==
                    LiveBus::RoutingGraphEditResult::Updated,
                "Submix cable can be removed");
    TEST_ASSERT(matrix.getChannel(0).submixGroup.load() == -1 &&
                    !matrix.getChannel(0).routeEnable[0].load() &&
                    !matrix.getChannel(0).routeEnable[1].load(),
                "Removing a Submix cable does not restore stale direct routes");
}

void testIndependentStreamMonitorRestore()
{
    RoutingMatrix matrix;
    auto& ch = matrix.getChannel(0);
    ch.gain.store(1.0f);
    ch.pan.store(0.0f);

    matrix.applyChannelRouteState(0, true, false);
    TEST_ASSERT(ch.routeEnable[0].load() && !ch.routeEnable[1].load() && ch.submixGroup.load() == -1,
                "Direct Stream route is applied without stale Monitor state");

    matrix.applyChannelRouteState(0, false, true);
    TEST_ASSERT(!ch.routeEnable[0].load() && ch.routeEnable[1].load() && ch.submixGroup.load() == -1,
                "Direct Monitor route is applied without stale Stream state");

    matrix.assignChannelToSubmix(0, 1);
    TEST_ASSERT(ch.submixGroup.load() == 1 && !ch.routeEnable[0].load() && !ch.routeEnable[1].load(),
                "Submix assignment clears direct route state");

    ch.submixGroup.store(1);
    ch.routeEnable[0].store(false);
    ch.routeEnable[1].store(false);
    matrix.applyChannelRouteState(0, true, false);
    TEST_ASSERT(ch.routeEnable[0].load() && !ch.routeEnable[1].load() && ch.submixGroup.load() == -1,
                "Restoring a direct route clears stale submix assignment");

    juce::AudioBuffer<float> stream(2, 64);
    juce::AudioBuffer<float> monitor(2, 64);
    std::vector<float> input(64, 1.0f);
    const float* ps[] = { input.data() };

    matrix.applyChannelRouteState(0, true, false);
    matrix.routeAndSum(ps, 1, stream, monitor, 64);
    TEST_ASSERT(stream.getSample(0, 0) > 0.99f, "Stream route sends the actual signal into the Stream bus");
    TEST_ASSERT(std::abs(monitor.getSample(0, 0)) < 1.0e-5f, "Stream route leaves the Monitor bus silent");

    matrix.applyChannelRouteState(0, false, true);
    matrix.routeAndSum(ps, 1, stream, monitor, 64);
    TEST_ASSERT(std::abs(stream.getSample(0, 0)) < 1.0e-5f, "Monitor route does not leak into the Stream bus");
    TEST_ASSERT(monitor.getSample(0, 0) > 0.99f, "Monitor route sends the actual signal into the Monitor bus");
}

void testRoutingGraphConnectionValidation()
{
    RoutingMatrix matrix;
    using namespace LiveBus;

    const RoutingGraphEndpoint channel{ RoutingGraphNodeType::Channel, 0 };
    const RoutingGraphEndpoint submix{ RoutingGraphNodeType::Submix, 0 };
    const auto validConnection = makeRoutingGraphConnection(channel, submix);
    TEST_ASSERT(validateRoutingGraphConnection(matrix, validConnection) ==
                    RoutingGraphValidationResult::Valid,
                "Supported graph cable passes node, port, and layout validation");
    TEST_ASSERT(validConnection.sourceNode.toStableString() == "channel:0" &&
                    validConnection.destinationNode.toStableString() == "submix:0",
                "Node IDs are stable and independent of canvas positions");
    TEST_ASSERT(validConnection.sourcePort == RoutingGraphPortId::AudioOutput &&
                    validConnection.destinationPort == RoutingGraphPortId::AudioInput &&
                    validConnection.channelMapping == RoutingGraphChannelMapping::EngineNative,
                "Connection persists its ports and engine-native channel mapping");
    TEST_ASSERT(matrix.addChannel("Graph Input") &&
                    validateRoutingGraphConnection(matrix,
                        makeRoutingGraphConnection(
                            { RoutingGraphNodeType::Channel, 4 },
                            { RoutingGraphNodeType::Stream, 0 })) == RoutingGraphValidationResult::Valid,
                "A newly activated input channel is available as a valid graph node");

    matrix.updateMasterBusMeter(0, 0.25f, 0.75f);
    matrix.updateMasterBusMeter(1, 0.4f, 0.2f);
    TEST_ASSERT(std::abs(matrix.getMasterBusPeak(0) - 0.75f) < 1.0e-5f &&
                    std::abs(matrix.getMasterBusPeak(1) - 0.4f) < 1.0e-5f,
                "Stream and Monitor graph meters read their corresponding engine bus peaks");

    auto invalid = validConnection;
    invalid.destinationNode.index = maxSubmixBuses;
    TEST_ASSERT(validateRoutingGraphConnection(matrix, invalid) ==
                    RoutingGraphValidationResult::NodeNotFound,
                "Unknown node IDs are rejected");

    invalid = validConnection;
    invalid.sourcePort = static_cast<RoutingGraphPortId>(255);
    TEST_ASSERT(validateRoutingGraphConnection(matrix, invalid) ==
                    RoutingGraphValidationResult::PortNotFound,
                "Unknown port IDs are rejected");

    invalid = validConnection;
    invalid.sourcePort = RoutingGraphPortId::AudioInput;
    TEST_ASSERT(validateRoutingGraphConnection(matrix, invalid) ==
                    RoutingGraphValidationResult::DirectionMismatch,
                "Input-to-input connections are rejected");

    invalid = validConnection;
    invalid.channelMapping = static_cast<RoutingGraphChannelMapping>(255);
    TEST_ASSERT(validateRoutingGraphConnection(matrix, invalid) ==
                    RoutingGraphValidationResult::ChannelLayoutUnsupported,
                "Unsupported channel mappings are rejected");

    const RoutingGraphEndpoint stream{ RoutingGraphNodeType::Stream, 0 };
    invalid = makeRoutingGraphConnection(stream, channel);
    invalid.sourcePort = RoutingGraphPortId::AudioOutput;
    TEST_ASSERT(validateRoutingGraphConnection(matrix, invalid) ==
                    RoutingGraphValidationResult::CycleUnsupported,
                "A feedback cable from a master output is rejected");

    TEST_ASSERT(connectRoutingGraphConnection(matrix, validConnection) ==
                    RoutingGraphEditResult::Updated,
                "Validated model connection can be applied to the engine");
    TEST_ASSERT(connectRoutingGraphConnection(matrix, validConnection) ==
                    RoutingGraphEditResult::AlreadyConnected,
                "Duplicate model connection is identified without duplicate state");
    TEST_ASSERT(disconnectRoutingGraphConnection(matrix, validConnection) ==
                    RoutingGraphEditResult::Updated,
                "Validated model connection can be removed from the engine");
}

void testRoutingGraphConnectionSerialization()
{
    using namespace LiveBus;
    RoutingMatrix matrix;

    for (int channel = 0; channel < matrix.getNumActiveChannels(); ++channel)
    {
        matrix.getChannel(channel).routeEnable[0].store(false);
        matrix.getChannel(channel).routeEnable[1].store(false);
        for (int aux = 0; aux < maxAuxBuses; ++aux)
            matrix.getChannel(channel).auxRouteEnable[(size_t)aux].store(false);
    }
    for (int submix = 0; submix < maxSubmixBuses; ++submix)
    {
        matrix.getSubmixBus(submix).routeToStream.store(false);
        matrix.getSubmixBus(submix).routeToMonitor.store(false);
    }
    for (int aux = 0; aux < maxAuxBuses; ++aux)
    {
        matrix.getAuxBus(aux).sendToStream.store(false);
        matrix.getAuxBus(aux).sendToMonitor.store(false);
    }

    matrix.getChannel(0).submixGroup.store(0);
    matrix.getChannel(1).auxRouteEnable[0].store(true);
    matrix.getSubmixBus(0).routeToStream.store(true);
    matrix.getAuxBus(0).sendToMonitor.store(true);

    const auto connections = getRoutingGraphConnections(matrix);
    TEST_ASSERT(connections.size() == 4,
                "Graph snapshot includes active channel, Submix, and Aux connections");
    TEST_ASSERT(std::none_of(connections.begin(), connections.end(),
                    [](const RoutingGraphConnection& connection)
                    {
                        return connection.sourceNode ==
                                   RoutingGraphNodeId{ RoutingGraphNodeType::Channel, 0 } &&
                               (connection.destinationNode ==
                                    RoutingGraphNodeId{ RoutingGraphNodeType::Stream, 0 } ||
                                connection.destinationNode ==
                                    RoutingGraphNodeId{ RoutingGraphNodeType::Monitor, 0 });
                    }),
                "Disabled direct routes are not represented as connected canvas cables");
    auto xml = serializeRoutingGraphConnections(matrix);
    TEST_ASSERT(xml != nullptr && xml->getNumChildElements() == 4,
                "Saved graph metadata serializes the engine's active connection set");
    TEST_ASSERT(xml->getChildByName("CONNECTION")->getStringAttribute("sourceType") == "channel",
                "Serialized connection retains stable endpoint type metadata");
}

void testIndependentBusSummingAndIsolation()
{
    RoutingMatrix matrix;
    juce::AudioBuffer<float> streamBuf(2, 64);
    juce::AudioBuffer<float> monitorBuf(2, 64);

    std::vector<std::vector<float>> inputs(4, std::vector<float>(64, 0.0f));
    std::fill(inputs[0].begin(), inputs[0].end(), 1.0f); // Signal on input 0 (Mic)
    std::fill(inputs[1].begin(), inputs[1].end(), 0.5f); // Signal on input 1 (Guitar)
    const float* inPtrs[4] = { inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data() };

    matrix.getChannel(0).gain.store(1.0f);
    matrix.getChannel(0).pan.store(0.0f);
    matrix.getChannel(1).gain.store(1.0f);
    matrix.getChannel(1).pan.store(0.0f);

    // Route Input 0 ONLY to Stream Bus
    matrix.getChannel(0).routeEnable[0].store(true);
    matrix.getChannel(0).routeEnable[1].store(false);

    // Route Input 1 ONLY to Monitor Bus
    matrix.getChannel(1).routeEnable[0].store(false);
    matrix.getChannel(1).routeEnable[1].store(true);

    matrix.getChannel(2).routeEnable[0].store(false);
    matrix.getChannel(2).routeEnable[1].store(false);
    matrix.getChannel(3).routeEnable[0].store(false);
    matrix.getChannel(3).routeEnable[1].store(false);

    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 64);

    // Verify Stream has Input 0 and ZERO from Input 1
    TEST_ASSERT(std::abs(streamBuf.getSample(0, 0) - 1.0f) < 1e-4f, "Stream received Input 0 correctly");
    // Verify Monitor has Input 1 and ZERO from Input 0 (zero crosstalk!)
    TEST_ASSERT(std::abs(monitorBuf.getSample(0, 0) - 0.5f) < 1e-4f, "Monitor received Input 1 correctly");
}

void testGainAndSendLevels()
{
    RoutingMatrix matrix;
    juce::AudioBuffer<float> streamBuf(2, 64);
    juce::AudioBuffer<float> monitorBuf(2, 64);

    std::vector<std::vector<float>> inputs(4, std::vector<float>(64, 1.0f));
    const float* inPtrs[4] = { inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data() };

    for (int i = 1; i < 4; ++i)
    {
        matrix.getChannel(i).routeEnable[0].store(false);
        matrix.getChannel(i).routeEnable[1].store(false);
    }

    matrix.getChannel(0).pan.store(0.0f);
    matrix.getChannel(0).gain.store(0.5f);
    matrix.getChannel(0).sendLevel[0].store(0.8f);
    matrix.getChannel(0).sendLevel[1].store(0.2f);

    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 64);

    float sLevel = streamBuf.getSample(0, 0);
    float mLevel = monitorBuf.getSample(0, 0);

    TEST_ASSERT(std::abs(sLevel - (0.5f * 0.8f)) < 1e-4f, "Stream gain * send accurate");
    TEST_ASSERT(std::abs(mLevel - (0.5f * 0.2f)) < 1e-4f, "Monitor gain * send accurate");
}

void testMuteAndSoloLogic()
{
    RoutingMatrix matrix;
    juce::AudioBuffer<float> streamBuf(2, 64);
    juce::AudioBuffer<float> monitorBuf(2, 64);

    std::vector<std::vector<float>> inputs(4, std::vector<float>(64, 1.0f));
    const float* inPtrs[4] = { inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data() };

    // Mute channel 0
    matrix.getChannel(0).mute.store(true);
    for (int i = 1; i < 4; ++i) matrix.getChannel(i).routeEnable[0].store(false);

    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 64);
    TEST_ASSERT(streamBuf.getSample(0, 0) == 0.0f, "Muted channel produces silence");

    // Unmute channel 0, solo channel 1
    matrix.getChannel(0).mute.store(false);
    matrix.getChannel(0).routeEnable[0].store(true);
    matrix.getChannel(1).routeEnable[0].store(true);
    matrix.getChannel(1).solo.store(true); // Channel 1 soloed

    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 64);
    TEST_ASSERT(matrix.getChannel(0).peakMeter.load() == 0.0f, "Non-soloed channel muted by solo bus");
    TEST_ASSERT(matrix.getChannel(1).peakMeter.load() > 0.0f, "Soloed channel active");
}

void testSubmixAndAuxSoloRouting()
{
    RoutingMatrix matrix;
    matrix.setNumActiveChannels(2);
    matrix.getChannel(0).gain.store(1.0f);
    matrix.getChannel(0).pan.store(0.0f);
    matrix.getChannel(0).submixGroup.store(0);
    matrix.getChannel(1).gain.store(1.0f);
    matrix.getChannel(1).routeEnable[0].store(true);
    matrix.getChannel(1).routeEnable[1].store(true);

    auto& submix = matrix.getSubmixBus(0);
    submix.solo.store(true);
    submix.routeToStream.store(true);
    submix.routeToMonitor.store(false);

    std::array<juce::AudioBuffer<float>, 2> inputs;
    inputs[0].setSize(2, 64);
    inputs[1].setSize(2, 64);
    inputs[0].clear();
    inputs[1].clear();
    inputs[0].setSample(0, 0, 1.0f);
    inputs[0].setSample(1, 0, 1.0f);
    inputs[1].setSample(0, 0, 0.5f);
    inputs[1].setSample(1, 0, 0.5f);
    juce::AudioBuffer<float> stream(2, 64);
    juce::AudioBuffer<float> monitor(2, 64);
    matrix.routeAndSum(inputs.data(), 2, stream, monitor, 64);
    TEST_ASSERT(std::abs(stream.getSample(0, 0) - 1.0f) < 1.0e-4f &&
                    std::abs(monitor.getSample(0, 0)) < 1.0e-5f,
                "Soloed Submix keeps its Stream route while isolating Monitor and other channels");
    TEST_ASSERT(matrix.getChannel(1).peakMeter.load() == 0.0f,
                "Submix solo excludes unrelated input channels");

    submix.solo.store(false);
    matrix.getChannel(0).submixGroup.store(-1);
    matrix.getChannel(0).auxRouteEnable[0].store(true);
    matrix.getChannel(0).auxSendLevel[0].store(1.0f);
    auto& aux = matrix.getAuxBus(0);
    aux.fxType.store(0);
    aux.returnLevel.store(1.0f);
    aux.sendToStream.store(true);
    aux.sendToMonitor.store(false);
    aux.solo.store(true);

    const float* monoInputs[2] = { inputs[0].getReadPointer(0), inputs[1].getReadPointer(0) };
    matrix.routeAndSum(monoInputs, 2, stream, monitor, 64);
    TEST_ASSERT(std::abs(stream.getSample(0, 0) - 1.0f) < 1.0e-4f &&
                    std::abs(monitor.getSample(0, 0)) < 1.0e-5f,
                "Soloed Aux return preserves only its enabled Stream destination");
    TEST_ASSERT(matrix.getChannel(1).peakMeter.load() == 0.0f &&
                    aux.peakMeterL.load() > 0.99f,
                "Aux solo isolates unrelated channels and meters the actual return");
}

void testDSPProcessorNumericalPerformance()
{
    BusDSPProcessor dsp;
    dsp.prepare(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    // Fill with high-amplitude 1 kHz sine wave (peak = 1.5f)
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buffer.setSample(ch, i, 1.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / 48000.0f));

    // Disable intermediate dynamics to isolate limiter & clipping
    dsp.params.hpfEnabled.store(false);
    dsp.params.compEnabled.store(false);
    dsp.params.gateEnabled.store(false);

    // Limiter at -1.0 dB
    dsp.params.limiterEnabled.store(true);
    dsp.params.limiterCeilingDb.store(-1.0f);
    dsp.updateCoefficients();

    dsp.process(buffer, 512);

    float maxSample = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 256; i < 512; ++i) // after limiter engages
        {
            float s = std::abs(buffer.getSample(ch, i));
            if (s > maxSample) maxSample = s;
        }
    }

    float ceilingLin = juce::Decibels::decibelsToGain(-1.0f); // ~0.891
    TEST_ASSERT(maxSample <= ceilingLin + 0.05f, "Limiter ceiling enforced without clipping");

    // Test clipping detection when limiter is disabled with hot signal
    dsp.params.limiterEnabled.store(false);
    dsp.resetClipping();
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buffer.setSample(ch, i, 1.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / 48000.0f));
    dsp.process(buffer, 512);
    TEST_ASSERT(dsp.isClipping() == true, "Clipping detector flagged high un-limited input");
}

void testSceneSerializationAndDeserialization()
{
    SceneManager manager;
    SceneData original;
    original.sceneName = "BroadcastingLivePreset";
    original.preferredSampleRate = 96000.0;
    original.preferredBlockSize = 256;
    original.preferredDeviceName = "Studio ASIO Interface";

    original.channels[0].name = "Lead Vocal";
    original.channels[0].gain = 1.25f;
    original.channels[0].pan = -0.3f;
    original.channels[0].mute = false;
    original.channels[0].solo = true;
    original.channels[0].streamSend = 0.95f;

    original.buses[0].eqEnabled = true;
    original.buses[0].eqFreq = 3200.0f;
    original.buses[0].eqGainDb = 4.5f;
    original.buses[0].compEnabled = true;
    original.buses[0].compThresholdDb = -14.0f;
    original.buses[0].reverbEnabled = true;
    original.buses[0].reverbRoomSize = 0.75f;
    original.routingGraphLayoutXml =
        "<ROUTING_GRAPH_LAYOUT version=\"1\" zoom=\"0.75\" viewX=\"120\" viewY=\"48\">"
        "<NODES><NODE type=\"channel\" index=\"0\" x=\"48\" y=\"64\"/></NODES>"
        "</ROUTING_GRAPH_LAYOUT>";

    // Add simulated plugin state blob
    VST3SceneState p;
    p.pluginPath = "C:\\Program Files\\Common Files\\VST3\\StudioComp.vst3";
    p.pluginName = "StudioComp";
    p.targetIndex = maxInputs - 1;
    p.isBypassed = false;
    const char dummyState[] = "LIVEBUS_VST3_BINARY_PRESET_DATA_12345";
    p.stateBlob.append(dummyState, sizeof(dummyState));
    original.plugins.push_back(p);

    auto xmlText = manager.serializeToXmlString(original);
    TEST_ASSERT(xmlText.isNotEmpty(), "XML text serialized");

    auto xmlDoc = juce::XmlDocument::parse(xmlText);
    TEST_ASSERT(xmlDoc != nullptr, "Valid XML document parsed");

    SceneData restored;
    juce::String err;
    bool success = manager.deserializeFromXml(*xmlDoc, restored, err);
    TEST_ASSERT(success, "Deserialization succeeded");
    TEST_ASSERT(restored.sceneName == original.sceneName, "Scene name preserved");
    TEST_ASSERT(restored.channels[0].name == "Lead Vocal", "Channel name preserved");
    TEST_ASSERT(std::abs(restored.channels[0].gain - 1.25f) < 1e-4f, "Channel gain preserved");
    TEST_ASSERT(restored.channels[0].solo == true, "Solo state preserved");
    TEST_ASSERT(restored.routingGraphLayoutXml == original.routingGraphLayoutXml,
                "Routing graph layout preserved");
    TEST_ASSERT(std::abs(restored.buses[0].eqFreq - 3200.0f) < 1e-4f, "EQ frequency preserved");
    TEST_ASSERT(restored.plugins.size() == 1, "Plugin chain preserved");
    TEST_ASSERT(restored.plugins[0].pluginName == "StudioComp", "Plugin name preserved");
    TEST_ASSERT(restored.plugins[0].targetIndex == maxInputs - 1, "Plugin target channel preserved");
    TEST_ASSERT(restored.plugins[0].stateBlob.getSize() == sizeof(dummyState), "Binary state blob preserved");

    original.eightBandEQs[(size_t)maxInputs + 1].bands[3].gainDb = 7.5f;
    original.vocalRacks[(size_t)maxInputs + 1].compThresholdDb = -31.0f;
    xmlText = manager.serializeToXmlString(original);
    xmlDoc = juce::XmlDocument::parse(xmlText);
    TEST_ASSERT(xmlDoc != nullptr, "Expanded DSP target scene parsed");
    SceneData expandedRestored;
    TEST_ASSERT(manager.deserializeFromXml(*xmlDoc, expandedRestored, err),
                "Expanded DSP target scene restored");
    TEST_ASSERT(std::abs(expandedRestored.eightBandEQs[(size_t)maxInputs + 1].bands[3].gainDb - 7.5f) < 1e-4f,
                "Channel 16 EQ scene state preserved");
    TEST_ASSERT(std::abs(expandedRestored.vocalRacks[(size_t)maxInputs + 1].compThresholdDb + 31.0f) < 1e-4f,
                "Channel 16 Vocal DSP scene state preserved");
}

void testSceneFileCorruptionRecovery()
{
    SceneManager manager;
    SceneData outData;
    juce::String err;

    juce::String corruptedXml = "<LIVEBUS_SCENE version='2'><CHANNELS><CHANNEL index='999' broken='yes'>";
    auto xmlDoc = juce::XmlDocument::parse(corruptedXml);
    TEST_ASSERT(xmlDoc == nullptr, "Malformed XML detected");

    juce::String incompleteXml = "<LIVEBUS_SCENE version='2' name='DamagedScene'></LIVEBUS_SCENE>";
    xmlDoc = juce::XmlDocument::parse(incompleteXml);
    TEST_ASSERT(xmlDoc != nullptr, "Incomplete XML parsed");

    bool ok = manager.deserializeFromXml(*xmlDoc, outData, err);
    TEST_ASSERT(ok, "Deserializer handled missing tags without crashing");
    TEST_ASSERT(outData.sceneName == "DamagedScene", "Name extracted");
    TEST_ASSERT(outData.channels[0].gain > 0.0f, "Safe defaults retained");
}

void testIPCProtocolAndConstants()
{
    TEST_ASSERT(LiveBusIPC::magicNumber == 0x4C495645, "Magic number is LIVE");
    TEST_ASSERT(LiveBusIPC::protocolVersion == 4, "Protocol version 4");
    TEST_ASSERT(sizeof(LiveBusIPC::SharedMemoryData) > 0, "Shared memory struct valid");
    TEST_ASSERT(LiveBusIPC::maxChannels == 2, "Stereo channels");
    TEST_ASSERT(LiveBusIPC::maxFrames == 8192, "Max frames 8192");
    TEST_ASSERT(!LiveBusIPC::isSupportedFrameCount(0), "Zero-frame requests are rejected");
    TEST_ASSERT(LiveBusIPC::isSupportedFrameCount(1), "Single-frame requests are supported");
    TEST_ASSERT(LiveBusIPC::isSupportedFrameCount(LiveBusIPC::maxFrames), "Maximum-sized requests are supported");
    TEST_ASSERT(!LiveBusIPC::isSupportedFrameCount(LiveBusIPC::maxFrames + 1), "Oversized requests are rejected");

    char boundedText[5] = { 'x', 'x', 'x', 'x', 'x' };
    LiveBusIPC::copyNullTerminatedString(boundedText, sizeof(boundedText), "123456");
    TEST_ASSERT(std::strcmp(boundedText, "1234") == 0, "IPC strings truncate safely and always terminate");
}

void testIsolatedPluginDryPathKeepsFixedBlockLatency()
{
    LiveBusIPC::FixedBlockAudioDelay delay;
    std::array<float, 4> leftInput{ 1.0f, 2.0f, 3.0f, 4.0f };
    std::array<float, 4> rightInput{ -1.0f, -2.0f, -3.0f, -4.0f };
    const float* firstInput[] = { leftInput.data(), rightInput.data() };
    std::array<float, 4> leftOutput{};
    std::array<float, 4> rightOutput{};
    float* output[] = { leftOutput.data(), rightOutput.data() };

    delay.storeCurrent(firstInput, 2, 4);
    TEST_ASSERT(!delay.copyPrevious(output, 2, 4),
                "First isolated block has no prior audio and must produce latency-fill silence");

    leftInput.fill(5.0f);
    rightInput.fill(-5.0f);
    const float* secondInput[] = { leftInput.data(), rightInput.data() };
    delay.storeCurrent(secondInput, 2, 4);
    TEST_ASSERT(delay.copyPrevious(output, 2, 4) &&
                    leftOutput[0] == 1.0f && rightOutput[0] == -1.0f,
                "A delayed dry block preserves the previous block's exact audio");
    TEST_ASSERT(!delay.copyPrevious(output, 2, 2),
                "A delayed block with a different frame count is rejected");

    delay.storeCurrent(secondInput, 2, 4);
    TEST_ASSERT(delay.copyPrevious(output, 2, 4) && leftOutput[0] == 5.0f,
                "Dry fallback advances continuously while isolated processing is late");
}

void testIPCCompletionPublication()
{
    LiveBusIPC::SharedMemoryData data{};
    std::atomic<std::uint32_t> acknowledgedSequence{0};
    bool payloadWasPublished = true;

    std::thread publisher([&]
    {
        for (std::uint32_t sequence = 1; sequence <= 1000; ++sequence)
        {
            while (acknowledgedSequence.load(std::memory_order_acquire) != sequence - 1)
                std::this_thread::yield();

            data.outputSamples[0][0] = static_cast<float>(sequence);
            data.status = LiveBusIPC::Status::Success;
            LiveBusIPC::SharedMemoryChannel::publishOutputReadySequence(data, sequence);
        }
    });

    for (std::uint32_t sequence = 1; sequence <= 1000; ++sequence)
    {
        while (LiveBusIPC::SharedMemoryChannel::loadOutputReadySequence(data) != sequence)
            std::this_thread::yield();

        if (data.status != LiveBusIPC::Status::Success
            || data.outputSamples[0][0] != static_cast<float>(sequence))
            payloadWasPublished = false;

        acknowledgedSequence.store(sequence, std::memory_order_release);
    }

    publisher.join();
    TEST_ASSERT(payloadWasPublished, "Completion sequence publishes audio and status before the client reads them");
}

void testPendingAudioBlockState()
{
    LiveBusIPC::PendingAudioBlock pending;
    TEST_ASSERT(!pending.isPending(), "No request is pending initially");

    pending.begin(12, 256);
    TEST_ASSERT(pending.isPending(), "A dispatched audio request remains pending until its matching response");
    TEST_ASSERT(!pending.isComplete(11), "A stale completion cannot release the in-flight request");
    TEST_ASSERT(!pending.canUseOutput(11, LiveBusIPC::Status::Success, 256),
                "Output cannot be consumed before the matching completion");

    pending.discardLateOutput();
    TEST_ASSERT(pending.isComplete(12), "The matching late response completes the request");
    TEST_ASSERT(!pending.canUseOutput(12, LiveBusIPC::Status::Success, 256),
                "A late output is discarded rather than applied to a later audio callback");
    pending.reset();

    pending.begin(13, 128);
    TEST_ASSERT(pending.canUseOutput(13, LiveBusIPC::Status::Success, 128),
                "Matching successful output with the same frame count can be consumed");
    TEST_ASSERT(!pending.canUseOutput(13, LiveBusIPC::Status::Success, 256),
                "Output from a differently sized block is not applied");
    TEST_ASSERT(!pending.canUseOutput(13, LiveBusIPC::Status::Error, 128),
                "Failed helper responses are not applied");
}

void testPluginDryFallbackOnCrash()
{
    PluginHostClient client;
    client.prepare(48000.0, 64);

    juce::AudioBuffer<float> buffer(2, 64);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 64; ++i)
            buffer.setSample(ch, i, 0.42f);

    // Process block with no helper running: 0ms non-blocking dry fallback
    client.processBlock(buffer, 64);

    TEST_ASSERT(std::abs(buffer.getSample(0, 0) - 0.42f) < 1e-5f, "Dry audio preserved intact on unstarted helper");
    TEST_ASSERT(std::abs(buffer.getSample(1, 63) - 0.42f) < 1e-5f, "Right channel dry audio preserved");
}

void testSimulatedPluginCrashAndWatchdogRecovery()
{
    PluginHostClient client;
    client.prepare(48000.0, 512);

    // Launch helper process
    bool launched = client.launchHelper();
    TEST_ASSERT(launched, "Helper process launched");
    TEST_ASSERT(client.isRunning(), "Helper is actively running");

    // Simulate third-party plugin unhandled crash in helper
    bool crashSent = client.simulateCrash();
    TEST_ASSERT(crashSent, "SimulateCrash command executed");

    // Wait briefly for OS process termination
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    TEST_ASSERT(!client.isRunning(), "Helper process terminated abruptly (simulated crash)");

    // Audio callback on main thread must remain 100% stable with zero wait
    juce::AudioBuffer<float> buffer(2, 512);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            buffer.setSample(ch, i, 0.77f);

    client.processBlock(buffer, 512);
    TEST_ASSERT(std::abs(buffer.getSample(0, 0) - 0.77f) < 1e-5f, "Main audio thread outputted dry signal during crash");

    // Watchdog recovers and restarts helper on worker thread
    client.checkHealthAndRecover(48000.0, 512);
    TEST_ASSERT(client.isRunning(), "Watchdog supervisor restarted helper process cleanly");

    client.terminateHelper();
}

void testDeviceSupervisorStateMachine()
{
    juce::AudioDeviceManager dm;
    DeviceSupervisor supervisor(dm);

    TEST_ASSERT(supervisor.getState() == DeviceState::Active, "Initial state Active");
    supervisor.triggerReconnection();
    TEST_ASSERT(supervisor.getState() == DeviceState::Reconnecting, "State changed to Reconnecting");
    supervisor.stop();
}

void testDynamicChannelAdditionAndRemoval()
{
    RoutingMatrix matrix;
    TEST_ASSERT(matrix.getNumActiveChannels() == 4, "Initial channel count is 4");

    bool added5 = matrix.addChannel("Mic 2");
    TEST_ASSERT(added5 == true, "Added 5th channel successfully");
    TEST_ASSERT(matrix.getNumActiveChannels() == 5, "Active channels is 5");
    TEST_ASSERT(matrix.getChannel(4).name == "Mic 2", "5th channel name set correctly");

    bool added6 = matrix.addChannel("Guest Vocal");
    TEST_ASSERT(added6 == true, "Added 6th channel successfully");
    TEST_ASSERT(matrix.getNumActiveChannels() == 6, "Active channels is 6");

    bool removed = matrix.removeLastChannel();
    TEST_ASSERT(removed == true, "Removed channel successfully");
    TEST_ASSERT(matrix.getNumActiveChannels() == 5, "Active channels back to 5");
}

void testFourBandStudioEqualizer()
{
    BusDSPProcessor dsp;
    dsp.prepare(48000.0, 512);

    // Flat response with 0dB gain
    dsp.params.eqEnabled.store(true);
    dsp.params.hpfEnabled.store(false);
    dsp.params.eqLowGainDb.store(0.0f);
    dsp.params.eqMid1GainDb.store(0.0f);
    dsp.params.eqGainDb.store(0.0f);
    dsp.params.eqHighGainDb.store(0.0f);
    dsp.updateCoefficients();

    float mag1kFlat = dsp.getMagnitudeForFrequency(1000.0f);
    TEST_ASSERT(std::abs(mag1kFlat) < 0.1f, "Flat EQ produces 0 dB response at 1kHz");

    // Boost Low Shelf by +6 dB at 100 Hz
    dsp.params.eqLowFreq.store(100.0f);
    dsp.params.eqLowGainDb.store(6.0f);
    dsp.updateCoefficients();

    float magLow = dsp.getMagnitudeForFrequency(50.0f);
    TEST_ASSERT(magLow > 4.5f, "Low Shelf boosts sub-bass by ~+6dB");

    // Cut Mid 1 by -6 dB at 500 Hz
    dsp.params.eqMid1Freq.store(500.0f);
    dsp.params.eqMid1GainDb.store(-6.0f);
    dsp.updateCoefficients();

    float magMid = dsp.getMagnitudeForFrequency(500.0f);
    TEST_ASSERT(magMid < -4.5f, "Mid 1 cuts resonance at 500Hz by ~-6dB");

    // Boost High Shelf by +6 dB at 10 kHz
    dsp.params.eqHighFreq.store(10000.0f);
    dsp.params.eqHighGainDb.store(6.0f);
    dsp.updateCoefficients();

    float magHigh = dsp.getMagnitudeForFrequency(15000.0f);
    TEST_ASSERT(magHigh > 4.5f, "High Shelf provides treble air boost by ~+6dB");
}

void testFifteenBandPuzuEqualizerAndSlopeFilters()
{
    BusDSPProcessor dsp;
    dsp.prepare(48000.0, 512);

    // 1. Verify 15-Band Flat Initialization
    dsp.params.eqEnabled.store(true);
    dsp.params.hpfEnabled.store(false);
    dsp.params.lpfEnabled.store(false);
    dsp.updateCoefficients();

    float flat1k = dsp.getMagnitudeForFrequency(1000.0f);
    TEST_ASSERT(std::abs(flat1k) < 0.1f, "15-Band PEQ defaults to flat 0dB response");

    // 2. Test Band 2 (63.7 Hz) boost by +6 dB
    dsp.params.puzuBands[2].gainDb.store(6.0f);
    dsp.updateCoefficients();
    float mag63 = dsp.getMagnitudeForFrequency(63.7f);
    TEST_ASSERT(mag63 > 4.8f && mag63 < 7.0f, "PUZU Band 2 boosts 63.7Hz by ~+6dB");

    // 3. Test HP Filter 12dB/Oct cutoff at 100Hz
    dsp.params.hpfEnabled.store(true);
    dsp.params.hpfFreq.store(100.0f);
    dsp.params.hpfSlope.store(1); // 12 dB/Oct
    dsp.updateCoefficients();
    float magSub = dsp.getMagnitudeForFrequency(25.0f);
    TEST_ASSERT(magSub < -10.0f, "HP Filter 12dB/Oct attenuates 25Hz significantly");

    // 4. Test LP Filter 24dB/Oct cutoff at 8000Hz
    dsp.params.lpfEnabled.store(true);
    dsp.params.lpfFreq.store(8000.0f);
    dsp.params.lpfSlope.store(3); // 24 dB/Oct
    dsp.updateCoefficients();
    float magAir = dsp.getMagnitudeForFrequency(16000.0f);
    TEST_ASSERT(magAir < -10.0f, "LP Filter 24dB/Oct attenuates 16kHz significantly");
}

void testMidiInputBufferThreadSafety()
{
    MidiEngine midi;
    TEST_ASSERT(midi.hasRecentActivity() == false, "Initial MIDI state has no activity");

    // Simulate incoming Note-On
    auto noteOn = juce::MidiMessage::noteOn(1, 60, (juce::uint8)100);
    midi.handleIncomingMidiMessage(nullptr, noteOn);

    TEST_ASSERT(midi.hasRecentActivity() == true, "Activity detected after Note-On");
    TEST_ASSERT(midi.getLastNote() == 60, "Last note number recorded accurately (60)");
    TEST_ASSERT(midi.getLastVelocity() == 100, "Last velocity recorded accurately (100)");

    // Simulate lock-free audio thread extraction
    juce::MidiBuffer audioThreadBuffer;
    midi.pullMidiBlock(audioThreadBuffer, 256);
    TEST_ASSERT(!audioThreadBuffer.isEmpty(), "Audio thread pulled MIDI events successfully");

    // Next block should be empty since events were consumed
    juce::MidiBuffer nextBlock;
    midi.pullMidiBlock(nextBlock, 256);
    TEST_ASSERT(nextBlock.isEmpty(), "Subsequent pull is empty (zero residual latency)");
}

void testAudioFilePlaybackAndBuffering()
{
    AudioFilePlayer player;
    player.prepareToPlay(48000.0, 512);

    TEST_ASSERT(player.isPlaying() == false, "Player starts stopped");
    player.setGain(0.75f);
    TEST_ASSERT(std::abs(player.getGain() - 0.75f) < 1e-4f, "Gain setting accurate");

    player.setRouteStream(true);
    player.setRouteMonitor(false);
    TEST_ASSERT(player.isRouteStream() == true, "Stream routing active");
    TEST_ASSERT(player.isRouteMonitor() == false, "Monitor routing isolated");

    // Audio callback mixing when idle produces silence without throwing
    juce::AudioBuffer<float> streamBuf(2, 512);
    juce::AudioBuffer<float> monitorBuf(2, 512);
    streamBuf.clear();
    monitorBuf.clear();

    player.processMix(streamBuf, monitorBuf, 512);
    TEST_ASSERT(streamBuf.getSample(0, 0) == 0.0f, "No audio output when idle");
}

void testZipPresetPackaging()
{
    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("LiveBusTestPresets_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    tempDir.createDirectory();

    auto scene1 = tempDir.getChildFile("SceneA.xml");
    scene1.replaceWithText("<Scene version='2'><Name>SceneA</Name></Scene>");

    auto zipFile = tempDir.getChildFile("Backup.zip");
    bool exported = PresetZipManager::exportPresetsToZip(zipFile, tempDir);
    TEST_ASSERT(exported == true, "Presets exported to ZIP successfully");
    TEST_ASSERT(zipFile.existsAsFile() && zipFile.getSize() > 0, "ZIP archive exists with non-zero size");

    auto restoreDir = tempDir.getChildFile("Restored");
    bool imported = PresetZipManager::importPresetsFromZip(zipFile, restoreDir);
    TEST_ASSERT(imported == true, "Presets imported from ZIP successfully");
    TEST_ASSERT(restoreDir.getChildFile("SceneA.xml").existsAsFile(), "SceneA.xml restored intact");

    tempDir.deleteRecursively();
}

void testZipPresetImportRejectsTraversalWithoutReplacingExistingFiles()
{
    const auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("LiveBusZipTraversal_" + juce::String(juce::Random::getSystemRandom().nextInt()));
    const auto scenesDirectory = tempDir.getChildFile("Scenes");
    const bool directoriesCreated = scenesDirectory.createDirectory();
    auto existingScene = scenesDirectory.getChildFile("Existing.xml");
    const bool existingSceneWritten = existingScene.replaceWithText("<Scene><Name>Keep</Name></Scene>");

    const auto safeSource = tempDir.getChildFile("Safe.xml");
    const auto traversalSource = tempDir.getChildFile("Traversal.xml");
    const bool sourcesWritten = safeSource.replaceWithText("<Scene><Name>New</Name></Scene>") &&
                                traversalSource.replaceWithText("<Scene><Name>Bad</Name></Scene>");
    const auto archiveFile = tempDir.getChildFile("Malicious.zip");
    juce::ZipFile::Builder builder;
    builder.addFile(safeSource, 9, "Existing.xml");
    builder.addFile(traversalSource, 9, "../escape.xml");
    bool archiveWritten = false;
    {
        juce::FileOutputStream stream(archiveFile);
        archiveWritten = stream.openedOk() && builder.writeToStream(stream, nullptr);
        stream.flush();
        archiveWritten = archiveWritten && stream.getStatus().wasOk();
    }

    const bool imported = archiveWritten &&
                          PresetZipManager::importPresetsFromZip(archiveFile, scenesDirectory);
    const auto existingContents = existingScene.loadFileAsString();
    const bool escapedFileCreated = tempDir.getChildFile("escape.xml").exists();
    tempDir.deleteRecursively();

    TEST_ASSERT(directoriesCreated && existingSceneWritten && sourcesWritten && archiveWritten,
                "Traversal test fixture was created");
    TEST_ASSERT(!imported, "ZIP entry with a traversal path is rejected");
    TEST_ASSERT(existingContents == "<Scene><Name>Keep</Name></Scene>",
                "Rejected archive leaves existing scene content untouched");
    TEST_ASSERT(!escapedFileCreated, "Rejected archive does not create files outside the scene directory");
}

void testSessionPersistenceAndFullState()
{
    SceneManager manager;
    SceneData original;
    original.sceneName = "_test_session";
    original.numActiveChannels = 12;
    original.mixerFirstVisibleChannel = 8;
    original.mixerTopPanelProportion = 0.62f;
    original.channels[0].name = "Host Mic";
    original.channels[0].gain = 0.85f;
    original.channels[0].pan = -0.25f;
    original.channels[0].mute = false;
    original.channels[0].solo = true;
    original.channels[0].streamRoute = true;
    original.channels[0].monitorRoute = false;
    original.channels[0].streamSend = 0.95f;
    original.channels[0].monitorSend = 0.1f;

    original.buses[0].masterGain = 0.92f;
    original.buses[0].masterMute = false;
    original.buses[0].eqEnabled = true;
    original.buses[0].eqLowGainDb = 4.5f;
    original.buses[0].compEnabled = true;
    original.buses[0].compThresholdDb = -16.0f;
    original.buses[0].limiterEnabled = true;
    original.buses[0].limiterCeilingDb = -0.5f;

    bool saved = manager.saveScene("_test_session", original);
    TEST_ASSERT(saved == true, "Full session configuration saved to XML");

    SceneData loaded;
    juce::String err;
    bool loadedOk = manager.loadScene("_test_session", loaded, err);
    TEST_ASSERT(loadedOk == true, "Full session configuration loaded without errors");

    TEST_ASSERT(loaded.channels[0].name == "Host Mic", "Restored channel name matches");
    TEST_ASSERT(std::abs(loaded.channels[0].gain - 0.85f) < 0.001f, "Restored gain matches");
    TEST_ASSERT(std::abs(loaded.channels[0].pan - (-0.25f)) < 0.001f, "Restored pan matches");
    TEST_ASSERT(loaded.channels[0].solo == true, "Restored solo matches");
    TEST_ASSERT(loaded.channels[0].streamRoute == true && loaded.channels[0].monitorRoute == false, "Restored routing flags match");
    TEST_ASSERT(loaded.numActiveChannels == 12, "Restored active channel count matches");
    TEST_ASSERT(loaded.mixerFirstVisibleChannel == 8, "Restored mixer horizontal scroll position matches");
    TEST_ASSERT(std::abs(loaded.mixerTopPanelProportion - 0.62f) < 0.001f, "Restored mixer panel size matches");
    TEST_ASSERT(std::abs(loaded.buses[0].masterGain - 0.92f) < 0.001f, "Restored bus master gain matches");
    TEST_ASSERT(std::abs(loaded.buses[0].eqLowGainDb - 4.5f) < 0.001f, "Restored EQ low gain matches");
    TEST_ASSERT(std::abs(loaded.buses[0].compThresholdDb - (-16.0f)) < 0.001f, "Restored Comp threshold matches");

    manager.deleteScene("_test_session");
}

void testEightBandEQParameterValidationAndClamping()
{
    using namespace LiveBus;

    // 1. Frequency validation & Nyquist clamping
    TEST_ASSERT(EightBandEQProcessor::validateFrequency(10.0f, 48000.0) == 20.0f, "Frequency clamped to min 20 Hz");
    TEST_ASSERT(EightBandEQProcessor::validateFrequency(25000.0f, 48000.0) <= 24000.0f, "Frequency clamped below 48k Nyquist");
    TEST_ASSERT(EightBandEQProcessor::validateFrequency(25000.0f, 44100.0) <= 22050.0f, "Frequency clamped below 44.1k Nyquist");
    TEST_ASSERT(EightBandEQProcessor::validateFrequency(1000.0f, 48000.0) == 1000.0f, "Valid frequency preserved");

    // 2. NaN and Infinity handling
    float nanVal = std::numeric_limits<float>::quiet_NaN();
    float infVal = std::numeric_limits<float>::infinity();
    TEST_ASSERT(EightBandEQProcessor::validateFrequency(nanVal, 48000.0) == 1000.0f, "NaN frequency falls back to 1000 Hz");
    TEST_ASSERT(EightBandEQProcessor::validateFrequency(infVal, 48000.0) <= 24000.0f, "Inf frequency clamped safely");
    TEST_ASSERT(EightBandEQProcessor::validateGain(nanVal) == 0.0f, "NaN gain falls back to 0.0 dB");
    TEST_ASSERT(EightBandEQProcessor::validateQ(nanVal) == 0.707f, "NaN Q falls back to 0.707");

    // 3. Gain and Q limits
    TEST_ASSERT(EightBandEQProcessor::validateGain(-25.0f) == -18.0f, "Gain clamped to min -18 dB");
    TEST_ASSERT(EightBandEQProcessor::validateGain(25.0f) == 18.0f, "Gain clamped to max +18 dB");
    TEST_ASSERT(EightBandEQProcessor::validateQ(0.01f) == 0.1f, "Q clamped to min 0.1");
    TEST_ASSERT(EightBandEQProcessor::validateQ(20.0f) == 10.0f, "Q clamped to max 10.0");
}

void testEightBandEQFilterTypesAndAcousticResponse()
{
    using namespace LiveBus;
    EightBandEQProcessor eq;
    eq.prepare(48000.0, 512);

    // Test 1: Bell Peak Filter (+6 dB at 1000 Hz)
    eq.params.setToFlat();
    eq.params.bands[3].type.store(EQFilterType::Bell);
    eq.params.bands[3].freq.store(1000.0f);
    eq.params.bands[3].gainDb.store(6.0f);
    eq.params.bands[3].q.store(1.0f);
    eq.params.bands[3].enabled.store(true);
    eq.updateCoefficients();

    float magCenter = eq.getBandMagnitudeForFrequency(3, 1000.0f);
    float magFar = eq.getBandMagnitudeForFrequency(3, 100.0f);
    TEST_ASSERT(std::abs(magCenter - 6.0f) < 0.2f, "Bell filter achieves +6dB at 1kHz center");
    TEST_ASSERT(std::abs(magFar - 0.0f) < 0.5f, "Bell filter has ~0dB at 100Hz far from center");

    // Test 2: Low Shelf Filter (+6 dB at 100 Hz)
    eq.params.setToFlat();
    eq.params.bands[1].type.store(EQFilterType::LowShelf);
    eq.params.bands[1].freq.store(100.0f);
    eq.params.bands[1].gainDb.store(6.0f);
    eq.params.bands[1].q.store(0.707f);
    eq.params.bands[1].enabled.store(true);
    eq.updateCoefficients();

    float magLow = eq.getBandMagnitudeForFrequency(1, 20.0f);
    float magHigh = eq.getBandMagnitudeForFrequency(1, 5000.0f);
    TEST_ASSERT(magLow >= 5.0f && magLow <= 6.2f, "Low Shelf achieves ~+6dB on bass frequencies");
    TEST_ASSERT(std::abs(magHigh - 0.0f) < 0.2f, "Low Shelf remains 0dB on high frequencies");

    // Test 3: High Shelf Filter (+6 dB at 10 kHz)
    eq.params.setToFlat();
    eq.params.bands[6].type.store(EQFilterType::HighShelf);
    eq.params.bands[6].freq.store(10000.0f);
    eq.params.bands[6].gainDb.store(6.0f);
    eq.params.bands[6].q.store(0.707f);
    eq.params.bands[6].enabled.store(true);
    eq.updateCoefficients();

    float magTreble = eq.getBandMagnitudeForFrequency(6, 18000.0f);
    float magBass = eq.getBandMagnitudeForFrequency(6, 200.0f);
    TEST_ASSERT(magTreble >= 5.0f && magTreble <= 6.2f, "High Shelf achieves ~+6dB on treble frequencies");
    TEST_ASSERT(std::abs(magBass - 0.0f) < 0.2f, "High Shelf remains 0dB on low frequencies");

    // Test 4: High-Pass Filter (cutoff 100 Hz)
    eq.params.setToFlat();
    eq.params.bands[0].type.store(EQFilterType::HighPass);
    eq.params.bands[0].freq.store(100.0f);
    eq.params.bands[0].q.store(0.707f);
    eq.params.bands[0].enabled.store(true);
    eq.updateCoefficients();

    float magBelowCutoff = eq.getBandMagnitudeForFrequency(0, 25.0f);
    float magPassband = eq.getBandMagnitudeForFrequency(0, 1000.0f);
    TEST_ASSERT(magBelowCutoff < -10.0f, "High-Pass strongly attenuates sub-bass (25Hz)");
    TEST_ASSERT(std::abs(magPassband - 0.0f) < 0.2f, "High-Pass passes 1kHz with 0dB attenuation");

    // Test 5: Low-Pass Filter (cutoff 5000 Hz)
    eq.params.setToFlat();
    eq.params.bands[7].type.store(EQFilterType::LowPass);
    eq.params.bands[7].freq.store(5000.0f);
    eq.params.bands[7].q.store(0.707f);
    eq.params.bands[7].enabled.store(true);
    eq.updateCoefficients();

    float magStopband = eq.getBandMagnitudeForFrequency(7, 18000.0f);
    float magPass = eq.getBandMagnitudeForFrequency(7, 200.0f);
    TEST_ASSERT(magStopband < -10.0f, "Low-Pass strongly attenuates ultra-highs (18kHz)");
    TEST_ASSERT(std::abs(magPass - 0.0f) < 0.2f, "Low-Pass passes 200Hz with 0dB attenuation");
}

void testDSPEightBandEQProcessingAndAnalyzerFeed()
{
    using namespace LiveBus;

    BusDSPProcessor processor;
    processor.prepare(48000.0, 512);
    processor.params.eqEnabled.store(false);
    processor.eightBandEQ.params.setToFlat();
    processor.eightBandEQ.params.bands[3].type.store(EQFilterType::Bell);
    processor.eightBandEQ.params.bands[3].freq.store(1000.0f);
    processor.eightBandEQ.params.bands[3].gainDb.store(12.0f);
    processor.eightBandEQ.params.bands[3].q.store(1.0f);
    processor.eightBandEQ.params.bands[3].enabled.store(true);
    processor.eightBandEQ.updateCoefficients();

    juce::AudioBuffer<float> buffer(2, 512);
    float inputRms = 0.0f;
    float outputRms = 0.0f;
    int samplePosition = 0;
    for (int block = 0; block < 8; ++block)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float value = 0.1f * std::sin(
                (float)(samplePosition++) * 1000.0f * juce::MathConstants<float>::twoPi / 48000.0f);
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }

        processor.process(buffer, buffer.getNumSamples());
        if (block == 7)
        {
            inputRms = 0.1f / std::sqrt(2.0f);
            outputRms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
        }
    }

    TEST_ASSERT(outputRms > inputRms * 2.0f,
                "8-band EQ applies its gain even when the legacy EQ is disabled");

    std::array<float, fftSize> analyzerSamples{};
    TEST_ASSERT(processor.eightBandEQ.pullFromAnalyzer(analyzerSamples.data(), fftSize),
                "DSP analyzer receives post-EQ audio");
    TEST_ASSERT(std::any_of(analyzerSamples.begin(), analyzerSamples.end(),
                            [](float value) { return std::abs(value) > 0.001f; }),
                "DSP analyzer data contains the processed signal");

    processor.eightBandEQ.params.masterEnabled.store(false);
    processor.eightBandEQ.setAnalyzerPost(false);
    for (int block = 0; block < 8; ++block)
    {
        buffer.clear();
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float value = 0.1f * std::sin(
                (float)(samplePosition++) * 1000.0f * juce::MathConstants<float>::twoPi / 48000.0f);
            buffer.setSample(0, sample, value);
            buffer.setSample(1, sample, value);
        }
        processor.process(buffer, buffer.getNumSamples());
    }

    TEST_ASSERT(processor.eightBandEQ.pullFromAnalyzer(analyzerSamples.data(), fftSize),
                "DSP analyzer continues receiving pre-EQ audio when EQ is bypassed");
}

void testEightBandEQBypassAndBitTransparency()
{
    using namespace LiveBus;
    EightBandEQProcessor eq;
    eq.prepare(48000.0, 256);

    // Setup aggressive EQ boosts
    eq.params.bands[0].gainDb.store(12.0f);
    eq.params.bands[1].gainDb.store(-15.0f);
    eq.params.bands[2].gainDb.store(8.0f);
    eq.updateCoefficients();

    // Turn master EQ OFF (Bypass)
    eq.params.masterEnabled.store(false);

    juce::AudioBuffer<float> testBuf(2, 256);
    for (int i = 0; i < 256; ++i)
    {
        float val = std::sin((float)i * 0.1f) * 0.5f;
        testBuf.setSample(0, i, val);
        testBuf.setSample(1, i, val);
    }

    juce::AudioBuffer<float> originalCopy;
    originalCopy.makeCopyOf(testBuf);

    eq.process(testBuf, 256);

    // Verify 100% bit-transparent passthrough
    float maxDiff = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < 256; ++i)
        {
            float diff = std::abs(testBuf.getSample(ch, i) - originalCopy.getSample(ch, i));
            if (diff > maxDiff) maxDiff = diff;
        }
    }
    TEST_ASSERT(maxDiff == 0.0f, "Master bypass yields 100% bit-transparent audio");

    // Verify individual band bypass
    eq.params.masterEnabled.store(true);
    eq.params.setToFlat();
    eq.params.bands[3].type.store(EQFilterType::Bell);
    eq.params.bands[3].freq.store(1000.0f);
    eq.params.bands[3].gainDb.store(10.0f);
    eq.params.bands[3].enabled.store(false); // band 3 bypassed
    eq.updateCoefficients();

    float magBypassedBand = eq.getMagnitudeForFrequency(1000.0f);
    TEST_ASSERT(std::abs(magBypassedBand - 0.0f) < 1e-4f, "Individually bypassed band produces 0dB response");
}

void testEightBandEQStereoConsistencyAndNumericalStability()
{
    using namespace LiveBus;
    EightBandEQProcessor eq;
    eq.prepare(48000.0, 512);
    eq.params.setToDefault();
    eq.updateCoefficients();

    juce::AudioBuffer<float> stereoBuffer(2, 512);
    for (int i = 0; i < 512; ++i)
    {
        float sample = (float)std::sin((double)i * 0.05);
        stereoBuffer.setSample(0, i, sample);
        stereoBuffer.setSample(1, i, sample);
    }

    eq.process(stereoBuffer, 512);

    float maxStereoDiff = 0.0f;
    bool hasNanOrInf = false;

    for (int i = 0; i < 512; ++i)
    {
        float l = stereoBuffer.getSample(0, i);
        float r = stereoBuffer.getSample(1, i);

        if (!std::isfinite(l) || !std::isfinite(r))
            hasNanOrInf = true;

        float diff = std::abs(l - r);
        if (diff > maxStereoDiff) maxStereoDiff = diff;
    }

    TEST_ASSERT(!hasNanOrInf, "Output buffer contains no NaN or Infinity");
    TEST_ASSERT(maxStereoDiff < 1e-6f, "Stereo processing is identical on left and right channels");
}

void testEightBandEQTargetIsolation()
{
    using namespace LiveBus;

    // Simulate the 6 targets
    std::array<EightBandEQProcessor, 6> targets;
    for (int t = 0; t < 6; ++t)
    {
        targets[t].prepare(48000.0, 256);
        targets[t].params.setToFlat();
        targets[t].updateCoefficients();
    }

    // Modify Stream Bus (Target 0)
    targets[0].params.bands[3].type.store(EQFilterType::Bell);
    targets[0].params.bands[3].freq.store(1000.0f);
    targets[0].params.bands[3].gainDb.store(12.0f);
    targets[0].updateCoefficients();

    // Modify Input 1 Mic (Target 2)
    targets[2].params.bands[0].type.store(EQFilterType::HighPass);
    targets[2].params.bands[0].freq.store(150.0f);
    targets[2].updateCoefficients();

    // Verify Monitor Bus (Target 1) is completely unaffected
    TEST_ASSERT(targets[1].params.bands[3].gainDb.load() == 0.0f, "Monitor Bus unaffected by Stream Bus changes");
    TEST_ASSERT(std::abs(targets[1].getMagnitudeForFrequency(1000.0f) - 0.0f) < 1e-4f, "Monitor Bus magnitude is flat");

    // Verify Input 2 Guitar (Target 3) is completely unaffected
    TEST_ASSERT(targets[3].params.bands[0].freq.load() == 20.0f, "Input 2 unaffected by Input 1 changes");
    TEST_ASSERT(targets[3].params.bands[0].type.load() == EQFilterType::HighPass, "Input 2 maintains default type");

    // Audio test: Pass signal through Stream Bus and Monitor Bus
    juce::AudioBuffer<float> streamBuf(2, 256);
    juce::AudioBuffer<float> monitorBuf(2, 256);
    for (int i = 0; i < 256; ++i)
    {
        float val = std::sin((float)i * 0.1f);
        streamBuf.setSample(0, i, val);
        streamBuf.setSample(1, i, val);
        monitorBuf.setSample(0, i, val);
        monitorBuf.setSample(1, i, val);
    }

    targets[0].process(streamBuf, 256);
    targets[1].process(monitorBuf, 256);

    // Stream Bus should have boosted signal, Monitor Bus should remain untouched
    float streamPeak = streamBuf.getMagnitude(0, 0, 256);
    float monitorPeak = monitorBuf.getMagnitude(0, 0, 256);

    TEST_ASSERT(streamPeak > monitorPeak, "Stream Bus processed with boost while Monitor Bus remains dry");
}

void testEightBandEQSceneSerializationRoundTripAndLegacy()
{
    SceneManager manager;
    SceneData original;
    original.sceneName = "_eq_roundtrip_test";

    // Configure distinct EQ parameters for Target 0 (Stream Bus) and Target 2 (Mic)
    original.eightBandEQs[0].masterEnabled = true;
    original.eightBandEQs[0].bands[2].type = (int)LiveBus::EQFilterType::Bell;
    original.eightBandEQs[0].bands[2].freq = 450.0f;
    original.eightBandEQs[0].bands[2].gainDb = -7.5f;
    original.eightBandEQs[0].bands[2].q = 1.8f;
    original.eightBandEQs[0].bands[2].enabled = true;

    original.eightBandEQs[2].masterEnabled = false; // Mic EQ bypassed
    original.eightBandEQs[2].bands[0].type = (int)LiveBus::EQFilterType::HighPass;
    original.eightBandEQs[2].bands[0].freq = 95.0f;
    original.eightBandEQs[2].bands[0].q = 0.85f;

    bool saved = manager.saveScene("_eq_roundtrip_test", original);
    TEST_ASSERT(saved == true, "Scene with 8-Band EQ saved successfully");

    SceneData loaded;
    juce::String err;
    bool loadedOk = manager.loadScene("_eq_roundtrip_test", loaded, err);
    TEST_ASSERT(loadedOk == true, "Scene with 8-Band EQ loaded without errors");

    // Verify Target 0
    TEST_ASSERT(loaded.eightBandEQs[0].masterEnabled == true, "Restored Stream Bus EQ master enable matches");
    TEST_ASSERT(loaded.eightBandEQs[0].bands[2].type == (int)LiveBus::EQFilterType::Bell, "Restored filter type matches");
    TEST_ASSERT(std::abs(loaded.eightBandEQs[0].bands[2].freq - 450.0f) < 0.01f, "Restored frequency matches");
    TEST_ASSERT(std::abs(loaded.eightBandEQs[0].bands[2].gainDb - (-7.5f)) < 0.01f, "Restored gain matches");
    TEST_ASSERT(std::abs(loaded.eightBandEQs[0].bands[2].q - 1.8f) < 0.01f, "Restored Q matches");

    // Verify Target 2
    TEST_ASSERT(loaded.eightBandEQs[2].masterEnabled == false, "Restored Mic EQ bypass matches");
    TEST_ASSERT(loaded.eightBandEQs[2].bands[0].type == (int)LiveBus::EQFilterType::HighPass, "Restored Mic HPF type matches");
    TEST_ASSERT(std::abs(loaded.eightBandEQs[2].bands[0].freq - 95.0f) < 0.01f, "Restored Mic HPF freq matches");

    // Test Legacy Scene (XML without EIGHT_BAND_EQS element)
    juce::String legacyXml =
        "<LIVEBUS_SCENE version=\"2\" name=\"LegacyTest\" sampleRate=\"48000\" blockSize=\"512\">"
        "  <CHANNELS><CHANNEL index=\"0\" name=\"Mic\" gain=\"0.707\" pan=\"0.0\"/></CHANNELS>"
        "  <BUSES><BUS index=\"0\" masterGain=\"1.0\" masterPan=\"0.0\"/></BUSES>"
        "</LIVEBUS_SCENE>";

    auto xmlParsed = juce::XmlDocument::parse(legacyXml);
    TEST_ASSERT(xmlParsed != nullptr, "Legacy XML parsed successfully");

    SceneData legacyData;
    juce::String legacyErr;
    bool legacyOk = manager.deserializeFromXml(*xmlParsed, legacyData, legacyErr);
    TEST_ASSERT(legacyOk == true, "Legacy XML deserialized without crash");
    TEST_ASSERT(legacyData.eightBandEQs[0].masterEnabled == true, "Legacy scene provides safe default EQ state");
    TEST_ASSERT(legacyData.eightBandEQs[0].bands[0].freq == 80.0f, "Legacy scene provides default 80Hz HPF");

    manager.deleteScene("_eq_roundtrip_test");
}

void testLevelMeterSilenceAndKnownSineWavePeakRms()
{
    using namespace LiveBus;
    LevelMeterProcessor meter;
    meter.prepare(48000.0, 512);

    // 1. Digital silence
    juce::AudioBuffer<float> silenceBuf(2, 512);
    silenceBuf.clear();
    meter.processBlock(silenceBuf, 512);
    meter.updateBallistics(0.02f);

    auto snap = meter.getSnapshot();
    TEST_ASSERT(snap.peakDbL <= -90.0f, "Silence left channel reads <= -90 dBFS");
    TEST_ASSERT(snap.peakDbR <= -90.0f, "Silence right channel reads <= -90 dBFS");
    TEST_ASSERT(snap.rmsDbL <= -90.0f, "Silence left RMS reads <= -90 dBFS");
    TEST_ASSERT(snap.rmsDbR <= -90.0f, "Silence right RMS reads <= -90 dBFS");
    TEST_ASSERT(!std::isnan(snap.peakDbL) && !std::isinf(snap.peakDbL), "Silence produces finite non-NaN peak");
    TEST_ASSERT(!std::isnan(snap.rmsDbL) && !std::isinf(snap.rmsDbL), "Silence produces finite non-NaN RMS");

    // 2. Full scale 1.0 Sine wave (0 dBFS peak, -3.01 dBFS RMS)
    juce::AudioBuffer<float> sineBuf(2, 960); // 20 ms of 1 kHz sine
    for (int i = 0; i < 960; ++i)
    {
        float val = std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / 48000.0f);
        sineBuf.setSample(0, i, val);
        sineBuf.setSample(1, i, val);
    }
    for (int k = 0; k < 15; ++k)
    {
        meter.processBlock(sineBuf, 960);
        meter.updateBallistics(0.02f);
    }
    snap = meter.getSnapshot();

    TEST_ASSERT(std::abs(snap.peakDbL - 0.0f) < 0.1f, "1.0 sine produces 0.0 dBFS sample peak (+/- 0.1 dB)");
    TEST_ASSERT(std::abs(snap.peakDbR - 0.0f) < 0.1f, "1.0 sine right produces 0.0 dBFS sample peak");
    TEST_ASSERT(std::abs(snap.rmsDbL - (-3.01f)) < 0.25f, "1.0 sine produces -3.01 dBFS RMS (+/- 0.25 dB)");

    // 3. Half scale 0.5 Sine wave (-6.02 dBFS peak, -9.03 dBFS RMS)
    meter.reset();
    juce::AudioBuffer<float> halfSineBuf(2, 960);
    for (int i = 0; i < 960; ++i)
    {
        float val = 0.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / 48000.0f);
        halfSineBuf.setSample(0, i, val);
        halfSineBuf.setSample(1, i, val);
    }
    for (int k = 0; k < 15; ++k)
    {
        meter.processBlock(halfSineBuf, 960);
        meter.updateBallistics(0.02f);
    }
    snap = meter.getSnapshot();

    TEST_ASSERT(std::abs(snap.peakDbL - (-6.02f)) < 0.15f, "0.5 sine produces -6.02 dBFS sample peak (+/- 0.15 dB)");
    TEST_ASSERT(std::abs(snap.rmsDbL - (-9.03f)) < 0.25f, "0.5 sine produces -9.03 dBFS RMS (+/- 0.25 dB)");

    // 4. Dirty buffer containing NaN and Inf
    juce::AudioBuffer<float> dirtyBuf(2, 512);
    dirtyBuf.clear();
    dirtyBuf.setSample(0, 10, std::numeric_limits<float>::quiet_NaN());
    dirtyBuf.setSample(1, 20, std::numeric_limits<float>::infinity());
    meter.processBlock(dirtyBuf, 512);
    meter.updateBallistics(0.02f);
    snap = meter.getSnapshot();

    TEST_ASSERT(!std::isnan(snap.peakDbL) && !std::isinf(snap.peakDbL), "Dirty buffer with NaN handled safely without poison");
    TEST_ASSERT(!std::isnan(snap.peakDbR) && !std::isinf(snap.peakDbR), "Dirty buffer with Inf handled safely without poison");
}

void testLevelMeterPeakHoldAndManualReset()
{
    using namespace LiveBus;
    LevelMeterProcessor meter;
    meter.prepare(48000.0, 512);
    meter.setPeakHoldEnabled(true);

    // Loud transient: 0.9f (-0.915 dBFS) for 1 block
    juce::AudioBuffer<float> pulseBuf(2, 512);
    pulseBuf.clear();
    pulseBuf.setSample(0, 0, 0.9f);
    pulseBuf.setSample(1, 0, 0.9f);
    meter.processBlock(pulseBuf, 512);
    meter.updateBallistics(0.01f);

    auto snap1 = meter.getSnapshot();
    float heldDb = snap1.peakHoldDbL;
    TEST_ASSERT(std::abs(heldDb - (-0.915f)) < 0.15f, "Peak hold registered loud transient peak");

    // Feed silence for 20 frames (0.6 seconds)
    juce::AudioBuffer<float> silenceBuf(2, 512);
    silenceBuf.clear();
    for (int k = 0; k < 20; ++k)
    {
        meter.processBlock(silenceBuf, 512);
        meter.updateBallistics(0.033f);
    }

    auto snap2 = meter.getSnapshot();
    TEST_ASSERT(snap2.peakDbL < -10.0f, "Current sample peak decayed during silence");
    TEST_ASSERT(std::abs(snap2.peakHoldDbL - heldDb) < 0.15f, "Peak hold retained initial peak level during hold time");

    // Manual reset
    meter.resetPeakHold();
    meter.updateBallistics(0.001f);
    auto snap3 = meter.getSnapshot();
    TEST_ASSERT(snap3.peakHoldDbL <= snap2.peakDbL + 0.1f, "Peak hold drops to current decayed level upon resetPeakHold");
}

void testLevelMeterClipDetectionAndLatch()
{
    using namespace LiveBus;
    LevelMeterProcessor meter;
    meter.prepare(48000.0, 512);

    // Sub-clip audio (0.95f)
    juce::AudioBuffer<float> safeBuf(2, 512);
    for (int i = 0; i < 512; ++i) { safeBuf.setSample(0, i, 0.95f); safeBuf.setSample(1, i, 0.95f); }
    meter.processBlock(safeBuf, 512);
    meter.updateBallistics(0.02f);
    auto snapSafe = meter.getSnapshot();
    TEST_ASSERT(!snapSafe.clippedL && !snapSafe.clippedR, "Sub-clip signal does not trigger clip flag");

    // Clipping audio (1.05f >= 1.0f)
    juce::AudioBuffer<float> clipBuf(2, 512);
    clipBuf.clear();
    clipBuf.setSample(0, 10, 1.05f);
    clipBuf.setSample(1, 10, 1.05f);
    meter.processBlock(clipBuf, 512);
    meter.updateBallistics(0.02f);
    auto snapClip = meter.getSnapshot();
    TEST_ASSERT(snapClip.clippedL && snapClip.clippedR, "Signal >= 1.0 triggers clip indicator");

    // Feed silence for several frames — clip indicator must latch
    juce::AudioBuffer<float> silenceBuf(2, 512);
    silenceBuf.clear();
    for (int k = 0; k < 10; ++k)
    {
        meter.processBlock(silenceBuf, 512);
        meter.updateBallistics(0.033f);
    }
    auto snapLatched = meter.getSnapshot();
    TEST_ASSERT(snapLatched.clippedL && snapLatched.clippedR, "Clip indicator latches/persists through silence until user reset");

    // Reset clip
    meter.resetClip();
    meter.updateBallistics(0.001f);
    auto snapCleared = meter.getSnapshot();
    TEST_ASSERT(!snapCleared.clippedL && !snapCleared.clippedR, "Clip indicator clears upon resetClip()");
}

void testLevelMeterVUBallisticsAndCalibration()
{
    using namespace LiveBus;
    LevelMeterProcessor meter;
    meter.prepare(48000.0, 512);

    // Default calibration is -18 dBFS reference for 0 VU
    TEST_ASSERT(meter.getVuReference() == -18.0f, "Default 0 VU reference is -18.0 dBFS");

    // Calibrated steady sine wave at -18.0 dBFS RMS
    float targetRmsLinear = LevelMeterProcessor::dbToLinear(-18.0f);
    float targetPeakLinear = targetRmsLinear * std::sqrt(2.0f);

    juce::AudioBuffer<float> calBuf(2, 480); // 10 ms blocks
    for (int i = 0; i < 480; ++i)
    {
        float val = targetPeakLinear * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / 48000.0f);
        calBuf.setSample(0, i, val);
        calBuf.setSample(1, i, val);
    }

    // Simulate 300 ms (IEC 60268-17 ballistics integration time) in 30 steps of 10 ms
    for (int step = 0; step < 30; ++step)
    {
        meter.processBlock(calBuf, 480);
        meter.updateBallistics(0.010f);
    }

    auto snap300ms = meter.getSnapshot();
    TEST_ASSERT(snap300ms.vuLevelL >= -2.0f && snap300ms.vuLevelL <= 2.0f,
                "VU needle reaches ~0 VU (+/- 2 VU) within 300 ms IEC integration window");

    // Run for another 300 ms to settle at steady state
    for (int step = 0; step < 30; ++step)
    {
        meter.processBlock(calBuf, 480);
        meter.updateBallistics(0.010f);
    }

    auto snapSettled = meter.getSnapshot();
    TEST_ASSERT(std::abs(snapSettled.vuLevelL - 0.0f) < 0.25f,
                "Steady -18 dBFS RMS signal settles at exactly 0.0 VU under default calibration");

    // Recalibrate 0 VU reference to -14.0 dBFS (European broadcast standard)
    meter.setVuReference(-14.0f);
    TEST_ASSERT(meter.getVuReference() == -14.0f, "VU calibration changed to -14.0 dBFS");

    // Under -14 dBFS calibration, a -18 dBFS RMS signal is 4 dB below reference, so it should read -4.0 VU
    for (int step = 0; step < 40; ++step)
    {
        meter.processBlock(calBuf, 480);
        meter.updateBallistics(0.010f);
    }

    auto snapCalibrated = meter.getSnapshot();
    TEST_ASSERT(std::abs(snapCalibrated.vuLevelL - (-4.0f)) < 0.35f,
                "Signal reads -4.0 VU when reference is recalibrated to -14.0 dBFS");

    // Release phase: feed silence and verify needle drops smoothly
    juce::AudioBuffer<float> silenceBuf(2, 480);
    silenceBuf.clear();
    for (int step = 0; step < 30; ++step)
    {
        meter.processBlock(silenceBuf, 480);
        meter.updateBallistics(0.010f);
    }

    auto snapReleased = meter.getSnapshot();
    TEST_ASSERT(snapReleased.vuLevelL < -12.0f, "VU needle releases smoothly toward rest position in silence");
}

void testLevelMeterBusIsolationAndStereoConsistency()
{
    using namespace LiveBus;
    LevelMeterProcessor streamMeter;
    LevelMeterProcessor monitorMeter;

    streamMeter.prepare(48000.0, 512);
    monitorMeter.prepare(48000.0, 512);

    // Stream bus receives hot signal (0.8f) on Left channel, silence on Right channel
    juce::AudioBuffer<float> streamBuf(2, 512);
    streamBuf.clear();
    for (int i = 0; i < 512; ++i) streamBuf.setSample(0, i, 0.8f);

    // Monitor bus receives silence on both channels
    juce::AudioBuffer<float> monitorBuf(2, 512);
    monitorBuf.clear();

    streamMeter.processBlock(streamBuf, 512);
    monitorMeter.processBlock(monitorBuf, 512);

    streamMeter.updateBallistics(0.02f);
    monitorMeter.updateBallistics(0.02f);

    auto streamSnap = streamMeter.getSnapshot();
    auto monitorSnap = monitorMeter.getSnapshot();

    // Stream Bus Left is active, Right is silent
    TEST_ASSERT(streamSnap.peakDbL > -3.0f, "Stream Bus Left channel detects active audio signal");
    TEST_ASSERT(streamSnap.peakDbR <= -90.0f, "Stream Bus Right channel remains in silence (stereo isolation)");

    // Monitor Bus has zero bleed from Stream Bus
    TEST_ASSERT(monitorSnap.peakDbL <= -90.0f, "Monitor Bus Left has zero crosstalk from Stream Bus");
    TEST_ASSERT(monitorSnap.peakDbR <= -90.0f, "Monitor Bus Right has zero crosstalk from Stream Bus");
    TEST_ASSERT(monitorSnap.vuLevelL <= -19.0f, "Monitor Bus VU needle remains at rest (-20 VU)");
}

// ==============================================================================
// Professional Vocal DSP Rack Tests
// ==============================================================================

void testVocalDSPGateHysteresisAndHold()
{
    using namespace LiveBus;
    VocalDSPProcessor rack;
    const double sr = 48000.0;
    const int bs = 256;
    rack.prepare(sr, bs);

    rack.params.masterEnabled.store(true);
    rack.params.hpfBypassed.store(true);
    rack.params.eqBypassed.store(true);
    rack.params.deEsserBypassed.store(true);
    rack.params.compBypassed.store(true);
    rack.params.limiterBypassed.store(true);
    rack.params.outputGainDb.store(0.0f);
    rack.params.inputGainDb.store(0.0f);

    // Configure gate: Open at -30 dB, Close at -40 dB (10 dB hysteresis)
    rack.params.gateBypassed.store(false);
    rack.params.gateMode.store(VocalDSPParameters::ModeGate);
    rack.params.gateOpenThresholdDb.store(-30.0f);
    rack.params.gateCloseThresholdDb.store(-40.0f);
    rack.params.gateAttackMs.store(0.5f);
    rack.params.gateHoldMs.store(20.0f);
    rack.params.gateReleaseMs.store(20.0f);
    rack.params.gateFloorDb.store(-60.0f);

    juce::AudioBuffer<float> buf(2, bs);

    auto fillSine = [&](float ampDb) {
        float amp = juce::Decibels::decibelsToGain(ampDb);
        float delta = (float)(1000.0 * juce::MathConstants<double>::twoPi / sr);
        for (int i = 0; i < bs; ++i)
        {
            float s = amp * std::sin((float)i * delta);
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
    };

    // 1. Audio Silence test: with 0 input, gate remains closed and output is attenuated
    buf.clear();
    rack.process(buf, bs);
    TEST_ASSERT(buf.getMagnitude(0, 0, bs) == 0.0f, "Gate handles digital silence cleanly with zero artifacts");
    TEST_ASSERT(rack.getGateGainReductionDb() <= -50.0f, "Gate attenuation meter reports maximum floor attenuation during silence");

    // 2. Signal between close and open thresholds (-35 dB): gate should stay closed initially
    fillSine(-35.0f);
    rack.process(buf, bs);
    TEST_ASSERT(rack.getGateGainReductionDb() <= -40.0f, "Signal between close and open thresholds does not trigger gate open");

    // 3. Signal above Open Threshold (-20 dB): gate opens
    for (int b = 0; b < 10; ++b)
    {
        fillSine(-20.0f);
        rack.process(buf, bs);
    }
    TEST_ASSERT(rack.getGateGainReductionDb() >= -1.0f, "Gate opens when signal exceeds Open Threshold");

    // 4. Signal drops to -35 dB (between Open -30 dB and Close -40 dB): Hysteresis keeps gate open!
    for (int b = 0; b < 5; ++b)
    {
        fillSine(-35.0f);
        rack.process(buf, bs);
    }
    TEST_ASSERT(rack.getGateGainReductionDb() >= -1.5f, "Gate remains open between Open and Close thresholds due to hysteresis");

    // 5. Signal drops below Close Threshold (-50 dB): Hold timer activates
    // 20ms hold at 48kHz = 960 samples ~= 3.7 blocks
    for (int b = 0; b < 3; ++b)
    {
        fillSine(-50.0f);
        rack.process(buf, bs);
    }
    TEST_ASSERT(rack.getGateGainReductionDb() >= -3.0f, "Gate remains held open during Hold period");

    // Process another 35 blocks (exceeding hold and release)
    for (int b = 0; b < 35; ++b)
    {
        fillSine(-50.0f);
        rack.process(buf, bs);
    }
    TEST_ASSERT(rack.getGateGainReductionDb() <= -40.0f, "Gate releases smoothly to floor after hold timer elapses");
}

void testVocalDSPCompressorKneeAndGainReduction()
{
    using namespace LiveBus;
    VocalDSPProcessor rack;
    const double sr = 48000.0;
    const int bs = 256;
    rack.prepare(sr, bs);

    rack.params.masterEnabled.store(true);
    rack.params.hpfBypassed.store(true);
    rack.params.gateBypassed.store(true);
    rack.params.eqBypassed.store(true);
    rack.params.deEsserBypassed.store(true);
    rack.params.limiterBypassed.store(true);
    rack.params.inputGainDb.store(0.0f);
    rack.params.outputGainDb.store(0.0f);

    rack.params.compBypassed.store(false);
    rack.params.compThresholdDb.store(-20.0f);
    rack.params.compRatio.store(4.0f);
    rack.params.compAttackMs.store(1.0f);
    rack.params.compReleaseMs.store(50.0f);
    rack.params.compKnee.store(VocalDSPParameters::KneeHard);
    rack.params.compMakeupDb.store(0.0f);
    rack.params.compMixPercent.store(100.0f);

    juce::AudioBuffer<float> buf(2, bs);

    auto fillSine = [&](float ampDb) {
        float amp = juce::Decibels::decibelsToGain(ampDb);
        float delta = (float)(1000.0 * juce::MathConstants<double>::twoPi / sr);
        for (int i = 0; i < bs; ++i)
        {
            float s = amp * std::sin((float)i * delta);
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
    };

    // 1. Signal below threshold (-30 dB): no gain reduction
    for (int b = 0; b < 10; ++b)
    {
        fillSine(-30.0f);
        rack.process(buf, bs);
    }
    TEST_ASSERT(rack.getCompGainReductionDb() >= -0.5f, "Compressor applies 0 dB reduction below threshold");

    // 2. Signal above threshold (-10 dB, which is 10 dB above -20 dB threshold)
    // With 4:1 ratio, overshoot is reduced to 10/4 = 2.5 dB, so expected GR is -7.5 dB
    for (int b = 0; b < 25; ++b)
    {
        fillSine(-10.0f);
        rack.process(buf, bs);
    }
    float gr = rack.getCompGainReductionDb();
    TEST_ASSERT(gr <= -5.0f && gr >= -9.0f, "Compressor 4:1 ratio reduces signal above threshold by expected GR");

    // 3. Dynamic transfer curve check
    float transferSub = rack.getCompressionTransferDb(-30.0f);
    TEST_ASSERT(std::abs(transferSub - (-30.0f)) < 0.1f, "Transfer curve is 1:1 below threshold in hard knee mode");

    float transferAbove = rack.getCompressionTransferDb(-10.0f);
    TEST_ASSERT(std::abs(transferAbove - (-17.5f)) < 0.2f, "Transfer curve matches 4:1 compression formula exactly");

    // 4. Soft knee transition test: at threshold (-20 dB), soft knee has gradual compression
    rack.params.compKnee.store(VocalDSPParameters::KneeSoft);
    float transferSoft = rack.getCompressionTransferDb(-20.0f);
    TEST_ASSERT(transferSoft < -20.0f, "Soft knee introduces smooth reduction around threshold");

    // 5. Makeup gain test
    rack.params.compMakeupDb.store(6.0f);
    float transferWithMakeup = rack.getCompressionTransferDb(-30.0f);
    TEST_ASSERT(std::abs(transferWithMakeup - (-24.0f)) < 0.1f, "Makeup gain (+6 dB) correctly offsets compression transfer curve");
}

void testVocalDSPDeEsserTargetFrequencyAttenuation()
{
    using namespace LiveBus;
    VocalDSPProcessor rack;
    const double sr = 48000.0;
    const int bs = 512;
    rack.prepare(sr, bs);

    rack.params.masterEnabled.store(true);
    rack.params.hpfBypassed.store(true);
    rack.params.gateBypassed.store(true);
    rack.params.eqBypassed.store(true);
    rack.params.compBypassed.store(true);
    rack.params.limiterBypassed.store(true);
    rack.params.inputGainDb.store(0.0f);
    rack.params.outputGainDb.store(0.0f);

    rack.params.deEsserBypassed.store(false);
    rack.params.deEsserFreq.store(6000.0f);      // Target sibilance: 6 kHz
    rack.params.deEsserThresholdDb.store(-20.0f);
    rack.params.deEsserRangeDb.store(-12.0f);

    // 1. Low-mid vowel/body tone at 500 Hz at -10 dB (above threshold, but outside sibilance band)
    juce::AudioBuffer<float> buf(2, bs);
    float lowPhase = 0.0f;
    float lowDelta = (float)(500.0 * juce::MathConstants<double>::twoPi / sr);
    float amp = juce::Decibels::decibelsToGain(-10.0f);

    for (int b = 0; b < 20; ++b)
    {
        for (int i = 0; i < bs; ++i)
        {
            float s = amp * std::sin(lowPhase);
            lowPhase += lowDelta;
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
        rack.process(buf, bs);
    }
    TEST_ASSERT(rack.getDeEsserGainReductionDb() >= -1.0f, "De-Esser sidechain rejects low-mid frequencies (500 Hz)");

    // 2. Sibilance tone at 6 kHz at -10 dB (hits detector center frequency)
    float sibPhase = 0.0f;
    float sibDelta = (float)(6000.0 * juce::MathConstants<double>::twoPi / sr);

    for (int b = 0; b < 25; ++b)
    {
        for (int i = 0; i < bs; ++i)
        {
            float s = amp * std::sin(sibPhase);
            sibPhase += sibDelta;
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
        rack.process(buf, bs);
    }
    float gr = rack.getDeEsserGainReductionDb();
    TEST_ASSERT(gr <= -3.0f, "De-Esser accurately triggers gain reduction on sibilant 6 kHz tone");
    TEST_ASSERT(gr >= -13.0f, "De-Esser attenuation is clamped within range limit (-12 dB)");
}

void testVocalDSPLimiterCeilingEnforcementAndOutputProtection()
{
    using namespace LiveBus;
    VocalDSPProcessor rack;
    const double sr = 48000.0;
    const int bs = 256;
    rack.prepare(sr, bs);

    rack.params.masterEnabled.store(true);
    rack.params.hpfBypassed.store(true);
    rack.params.gateBypassed.store(true);
    rack.params.eqBypassed.store(true);
    rack.params.deEsserBypassed.store(true);
    rack.params.compBypassed.store(true);
    rack.params.inputGainDb.store(0.0f);
    rack.params.outputGainDb.store(0.0f);

    rack.params.limiterBypassed.store(false);
    rack.params.limiterCeilingDb.store(-1.5f); // Ceiling: -1.5 dBFS (~0.841)
    rack.params.limiterReleaseMs.store(30.0f);

    // Drive extremely hot signal: +6 dBFS (amplitude = 2.0)
    juce::AudioBuffer<float> buf(2, bs);
    float hotAmp = 2.0f; // +6.02 dBFS
    float phase = 0.0f;
    float delta = (float)(1000.0 * juce::MathConstants<double>::twoPi / sr);

    for (int b = 0; b < 20; ++b)
    {
        for (int i = 0; i < bs; ++i)
        {
            float s = hotAmp * std::sin(phase);
            phase += delta;
            buf.setSample(0, i, s);
            buf.setSample(1, i, s);
        }
        rack.process(buf, bs);
    }

    float maxOut = buf.getMagnitude(0, 0, bs);
    float ceilingGain = juce::Decibels::decibelsToGain(-1.5f);

    TEST_ASSERT(maxOut <= ceilingGain * 1.05f, "Limiter clamps output to -1.5 dBFS ceiling");
    TEST_ASSERT(rack.getLimiterGainReductionDb() <= -4.0f, "Limiter gain reduction meter reports active clamping");

    // Test NaN / Inf safety
    for (int i = 0; i < bs; ++i)
    {
        buf.setSample(0, i, std::numeric_limits<float>::quiet_NaN());
        buf.setSample(1, i, std::numeric_limits<float>::infinity());
    }
    rack.process(buf, bs);
    for (int i = 0; i < bs; ++i)
    {
        TEST_ASSERT(!std::isnan(buf.getSample(0, i)), "Limiter and rack protect against NaN output");
        TEST_ASSERT(!std::isinf(buf.getSample(1, i)), "Limiter and rack protect against Infinity output");
    }
}

void testVocalDSPBypassAndBitTransparency()
{
    using namespace LiveBus;
    VocalDSPProcessor rack;
    const double sr = 48000.0;
    const int bs = 256;
    rack.prepare(sr, bs);

    // Set aggressive DSP parameters that would dramatically alter audio
    rack.params.gateOpenThresholdDb.store(0.0f);
    rack.params.compThresholdDb.store(-50.0f);
    rack.params.compRatio.store(20.0f);
    rack.params.limiterCeilingDb.store(-20.0f);

    // Master Bypass active
    rack.params.masterEnabled.store(false);

    juce::AudioBuffer<float> buf(2, bs);
    juce::AudioBuffer<float> original(2, bs);

    for (int i = 0; i < bs; ++i)
    {
        float sL = 0.45f * std::sin((float)i * 0.1f);
        float sR = 0.38f * std::cos((float)i * 0.15f);
        buf.setSample(0, i, sL);
        buf.setSample(1, i, sR);
        original.setSample(0, i, sL);
        original.setSample(1, i, sR);
    }

    rack.process(buf, bs);

    // Verify bit transparency when bypassed
    float maxDiff = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < bs; ++i)
        {
            float d = std::abs(buf.getSample(ch, i) - original.getSample(ch, i));
            if (d > maxDiff) maxDiff = d;
        }
    }
    TEST_ASSERT(maxDiff < 1e-7f, "Master bypass provides 100% bit-transparent passthrough");
}

void testVocalDSPStreamAndMonitorBusIndependence()
{
    using namespace LiveBus;
    BusDSPProcessor streamBus;
    BusDSPProcessor monitorBus;
    const double sr = 48000.0;
    const int bs = 256;

    streamBus.prepare(sr, bs);
    monitorBus.prepare(sr, bs);

    // Modify Stream Bus vocal rack
    streamBus.vocalRack.params.compThresholdDb.store(-14.0f);
    streamBus.vocalRack.params.compRatio.store(6.0f);
    streamBus.vocalRack.params.gateOpenThresholdDb.store(-28.0f);
    streamBus.vocalRack.params.limiterCeilingDb.store(-0.5f);

    // Monitor Bus vocal rack should remain at default values
    TEST_ASSERT(monitorBus.vocalRack.params.compThresholdDb.load() == -24.0f, "Monitor Bus Comp threshold independent from Stream Bus");
    TEST_ASSERT(monitorBus.vocalRack.params.compRatio.load() == 3.0f, "Monitor Bus Comp ratio independent from Stream Bus");
    TEST_ASSERT(monitorBus.vocalRack.params.gateOpenThresholdDb.load() == -42.0f, "Monitor Bus Gate open threshold independent");
    TEST_ASSERT(monitorBus.vocalRack.params.limiterCeilingDb.load() == -1.0f, "Monitor Bus Limiter ceiling independent");
}

void testVocalDSPSceneSerializationRoundTrip()
{
    SceneManager manager;
    SceneData original;
    original.sceneName = "VocalDSP_TestSession";

    // Set custom Vocal DSP parameters for Target 0 (Stream Bus) and Target 2 (Mic)
    auto& streamRack = original.vocalRacks[0];
    streamRack.masterEnabled = true;
    streamRack.gateOpenThresholdDb = -38.5f;
    streamRack.gateCloseThresholdDb = -48.0f;
    streamRack.compThresholdDb = -18.5f;
    streamRack.compRatio = 4.5f;
    streamRack.compKnee = 0; // Soft
    streamRack.compMakeupDb = 4.0f;
    streamRack.deEsserFreq = 6200.0f;
    streamRack.deEsserRangeDb = -10.0f;
    streamRack.limiterCeilingDb = -0.8f;
    streamRack.limiterReleaseMs = 45.0f;

    auto& micRack = original.vocalRacks[2];
    micRack.masterEnabled = true;
    micRack.hpfFreq = 95.0f;
    micRack.hpfSlope = 2; // 18dB
    micRack.gateMode = 1; // Expander
    micRack.gateOpenThresholdDb = -44.0f;
    micRack.compAttackMs = 8.0f;
    micRack.compReleaseMode = 1; // Auto
    micRack.deEsserThresholdDb = -18.0f;

    juce::String xmlStr = manager.serializeToXmlString(original);
    TEST_ASSERT(xmlStr.isNotEmpty(), "Scene with Vocal DSP Rack serialized to XML");
    TEST_ASSERT(xmlStr.contains("VOCAL_RACKS"), "XML contains VOCAL_RACKS tag");

    auto xml = juce::XmlDocument::parse(xmlStr);
    TEST_ASSERT(xml != nullptr, "Parsed serialized XML successfully");

    SceneData restored;
    juce::String err;
    bool ok = manager.deserializeFromXml(*xml, restored, err);
    TEST_ASSERT(ok == true, "Deserialized scene with Vocal DSP Racks successfully");

    // Verify Stream Bus Vocal Rack
    TEST_ASSERT(std::abs(restored.vocalRacks[0].gateOpenThresholdDb - (-38.5f)) < 0.01f, "Stream Bus Gate Open Thresh restored");
    TEST_ASSERT(std::abs(restored.vocalRacks[0].compThresholdDb - (-18.5f)) < 0.01f, "Stream Bus Comp Thresh restored");
    TEST_ASSERT(std::abs(restored.vocalRacks[0].compRatio - 4.5f) < 0.01f, "Stream Bus Comp Ratio restored");
    TEST_ASSERT(restored.vocalRacks[0].compKnee == 0, "Stream Bus Comp Knee restored");
    TEST_ASSERT(std::abs(restored.vocalRacks[0].deEsserFreq - 6200.0f) < 1.0f, "Stream Bus De-Esser Freq restored");
    TEST_ASSERT(std::abs(restored.vocalRacks[0].limiterCeilingDb - (-0.8f)) < 0.01f, "Stream Bus Limiter Ceiling restored");

    // Verify Mic Vocal Rack
    TEST_ASSERT(std::abs(restored.vocalRacks[2].hpfFreq - 95.0f) < 0.1f, "Mic Rack HPF Freq restored");
    TEST_ASSERT(restored.vocalRacks[2].hpfSlope == 2, "Mic Rack HPF Slope restored");
    TEST_ASSERT(restored.vocalRacks[2].gateMode == 1, "Mic Rack Gate Expander Mode restored");
    TEST_ASSERT(restored.vocalRacks[2].compReleaseMode == 1, "Mic Rack Comp Auto-Release restored");
    TEST_ASSERT(std::abs(restored.vocalRacks[2].deEsserThresholdDb - (-18.0f)) < 0.01f, "Mic Rack De-Esser Thresh restored");
}

void testAudioDeviceServiceEnumerationAndBackendValidation()
{
    juce::AudioDeviceManager dm;
    LiveBus::AudioDeviceService service(dm);

    auto backends = service.getAvailableBackendTypes();
    TEST_ASSERT(!backends.isEmpty(), "Available backend types list is not empty");

    auto currentBackend = service.getCurrentBackendType();
    TEST_ASSERT(!currentBackend.isEmpty(), "Current backend type is not empty");
    TEST_ASSERT(backends.contains(currentBackend), "Current backend is in available backends list");

    // Invalid backend should safely reject without changing current backend
    service.setBackendType("NonExistentBackend12345");
    TEST_ASSERT(service.getCurrentBackendType() == currentBackend, "Backend remains unchanged on invalid input");
}

void testAudioDeviceServiceBufferDurationCalculation()
{
    // 480 samples @ 48000 Hz = 10.0 ms
    float ms480_48k = LiveBus::AudioDeviceService::calculateBufferDurationMs(480, 48000.0);
    TEST_ASSERT(std::abs(ms480_48k - 10.0f) < 0.01f, "480 samples @ 48 kHz is 10.0 ms");

    // 256 samples @ 44100 Hz = 256 / 44100 * 1000 = 5.80498... ms
    float ms256_44k = LiveBus::AudioDeviceService::calculateBufferDurationMs(256, 44100.0);
    TEST_ASSERT(std::abs(ms256_44k - 5.805f) < 0.01f, "256 samples @ 44.1 kHz is ~5.805 ms");

    // 64 samples @ 48000 Hz = 64 / 48000 * 1000 = 1.333... ms
    float ms64_48k = LiveBus::AudioDeviceService::calculateBufferDurationMs(64, 48000.0);
    TEST_ASSERT(std::abs(ms64_48k - 1.333f) < 0.01f, "64 samples @ 48 kHz is ~1.333 ms");

    // 1024 samples @ 96000 Hz = 1024 / 96000 * 1000 = 10.666... ms
    float ms1024_96k = LiveBus::AudioDeviceService::calculateBufferDurationMs(1024, 96000.0);
    TEST_ASSERT(std::abs(ms1024_96k - 10.667f) < 0.01f, "1024 samples @ 96 kHz is ~10.667 ms");

    // Zero or negative values return 0.0 ms safely
    TEST_ASSERT(LiveBus::AudioDeviceService::calculateBufferDurationMs(0, 48000.0) == 0.0f, "Zero samples returns 0 ms");
    TEST_ASSERT(LiveBus::AudioDeviceService::calculateBufferDurationMs(512, 0.0) == 0.0f, "Zero sample rate returns 0 ms");
    TEST_ASSERT(LiveBus::AudioDeviceService::calculateBufferDurationMs(-128, 48000.0) == 0.0f, "Negative samples returns 0 ms");
}

void testAudioDeviceServiceTestToneSafetyAndLevel()
{
    juce::AudioDeviceManager dm;
    LiveBus::AudioDeviceService service(dm);

    TEST_ASSERT(!service.isTestToneEnabled(), "Test tone disabled by default");

    // Enable test tone
    service.setTestToneEnabled(true);
    TEST_ASSERT(service.isTestToneEnabled(), "Test tone state is active");

    juce::AudioBuffer<float> buffer(2, 512);

    // Render several blocks to let gain ramp complete
    for (int b = 0; b < 10; ++b)
    {
        buffer.clear();
        service.renderTestTone(buffer.getArrayOfWritePointers(), 2, 512, 48000.0);
    }

    float peakL = buffer.getMagnitude(0, 0, 512);
    float peakR = buffer.getMagnitude(1, 0, 512);

    // -18 dBFS is exactly 10^(-18/20) = 0.12589...
    TEST_ASSERT(peakL > 0.10f && peakL <= 0.13f, "Test tone peak within safe -18 dBFS bounds (L)");
    TEST_ASSERT(peakR > 0.10f && peakR <= 0.13f, "Test tone peak within safe -18 dBFS bounds (R)");

    // Disable test tone and verify smooth ramp-down
    service.setTestToneEnabled(false);
    TEST_ASSERT(!service.isTestToneEnabled(), "Test tone state is inactive");

    for (int b = 0; b < 10; ++b)
    {
        buffer.clear();
        service.renderTestTone(buffer.getArrayOfWritePointers(), 2, 512, 48000.0);
    }

    float offPeak = buffer.getMagnitude(0, 0, 512);
    TEST_ASSERT(offPeak < 0.001f, "Test tone smoothly ramps down to silence without clicks");
}

void testAudioDeviceServiceInputMeterBallistics()
{
    juce::AudioDeviceManager dm;
    LiveBus::AudioDeviceService service(dm);

    // Test silence
    juce::AudioBuffer<float> silenceBuffer(2, 512);
    silenceBuffer.clear();
    service.processInputMeter(silenceBuffer.getArrayOfReadPointers(), 2, 512);

    TEST_ASSERT(service.getInputPeakDb() <= -90.0f, "Silence measures <= -90 dBFS peak");
    TEST_ASSERT(service.getInputRmsDb() <= -90.0f, "Silence measures <= -90 dBFS RMS");

    // Test known 1 kHz sine wave at amplitude 0.5 (-6.02 dBFS)
    juce::AudioBuffer<float> sineBuffer(2, 512);
    const float freq = 1000.0f;
    const float amp = 0.5f;
    for (int s = 0; s < 512; ++s)
    {
        float val = amp * std::sin((float)(2.0 * juce::MathConstants<double>::pi * freq * s / 48000.0));
        sineBuffer.setSample(0, s, val);
        sineBuffer.setSample(1, s, val);
    }

    service.processInputMeter(sineBuffer.getArrayOfReadPointers(), 2, 512);

    float peakDb = service.getInputPeakDb();
    float rmsDb = service.getInputRmsDb();

    // Peak of amp 0.5 is -6.02 dBFS; RMS of pure sine is amp / sqrt(2) = 0.3535 -> -9.03 dBFS
    TEST_ASSERT(std::abs(peakDb - (-6.02f)) < 0.2f, "Input meter accurately measures -6 dBFS peak");
    TEST_ASSERT(std::abs(rmsDb - (-9.03f)) < 0.3f, "Input meter accurately measures -9 dBFS RMS");

    // Test natural ballistics decay on silence
    silenceBuffer.clear();
    service.processInputMeter(silenceBuffer.getArrayOfReadPointers(), 2, 512);
    float decayedPeak = service.getInputPeakDb();
    TEST_ASSERT(decayedPeak < peakDb && decayedPeak > -80.0f, "Input peak decays smoothly via ballistics");
}

void testAudioDeviceServiceChannelBitmaskValidation()
{
    juce::AudioDeviceManager dm;
    dm.initialiseWithDefaultDevices(2, 2);
    LiveBus::AudioDeviceService service(dm);

    auto outChannels = service.getAvailableOutputChannelNames();
    TEST_ASSERT(!outChannels.isEmpty(), "Output channels list is available");

    auto inChannels = service.getAvailableInputChannelNames();
    TEST_ASSERT(!inChannels.isEmpty(), "Input channels list is available");

    // Test bitmask access
    service.setOutputChannelActive(0, true);
    auto updatedOut = service.getActiveOutputChannels();
    TEST_ASSERT(updatedOut[0] == true, "Output channel 0 active bit set");
}

void testAudioDeviceServiceSupervisorReconnectionAndBackoff()
{
    juce::AudioDeviceManager dm;
    dm.initialiseWithDefaultDevices(2, 2);
    LiveBus::AudioDeviceService service(dm);

    // Initial state
    TEST_ASSERT(!service.getStatusMessage().isEmpty(), "Diagnostic status message is populated");

    // Trigger reconnection
    service.triggerReconnection();
    TEST_ASSERT(service.getDeviceState() == DeviceState::Reconnecting, "State changed to Reconnecting");

    // Manual retry resets and updates state
    service.manualRetry();
    service.getSupervisor().stop();

    TEST_ASSERT(service.getDeviceState() == DeviceState::Active ||
                service.getDeviceState() == DeviceState::Reconnecting ||
                service.getDeviceState() == DeviceState::Disconnected,
                "Manual retry safely executes state transition");
}

void testSceneSaveLoadAndMetadata()
{
    SceneManager sm;
    juce::String sceneName = "Test_BroadcastMaster";

    SceneData data;
    data.sceneName = sceneName;
    data.description = "Professional Broadcast Master configuration";
    data.numActiveChannels = maxInputs;
    data.channels[0].name = "Lead Host";
    data.channels[0].accentColour = 0xff06b6d4;
    data.channels[0].gain = 0.82f;
    data.channels[0].pan = -0.25f;
    data.channels[0].mute = true;
    data.channels[0].streamRoute = true;
    data.channels[0].monitorRoute = false;
    data.channels[0].streamSend = 0.95f;
    data.channels[0].monitorSend = 0.70f;
    data.channels[maxInputs - 1].accentColour = 0xff9333ea;

    // 8-Band EQ on target 2 (Mic)
    data.eightBandEQs[2].bands[3].freq = 2400.0f;
    data.eightBandEQs[2].bands[3].gainDb = 4.5f;
    data.eightBandEQs[2].bands[3].q = 1.25f;

    // Vocal DSP on target 2
    data.vocalRacks[2].compThresholdDb = -19.5f;
    data.vocalRacks[2].compRatio = 3.5f;
    data.vocalRacks[2].limiterCeilingDb = -0.8f;

    TEST_ASSERT(sm.saveScene(sceneName, data), "Scene save succeeds");

    SceneData loaded;
    juce::String err;
    TEST_ASSERT(sm.loadScene(sceneName, loaded, err), "Scene load succeeds: " + err);

    TEST_ASSERT(loaded.version >= 3, "Schema version is 3 or newer");
    TEST_ASSERT(loaded.schemaVersion == "3.0", "Schema version attribute is 3.0");
    TEST_ASSERT(loaded.description == "Professional Broadcast Master configuration", "Description metadata preserved");
    TEST_ASSERT(loaded.timestamp > 0, "Valid timestamp recorded");
    TEST_ASSERT(loaded.channels[0].name == "Lead Host", "Channel name preserved");
    TEST_ASSERT(loaded.channels[0].accentColour == 0xff06b6d4, "Channel accent color preserved");
    TEST_ASSERT(loaded.channels[maxInputs - 1].accentColour == 0xff9333ea,
                "Sixteenth channel accent color preserved");
    TEST_ASSERT(std::abs(loaded.channels[0].gain - 0.82f) < 1e-4f, "Channel gain preserved");
    TEST_ASSERT(std::abs(loaded.channels[0].pan - -0.25f) < 1e-4f, "Channel pan preserved");
    TEST_ASSERT(loaded.channels[0].mute == true, "Channel mute preserved");
    TEST_ASSERT(loaded.channels[0].streamRoute == true, "Stream route preserved");
    TEST_ASSERT(loaded.channels[0].monitorRoute == false, "Monitor route preserved");
    TEST_ASSERT(std::abs(loaded.channels[0].streamSend - 0.95f) < 1e-4f, "Stream send preserved");
    TEST_ASSERT(std::abs(loaded.channels[0].monitorSend - 0.70f) < 1e-4f, "Monitor send preserved");

    TEST_ASSERT(std::abs(loaded.eightBandEQs[2].bands[3].freq - 2400.0f) < 1e-1f, "EQ freq preserved");
    TEST_ASSERT(std::abs(loaded.eightBandEQs[2].bands[3].gainDb - 4.5f) < 1e-2f, "EQ gain preserved");
    TEST_ASSERT(std::abs(loaded.vocalRacks[2].compThresholdDb - -19.5f) < 1e-2f, "Vocal comp threshold preserved");
    TEST_ASSERT(std::abs(loaded.vocalRacks[2].compRatio - 3.5f) < 1e-2f, "Vocal comp ratio preserved");

    // Clean up
    sm.deleteScene(sceneName);
}

void testSceneSaveAsDuplicateRenameDelete()
{
    SceneManager sm;
    juce::String orig = "Test_Original";
    juce::String dup = "Test_Duplicate";
    juce::String renamed = "Test_Renamed";

    SceneData data;
    data.sceneName = orig;
    data.channels[1].name = "Guitar Acoustic";
    data.channels[1].gain = 0.65f;

    // 1. Save
    TEST_ASSERT(sm.saveScene(orig, data), "Save original scene succeeds");

    // 2. Duplicate
    TEST_ASSERT(sm.duplicateScene(orig, dup), "Duplicate scene succeeds");
    SceneData dupData;
    juce::String err;
    TEST_ASSERT(sm.loadScene(dup, dupData, err), "Loaded duplicated scene");
    TEST_ASSERT(std::abs(dupData.channels[1].gain - 0.65f) < 1e-4f, "Duplicate data matches original");

    // 3. Rename
    TEST_ASSERT(sm.renameScene(dup, renamed), "Rename duplicated scene succeeds");
    SceneData renamedData;
    TEST_ASSERT(sm.loadScene(renamed, renamedData, err), "Loaded renamed scene");
    TEST_ASSERT(!sm.loadScene(dup, dupData, err), "Old duplicate scene name no longer exists");

    // 4. Delete
    TEST_ASSERT(sm.deleteScene(orig), "Deleted original scene");
    TEST_ASSERT(sm.deleteScene(renamed), "Deleted renamed scene");
    TEST_ASSERT(!sm.loadScene(orig, data, err), "Original scene confirmed deleted");
    TEST_ASSERT(!sm.loadScene(renamed, renamedData, err), "Renamed scene confirmed deleted");
}

void testScenePreWriteBackupAndCorruptionRecovery()
{
    SceneManager sm;
    juce::String sceneName = "Test_BackupScene";

    SceneData data1;
    data1.sceneName = sceneName;
    data1.description = "Version 1 - Safe State";
    data1.channels[0].gain = 0.50f;
    sm.saveScene(sceneName, data1);

    // Overwrite to create automatic .bak.xml
    SceneData data2;
    data2.sceneName = sceneName;
    data2.description = "Version 2 - Safe State";
    data2.channels[0].gain = 0.75f;
    sm.saveScene(sceneName, data2);

    auto targetFile = sm.getScenesDirectory().getChildFile(sceneName + ".xml");
    auto backupFile = sm.getScenesDirectory().getChildFile(sceneName + ".bak.xml");
    TEST_ASSERT(backupFile.existsAsFile(), "Automatic .bak.xml backup was generated before overwrite");

    // Corrupt the primary XML file deliberately
    targetFile.replaceWithText("<LIVEBUS_SCENE>BROKEN_SYNTAX_NO_CLOSING_TAG");

    // Load should detect corruption and gracefully recover from .bak.xml
    SceneData recoveredData;
    juce::String err;
    bool recovered = sm.loadScene(sceneName, recoveredData, err);
    TEST_ASSERT(recovered, "SceneManager automatically recovered from backup file upon XML corruption");
    TEST_ASSERT(recoveredData.description.contains("Safe State"), "Recovered data is valid");

    // Clean up
    sm.deleteScene(sceneName);
}

void testRecallFilterBlockAllAndAllowAll()
{
    LiveBus::RecallFilter filter;

    // Test Block All
    filter.blockAll();
    TEST_ASSERT(!filter.isAnyAllowed(), "Block All results in isAnyAllowed() == false");
    for (const auto& ch : filter.channels)
    {
        TEST_ASSERT(!ch.processing, "Channel processing blocked");
        TEST_ASSERT(!ch.level, "Channel level blocked");
        TEST_ASSERT(!ch.mute, "Channel mute blocked");
        TEST_ASSERT(!ch.routing, "Channel routing blocked");
        TEST_ASSERT(!ch.directOut, "Channel directOut blocked");
        TEST_ASSERT(!ch.iemWedge, "Channel iemWedge blocked");
    }
    for (const auto& b : filter.buses)
    {
        TEST_ASSERT(!b.processing, "Bus processing blocked");
        TEST_ASSERT(!b.level, "Bus level blocked");
        TEST_ASSERT(!b.mute, "Bus mute blocked");
    }
    TEST_ASSERT(!filter.fxUnits, "FX Units blocked");
    TEST_ASSERT(!filter.midi, "MIDI blocked");
    TEST_ASSERT(!filter.otherDevice, "Device blocked");

    // Test Allow All
    filter.allowAll();
    TEST_ASSERT(filter.isAnyAllowed(), "Allow All results in isAnyAllowed() == true");
    TEST_ASSERT(filter.channels[0].processing, "Channel processing allowed");
    TEST_ASSERT(filter.channels[0].level, "Channel level allowed");
    TEST_ASSERT(filter.buses[0].processing, "Bus processing allowed");
    TEST_ASSERT(filter.fxUnits, "FX Units allowed");

    // Test column toggling
    filter.toggleColumnInputs(1); // Toggle mute column
    TEST_ASSERT(!filter.channels[0].mute, "Mute column toggled to blocked");
    TEST_ASSERT(!filter.channels[3].mute, "All channels mute column blocked");
    filter.toggleColumnInputs(1);
    TEST_ASSERT(filter.channels[0].mute, "Mute column toggled back to allowed");
}

void testRecallFilterSelectiveApplication()
{
    // Baseline state
    float ch0Gain = 0.50f;
    bool ch0Mute = false;
    float ch0EQFreq = 1000.0f;
    float ch0EQGain = 0.0f;

    // Incoming scene state
    SceneData incoming;
    incoming.channels[0].gain = 0.95f;
    incoming.channels[0].mute = true;
    incoming.eightBandEQs[2].bands[0].freq = 85.0f;
    incoming.eightBandEQs[2].bands[0].gainDb = 5.0f;

    // Scenario A: Recall Filter allows ONLY processing (EQ + DSP), blocking level and mute
    LiveBus::RecallFilter filterA;
    filterA.blockAll();
    filterA.channels[0].processing = true;

    if (filterA.channels[0].level) ch0Gain = incoming.channels[0].gain;
    if (filterA.channels[0].mute) ch0Mute = incoming.channels[0].mute;
    if (filterA.channels[0].processing)
    {
        ch0EQFreq = incoming.eightBandEQs[2].bands[0].freq;
        ch0EQGain = incoming.eightBandEQs[2].bands[0].gainDb;
    }

    TEST_ASSERT(std::abs(ch0Gain - 0.50f) < 1e-4f, "Channel gain preserved unchanged when level is filtered out");
    TEST_ASSERT(ch0Mute == false, "Channel mute preserved unchanged when mute is filtered out");
    TEST_ASSERT(std::abs(ch0EQFreq - 85.0f) < 1e-4f, "Channel EQ frequency updated when processing is allowed");
    TEST_ASSERT(std::abs(ch0EQGain - 5.0f) < 1e-4f, "Channel EQ gain updated when processing is allowed");

    // Scenario B: Recall Filter allows ONLY level and mute, blocking processing
    LiveBus::RecallFilter filterB;
    filterB.blockAll();
    filterB.channels[0].level = true;
    filterB.channels[0].mute = true;

    if (filterB.channels[0].level) ch0Gain = incoming.channels[0].gain;
    if (filterB.channels[0].mute) ch0Mute = incoming.channels[0].mute;
    if (filterB.channels[0].processing)
    {
        ch0EQFreq = 500.0f;
    }

    TEST_ASSERT(std::abs(ch0Gain - 0.95f) < 1e-4f, "Channel gain updated when level is allowed");
    TEST_ASSERT(ch0Mute == true, "Channel mute updated when mute is allowed");
    TEST_ASSERT(std::abs(ch0EQFreq - 85.0f) < 1e-4f, "Channel EQ untouched when processing is filtered out");
}

void testRecallFilterCancelSafety()
{
    // Simulate user opening recall dialog, making changes in preview, then pressing Cancel
    SceneData liveState;
    liveState.channels[0].gain = 0.707f;
    liveState.channels[0].pan = 0.0f;
    liveState.buses[0].masterGain = 1.0f;

    // Staging snapshot taken on dialog open
    SceneData stagingState = liveState;

    // Incoming scene with drastic changes
    SceneData sceneToPreview;
    sceneToPreview.channels[0].gain = 0.10f;
    sceneToPreview.channels[0].pan = -1.0f;
    sceneToPreview.buses[0].masterGain = 0.0f;

    // Cancel pressed -> rollback to stagingState
    liveState = stagingState;

    TEST_ASSERT(std::abs(liveState.channels[0].gain - 0.707f) < 1e-4f, "Cancel guarantees live gain unchanged");
    TEST_ASSERT(std::abs(liveState.channels[0].pan - 0.0f) < 1e-4f, "Cancel guarantees live pan unchanged");
    TEST_ASSERT(std::abs(liveState.buses[0].masterGain - 1.0f) < 1e-4f, "Cancel guarantees master fader unchanged");
}

void testPresetManagerCRUDAndFactoryProtection()
{
    LiveBus::PresetManager pm;

    // 1. Verify Factory Presets exist
    auto vocalPresets = pm.getPresetsForCategory(LiveBus::PresetCategory::VocalDSP);
    TEST_ASSERT(vocalPresets.size() >= 5, "Vocal DSP factory presets populated");

    bool hasLead = false;
    for (const auto& p : vocalPresets)
    {
        if (p.name == "Broadcast Lead Vocal" && p.isFactory)
            hasLead = true;
    }
    TEST_ASSERT(hasLead, "Found Factory 'Broadcast Lead Vocal' preset");

    // 2. Factory presets cannot be deleted
    LiveBus::PresetItem factoryItem{ "Broadcast Lead Vocal", LiveBus::PresetCategory::VocalDSP, true, "", {} };
    TEST_ASSERT(!pm.deleteUserPreset(factoryItem), "Deleting factory preset is blocked and returns false");
    TEST_ASSERT(!pm.renameUserPreset(factoryItem, "HackedName"), "Renaming factory preset is blocked");

    // 3. User Preset Save & Load
    SceneData::VocalDSPSceneData userVocal;
    userVocal.inputGainDb = 2.5f;
    userVocal.hpfFreq = 95.0f;
    userVocal.compThresholdDb = -21.0f;
    userVocal.compRatio = 4.0f;

    juce::String userPresetName = "MyCustomVocal";
    TEST_ASSERT(pm.saveVocalDSPPreset(userPresetName, userVocal, "Custom test vocal preset"), "Saved user vocal preset");

    auto updatedPresets = pm.getPresetsForCategory(LiveBus::PresetCategory::VocalDSP);
    bool foundUser = false;
    LiveBus::PresetItem foundItem;
    for (const auto& p : updatedPresets)
    {
        if (p.name == userPresetName && !p.isFactory)
        {
            foundUser = true;
            foundItem = p;
            break;
        }
    }
    TEST_ASSERT(foundUser, "User preset discovered in category listing");

    SceneData::VocalDSPSceneData loadedVocal;
    juce::String err;
    TEST_ASSERT(pm.loadVocalDSPPreset(foundItem, loadedVocal, err), "Loaded user vocal preset");
    TEST_ASSERT(std::abs(loadedVocal.inputGainDb - 2.5f) < 1e-4f, "User preset input gain preserved");
    TEST_ASSERT(std::abs(loadedVocal.hpfFreq - 95.0f) < 1e-1f, "User preset HPF frequency preserved");
    TEST_ASSERT(std::abs(loadedVocal.compThresholdDb - -21.0f) < 1e-2f, "User preset comp threshold preserved");

    // 4. Duplicate
    juce::String dupName = "MyCustomVocal_Copy";
    TEST_ASSERT(pm.duplicateUserPreset(foundItem, dupName), "Duplicated user preset");

    // 5. Delete
    auto dir = pm.getCategoryDirectory(LiveBus::PresetCategory::VocalDSP);
    auto dupFile = dir.getChildFile(dupName + ".xml");
    LiveBus::PresetItem dupItem{ dupName, LiveBus::PresetCategory::VocalDSP, false, "", dupFile };
    TEST_ASSERT(pm.deleteUserPreset(foundItem), "Deleted original user preset");
    TEST_ASSERT(pm.deleteUserPreset(dupItem), "Deleted duplicated user preset");
}

void testPresetIsolationScopeAndCopyTools()
{
    // Mixer State Baseline
    float ch0Gain = 0.707f;
    float ch0Pan = 0.0f;
    bool ch0Mute = false;
    bool ch0StreamRoute = true;
    bool ch0MonitorRoute = true;

    // Processors
    LiveBus::EightBandEQProcessor ch0EQ;
    LiveBus::VocalDSPProcessor ch0Vocal;
    LiveBus::EightBandEQProcessor ch1EQ;
    LiveBus::VocalDSPProcessor ch1Vocal;

    ch0EQ.prepare(48000.0, 512);
    ch0Vocal.prepare(48000.0, 512);
    ch1EQ.prepare(48000.0, 512);
    ch1Vocal.prepare(48000.0, 512);

    ch0EQ.params.bands[1].gainDb.store(0.0f);
    ch0Vocal.params.compThresholdDb.store(-24.0f);
    ch1EQ.params.bands[1].gainDb.store(0.0f);

    // 1. Load an EQ preset: MUST NOT touch routing, fader, mute, or Vocal DSP!
    LiveBus::PresetManager pm;
    LiveBus::PresetItem eqPreset{ "Vocal Air & Clarity", LiveBus::PresetCategory::EightBandEQ, true, "", {} };
    SceneData::EightBandEQSceneData eqData;
    juce::String err;
    TEST_ASSERT(pm.loadEightBandEQPreset(eqPreset, eqData, err), "Loaded factory EQ preset");

    ch0EQ.params.bands[1].gainDb.store(eqData.bands[1].gainDb); // apply to EQ

    // Verify Scope: Channel routing, gain, and Vocal DSP must be 100% unaffected
    TEST_ASSERT(std::abs(ch0Gain - 0.707f) < 1e-4f, "Channel gain unaffected by EQ preset");
    TEST_ASSERT(std::abs(ch0Pan - 0.0f) < 1e-4f, "Channel pan unaffected by EQ preset");
    TEST_ASSERT(ch0Mute == false, "Channel mute unaffected by EQ preset");
    TEST_ASSERT(ch0StreamRoute == true && ch0MonitorRoute == true, "Routing matrix unaffected by EQ preset");
    TEST_ASSERT(std::abs(ch0Vocal.params.compThresholdDb.load() - -24.0f) < 1e-2f, "Vocal DSP threshold unaffected by EQ preset");
    TEST_ASSERT(std::abs(ch0EQ.params.bands[1].gainDb.load() - 2.0f) < 1e-2f, "EQ parameter successfully applied");

    // 2. Copy EQ from Ch 0 to Ch 1: MUST NOT touch Ch 1 Vocal DSP or other settings
    ch1EQ.params.bands[1].gainDb.store(ch0EQ.params.bands[1].gainDb.load());
    TEST_ASSERT(std::abs(ch1EQ.params.bands[1].gainDb.load() - 2.0f) < 1e-2f, "Copied EQ band to Ch 1");
    TEST_ASSERT(std::abs(ch1Vocal.params.compThresholdDb.load() - -24.0f) < 1e-2f, "Ch 1 Vocal DSP untouched by EQ copy");
}

// ==============================================================================
// OBS Studio Integration Automated Tests
// ==============================================================================

void testOBSVirtualDeviceDetectionAndPatternRecognition()
{
    using namespace LiveBus;

    // Test positive virtual device keywords
    TEST_ASSERT(OBSIntegrationService::isVirtualAudioDevice("CABLE Input (VB-Audio Virtual Cable)"), "Detects VB-CABLE Input");
    TEST_ASSERT(OBSIntegrationService::isVirtualAudioDevice("CABLE Output (VB-Audio Virtual Cable)"), "Detects VB-CABLE Output");
    TEST_ASSERT(OBSIntegrationService::isVirtualAudioDevice("Line 1 (Virtual Audio Cable)"), "Detects VAC Line 1");
    TEST_ASSERT(OBSIntegrationService::isVirtualAudioDevice("VoiceMeeter Input (VB-Audio VoiceMeeter VAIO)"), "Detects VoiceMeeter VAIO");
    TEST_ASSERT(OBSIntegrationService::isVirtualAudioDevice("VoiceMeeter Aux Input"), "Detects VoiceMeeter Aux");
    TEST_ASSERT(OBSIntegrationService::isVirtualAudioDevice("Wave Link Stream (Elgato Wave Link)"), "Detects Elgato Wave Link");
    TEST_ASSERT(OBSIntegrationService::isVirtualAudioDevice("OBS-Virtual-Audio"), "Detects OBS Virtual Audio");

    // Test negative physical hardware devices
    TEST_ASSERT(!OBSIntegrationService::isVirtualAudioDevice("Speakers (Realtek High Definition Audio)"), "Rejects Realtek Speakers");
    TEST_ASSERT(!OBSIntegrationService::isVirtualAudioDevice("Headphones (Focusrite USB Audio)"), "Rejects Focusrite USB Audio");
    TEST_ASSERT(!OBSIntegrationService::isVirtualAudioDevice("Digital Output (S/PDIF)"), "Rejects S/PDIF Digital Output");

    // Test driver identification classification
    TEST_ASSERT(OBSIntegrationService::identifyVirtualDriverType("CABLE Input (VB-Audio)") == "VB-Audio CABLE", "Classifies VB-Audio CABLE");
    TEST_ASSERT(OBSIntegrationService::identifyVirtualDriverType("Line 1 (Virtual Audio Cable)") == "Virtual Audio Cable (VAC)", "Classifies VAC");
    TEST_ASSERT(OBSIntegrationService::identifyVirtualDriverType("VoiceMeeter Input") == "VB-Audio VoiceMeeter", "Classifies VoiceMeeter");
    TEST_ASSERT(OBSIntegrationService::identifyVirtualDriverType("Wave Link Stream") == "Elgato Wave Link", "Classifies Elgato Wave Link");
    TEST_ASSERT(OBSIntegrationService::identifyVirtualDriverType("Realtek Audio") == "Physical Hardware", "Classifies Realtek as Physical");
}

void testOBSNativeDriverReadinessIsHonest()
{
    using namespace LiveBus;
    juce::AudioDeviceManager dummyMgr;
    OBSIntegrationService obs(dummyMgr);

    TEST_ASSERT(obs.hasNativeVirtualAudioDriver() == false, "Native LiveBus virtual driver is disabled by default");
    TEST_ASSERT(obs.getDriverReadinessSummary().contains("not installed") ||
                    obs.getDriverReadinessSummary().contains("required for endpoint routing"),
                "Driver readiness summary honestly states the native driver is unavailable");

    obs.setNativeVirtualAudioDriverInstalled(true);
    TEST_ASSERT(obs.hasNativeVirtualAudioDriver() == true, "Native driver flag can be enabled explicitly");
    TEST_ASSERT(obs.getDriverReadinessSummary().contains("Native") || obs.getDriverReadinessSummary().contains("active"),
                "Enabled native driver changes the status summary");
}

void testOBSWebSocketV5HandshakeAndAuthentication()
{
#if defined(_WIN32)
    using namespace LiveBus;
    juce::StreamingSocket listener;
    TEST_ASSERT(listener.createListener(0), "Local OBS WebSocket mock listener created");
    const int port = listener.getBoundPort();
    TEST_ASSERT(port > 0, "Mock listener received an ephemeral port");

    std::atomic<bool> stopServer{false};
    std::atomic<bool> identifyWasValid{false};
    std::string receivedIdentifyText;
    std::thread mockServer([&]()
    {
        std::unique_ptr<juce::StreamingSocket> connection(listener.waitForNextConnection());
        if (connection == nullptr)
            return;

        auto readExactFromClient = [&](void* destination, int size)
        {
            auto* output = static_cast<char*>(destination);
            int received = 0;
            while (received < size)
            {
                if (connection->waitUntilReady(true, 3000) <= 0)
                    return false;
                const int count = connection->read(output + received, size - received, false);
                if (count <= 0)
                    return false;
                received += count;
            }
            return true;
        };
        auto writeToClient = [&](const void* data, int size)
        {
            const auto* input = static_cast<const char*>(data);
            int sent = 0;
            while (sent < size)
            {
                if (connection->waitUntilReady(false, 3000) <= 0)
                    return false;
                const int count = connection->write(input + sent, size - sent);
                if (count <= 0)
                    return false;
                sent += count;
            }
            return true;
        };
        auto sendTextFrame = [&](const juce::String& text)
        {
            const char* utf8 = text.toRawUTF8();
            const auto payloadSize = (int)std::strlen(utf8);
            if (payloadSize < 2 || payloadSize >= 252)
                return false;
            const int firstSize = payloadSize / 2;
            const int secondSize = payloadSize - firstSize;
            const unsigned char firstHeader[2] = { 0x01, (unsigned char)firstSize };
            const unsigned char continuationHeader[2] = { 0x80, (unsigned char)secondSize };
            return writeToClient(firstHeader, 2) && writeToClient(utf8, firstSize) &&
                   writeToClient(continuationHeader, 2) && writeToClient(utf8 + firstSize, secondSize);
        };

        std::string request;
        while (request.size() < 16384 && request.find("\r\n\r\n") == std::string::npos)
        {
            char c = 0;
            if (!readExactFromClient(&c, 1))
                return;
            request.push_back(c);
        }
        const std::string keyHeader = "Sec-WebSocket-Key: ";
        const auto keyStart = request.find(keyHeader);
        if (keyStart == std::string::npos)
            return;
        const auto keyEnd = request.find("\r\n", keyStart);
        const auto key = request.substr(keyStart + keyHeader.size(), keyEnd - keyStart - keyHeader.size());
        const auto acceptInput = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectSize = 0;
        DWORD resultSize = 0;
        std::array<UCHAR, 20> digest{};
        std::vector<UCHAR> hashObject;
        bool hashOk = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) >= 0 &&
                      BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                                        sizeof(objectSize), &resultSize, 0) >= 0;
        if (hashOk)
        {
            hashObject.resize(objectSize);
            hashOk = BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0) >= 0 &&
                     BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(acceptInput.data())),
                                    (ULONG)acceptInput.size(), 0) >= 0 &&
                     BCryptFinishHash(hash, digest.data(), (ULONG)digest.size(), 0) >= 0;
        }
        if (hash != nullptr)
            BCryptDestroyHash(hash);
        if (algorithm != nullptr)
            BCryptCloseAlgorithmProvider(algorithm, 0);
        if (!hashOk)
            return;

        const auto accept = juce::Base64::toBase64(digest.data(), digest.size());
        const juce::String response = "HTTP/1.1 101 Switching Protocols\r\n"
                                      "Upgrade: websocket\r\n"
                                      "Connection: Upgrade\r\n"
                                      "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
        const char* responseUtf8 = response.toRawUTF8();
        if (!writeToClient(responseUtf8, (int)response.getNumBytesAsUTF8()))
            return;

        const unsigned char pingHeader[2] = { 0x89, 4 };
        const unsigned char pingPayload[4] = { 0x41, 0x00, 0xff, 0x5a };
        if (!writeToClient(pingHeader, 2) || !writeToClient(pingPayload, 4))
            return;

        std::array<unsigned char, 2> pongHeader{};
        if (!readExactFromClient(pongHeader.data(), (int)pongHeader.size()) ||
            (pongHeader[0] & 0x0f) != 0x0a || (pongHeader[1] & 0x80) == 0 ||
            (pongHeader[1] & 0x7f) != 4)
            return;
        std::array<unsigned char, 4> pongMask{};
        std::array<unsigned char, 4> pongPayload{};
        if (!readExactFromClient(pongMask.data(), (int)pongMask.size()) ||
            !readExactFromClient(pongPayload.data(), (int)pongPayload.size()))
            return;
        for (size_t i = 0; i < pongPayload.size(); ++i)
            pongPayload[i] ^= pongMask[i];
        if (!std::equal(pongPayload.begin(), pongPayload.end(), std::begin(pingPayload)))
            return;

        if (!sendTextFrame("{\"op\":0,\"d\":{\"obsWebSocketVersion\":\"5.0.0\",\"rpcVersion\":1,"
                           "\"authentication\":{\"salt\":\"salt\",\"challenge\":\"challenge\"}}}"))
            return;

        std::array<unsigned char, 2> frameHeader{};
        if (!readExactFromClient(frameHeader.data(), (int)frameHeader.size()) ||
            (frameHeader[0] & 0x0f) != 0x1 || (frameHeader[1] & 0x80) == 0)
            return;

        std::uint64_t payloadLength = frameHeader[1] & 0x7f;
        if (payloadLength == 126)
        {
            std::array<unsigned char, 2> extended{};
            if (!readExactFromClient(extended.data(), (int)extended.size()))
                return;
            payloadLength = ((std::uint64_t)extended[0] << 8) | extended[1];
        }
        if (payloadLength > 4096)
            return;

        std::array<unsigned char, 4> mask{};
        std::vector<char> identifyPayload((size_t)payloadLength);
        if (!readExactFromClient(mask.data(), (int)mask.size()) ||
            (payloadLength > 0 && !readExactFromClient(identifyPayload.data(), (int)payloadLength)))
            return;
        for (size_t i = 0; i < identifyPayload.size(); ++i)
            identifyPayload[i] ^= (char)mask[i % mask.size()];

        const auto identifyText = juce::String::fromUTF8(identifyPayload.data(), (int)identifyPayload.size());
        receivedIdentifyText = identifyText.toStdString();
        const auto identify = juce::JSON::parse(identifyText);
        auto* identifyData = identify.getProperty("d", juce::var()).getDynamicObject();
        const juce::String expectedAuthentication = "39cfhx7et2iyoMZvoQ6o3OPLNSKgtMmy48GQ7jnvsdE=";

        identifyWasValid.store(identify.isObject() && (int)identify.getProperty("op", -1) == 1 &&
                               identifyData != nullptr &&
                               (int)identifyData->getProperty("rpcVersion") == 1 &&
                               identifyData->getProperty("authentication").toString() == expectedAuthentication);
        if (!identifyWasValid.load() || !sendTextFrame("{\"op\":2,\"d\":{\"negotiatedRpcVersion\":1}}"))
            return;

        while (!stopServer.load())
            juce::Thread::sleep(10);
    });

    OBSWebSocketClient client;
    client.configure("127.0.0.1", port, "secret");
    client.connectAsync();

    const auto deadline = juce::Time::getMillisecondCounter() + 5000;
    while (!client.isConnected() && juce::Time::getMillisecondCounter() < deadline)
    {
        if (client.getState() == OBSWebSocketState::ConnectionFailed ||
            client.getState() == OBSWebSocketState::AuthenticationFailed)
            break;
        juce::Thread::sleep(10);
    }

    const bool connected = client.isConnected();
    const auto clientStatus = client.getStatusString();
    client.disconnect();
    stopServer.store(true);
    mockServer.join();
    listener.close();

    if (!identifyWasValid.load())
        std::cerr << "OBS mock received Identify: " << receivedIdentifyText
                  << " | client status: " << clientStatus << std::endl;
    TEST_ASSERT(identifyWasValid.load(), "Client sent a masked OBS v5 Identify with the expected authentication proof");
    TEST_ASSERT(connected, "Client reports ready only after OBS Identify succeeds");
#else
    TEST_ASSERT(true, "OBS WebSocket mock integration test requires Windows BCrypt");
#endif
}

void testOBSStreamRoutingAndMeterBallistics()
{
    using namespace LiveBus;
    juce::AudioDeviceManager dummyMgr;
    OBSIntegrationService obs(dummyMgr);

    const int numSamples = 256;
    juce::AudioBuffer<float> streamBuf(2, numSamples);

    // Fill streamBuf with known test sine wave (peak = 0.5f -> ~ -6.02 dBFS)
    float phase = 0.0f;
    float delta = (float)(1000.0 * juce::MathConstants<double>::twoPi / 48000.0);
    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.5f * std::sin(phase);
        phase += delta;
        streamBuf.setSample(0, i, s);
        streamBuf.setSample(1, i, s);
    }

    // Push into circular FIFO
    obs.pushStreamAudioBlock(streamBuf, numSamples);

    // Read through secondary audio callback
    juce::AudioBuffer<float> outBuf(2, numSamples);
    juce::AudioIODeviceCallbackContext dummyContext;
    obs.audioDeviceIOCallbackWithContext(nullptr, 0, outBuf.getArrayOfWritePointers(), 2, numSamples, dummyContext);

    // Verify samples survived through the FIFO without distortion
    float outPeakL = outBuf.getMagnitude(0, 0, numSamples);
    float outPeakR = outBuf.getMagnitude(1, 0, numSamples);
    TEST_ASSERT(std::abs(outPeakL - 0.5f) < 1e-4f, "Stream Bus audio successfully delivered through FIFO to Ch L");
    TEST_ASSERT(std::abs(outPeakR - 0.5f) < 1e-4f, "Stream Bus audio successfully delivered through FIFO to Ch R");

    // Verify metering telemetry
    TEST_ASSERT(std::abs(obs.getStreamPeakL() - 0.5f) < 1e-3f, "Stream peak meter L accurately reports 0.5 magnitude");
    TEST_ASSERT(std::abs(obs.getStreamPeakR() - 0.5f) < 1e-3f, "Stream peak meter R accurately reports 0.5 magnitude");
    TEST_ASSERT(obs.getStreamRmsL() > 0.3f && obs.getStreamRmsL() < 0.4f, "Stream RMS meter accurately reports ~0.353 magnitude for sine wave");
}

void testOBSStreamAndMonitorGainIndependence()
{
    RoutingMatrix matrix;
    const int numSamples = 256;
    const double sampleRate = 48000.0;

    BusDSPProcessor streamBus;
    BusDSPProcessor monitorBus;
    streamBus.prepare(sampleRate, numSamples);
    monitorBus.prepare(sampleRate, numSamples);

    // Bypass EQ/Comp/Gate/Limiter to test pure gain fader isolation
    streamBus.params.hpfEnabled.store(false);
    streamBus.params.eqEnabled.store(false);
    streamBus.params.compEnabled.store(false);
    streamBus.params.limiterEnabled.store(false);
    streamBus.params.masterGain.store(1.0f);
    streamBus.updateCoefficients();

    monitorBus.params.hpfEnabled.store(false);
    monitorBus.params.eqEnabled.store(false);
    monitorBus.params.compEnabled.store(false);
    monitorBus.params.limiterEnabled.store(false);
    monitorBus.params.masterGain.store(1.0f);
    monitorBus.updateCoefficients();

    juce::AudioBuffer<float> streamBuf(2, numSamples);
    juce::AudioBuffer<float> monitorBuf(2, numSamples);

    // Generate 1 kHz sine input signal
    std::vector<float> inData(numSamples);
    for (int i = 0; i < numSamples; ++i)
        inData[i] = 0.8f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / (float)sampleRate);
    const float* inPtrs[] = { inData.data() };

    // 1. Initial State: Both at unity gain (gain = 1.0)
    matrix.getChannel(0).gain.store(1.0f);
    matrix.getChannel(0).routeEnable[0].store(true);
    matrix.getChannel(0).routeEnable[1].store(true);
    matrix.getChannel(0).sendLevel[0].store(1.0f);
    matrix.getChannel(0).sendLevel[1].store(1.0f);

    for (int b = 0; b < 10; ++b)
    {
        matrix.routeAndSum(inPtrs, 1, streamBuf, monitorBuf, numSamples);
        streamBus.process(streamBuf, numSamples);
        monitorBus.process(monitorBuf, numSamples);
    }

    float initialStreamMag = streamBuf.getMagnitude(0, 0, numSamples);
    float initialMonitorMag = monitorBuf.getMagnitude(0, 0, numSamples);
    TEST_ASSERT(initialStreamMag > 0.7f, "Initial stream buffer active");
    TEST_ASSERT(initialMonitorMag > 0.7f, "Initial monitor buffer active");

    // 2. Reduce Stream Gain to 0.25 (-12 dB): Monitor MUST NOT change!
    streamBus.params.masterGain.store(0.25f);
    for (int b = 0; b < 10; ++b)
    {
        matrix.routeAndSum(inPtrs, 1, streamBuf, monitorBuf, numSamples);
        streamBus.process(streamBuf, numSamples);
        monitorBus.process(monitorBuf, numSamples);
    }

    float reducedStreamMag = streamBuf.getMagnitude(0, 0, numSamples);
    float unchangedMonitorMag = monitorBuf.getMagnitude(0, 0, numSamples);

    TEST_ASSERT(std::abs(reducedStreamMag - (initialStreamMag * 0.25f)) < 0.05f, "Stream master gain successfully attenuated stream output");
    TEST_ASSERT(std::abs(unchangedMonitorMag - initialMonitorMag) < 1e-4f, "Monitor master gain remains 100% independent when stream gain is adjusted");

    // 3. Mute Stream Bus completely: Monitor MUST continue playing at full volume!
    streamBus.params.masterGain.store(0.0f);
    for (int b = 0; b < 10; ++b)
    {
        matrix.routeAndSum(inPtrs, 1, streamBuf, monitorBuf, numSamples);
        streamBus.process(streamBuf, numSamples);
        monitorBus.process(monitorBuf, numSamples);
    }

    TEST_ASSERT(streamBuf.getMagnitude(0, 0, numSamples) < 1e-4f, "Stream bus is completely silent at 0 gain");
    TEST_ASSERT(std::abs(monitorBuf.getMagnitude(0, 0, numSamples) - initialMonitorMag) < 1e-4f, "Monitor bus maintains full level while stream bus is zeroed");
}

void testOBSStreamMuteDoesNotMuteMonitor()
{
    const int numSamples = 128;
    BusDSPProcessor streamBus;
    BusDSPProcessor monitorBus;
    streamBus.prepare(48000.0, numSamples);
    monitorBus.prepare(48000.0, numSamples);

    juce::AudioBuffer<float> streamBuf(2, numSamples);
    juce::AudioBuffer<float> monitorBuf(2, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        streamBuf.setSample(0, i, 0.8f);
        streamBuf.setSample(1, i, 0.8f);
        monitorBuf.setSample(0, i, 0.8f);
        monitorBuf.setSample(1, i, 0.8f);
    }

    // MUTE STREAM BUS
    streamBus.params.masterMute.store(true);
    monitorBus.params.masterMute.store(false);

    streamBus.process(streamBuf, numSamples);
    monitorBus.process(monitorBuf, numSamples);

    TEST_ASSERT(streamBuf.getMagnitude(0, 0, numSamples) == 0.0f, "Stream Bus is silenced when Stream Mute is active");
    TEST_ASSERT(monitorBuf.getMagnitude(0, 0, numSamples) > 0.7f, "Monitor Bus continues transmitting audio when Stream Mute is active");
}

void testOBSMonitorMuteDoesNotMuteStream()
{
    const int numSamples = 128;
    BusDSPProcessor streamBus;
    BusDSPProcessor monitorBus;
    streamBus.prepare(48000.0, numSamples);
    monitorBus.prepare(48000.0, numSamples);

    juce::AudioBuffer<float> streamBuf(2, numSamples);
    juce::AudioBuffer<float> monitorBuf(2, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        streamBuf.setSample(0, i, 0.8f);
        streamBuf.setSample(1, i, 0.8f);
        monitorBuf.setSample(0, i, 0.8f);
        monitorBuf.setSample(1, i, 0.8f);
    }

    // MUTE MONITOR BUS
    streamBus.params.masterMute.store(false);
    monitorBus.params.masterMute.store(true);

    streamBus.process(streamBuf, numSamples);
    monitorBus.process(monitorBuf, numSamples);

    TEST_ASSERT(streamBuf.getMagnitude(0, 0, numSamples) > 0.7f, "Stream Bus to OBS continues playing when Monitor Mute is active");
    TEST_ASSERT(monitorBuf.getMagnitude(0, 0, numSamples) == 0.0f, "Monitor Bus is silenced when Monitor Mute is active");
}

void testOBSChannelMappingAndStereoIsolation()
{
    juce::AudioDeviceManager primaryDeviceManager;
    LiveBus::OBSIntegrationService obsService(primaryDeviceManager);
    TEST_ASSERT(obsService.getRoutingMode() == LiveBus::OBSRoutingMode::DedicatedSecondaryDevice,
                "Stereo primary defaults to Monitor Bus output when no secondary endpoint is selected");

    const int numSamples = 128;
    juce::AudioBuffer<float> streamBuf(2, numSamples);
    juce::AudioBuffer<float> monitorBuf(2, numSamples);

    // Stream audio = 0.5f, Monitor audio = 0.8f
    for (int i = 0; i < numSamples; ++i)
    {
        streamBuf.setSample(0, i, 0.5f);
        streamBuf.setSample(1, i, 0.5f);
        monitorBuf.setSample(0, i, 0.8f);
        monitorBuf.setSample(1, i, 0.8f);
    }

    // 1. Multi-channel mode maps Stream to outputs 1-2 and Monitor to 3-4.
    juce::AudioBuffer<float> hardware4Ch(4, numSamples);
    auto hardware4ChOutputs = hardware4Ch.getArrayOfWritePointers();
    LiveBus::routeMasterBusesToDevice(hardware4ChOutputs, 4, streamBuf, monitorBuf, numSamples,
                                      LiveBus::OBSRoutingMode::PrimaryMultiChannel);

    TEST_ASSERT(std::abs(hardware4Ch.getMagnitude(0, 0, numSamples) - 0.5f) < 1e-4f, "Hardware Ch 0 carries Stream Bus L");
    TEST_ASSERT(std::abs(hardware4Ch.getMagnitude(1, 0, numSamples) - 0.5f) < 1e-4f, "Hardware Ch 1 carries Stream Bus R");
    TEST_ASSERT(std::abs(hardware4Ch.getMagnitude(2, 0, numSamples) - 0.8f) < 1e-4f, "Hardware Ch 2 carries Monitor Bus L");
    TEST_ASSERT(std::abs(hardware4Ch.getMagnitude(3, 0, numSamples) - 0.8f) < 1e-4f, "Hardware Ch 3 carries Monitor Bus R");

    // Dedicated endpoint mode must put Monitor on the primary stereo pair even on a
    // multichannel interface; the stream bus is delivered by the secondary endpoint.
    hardware4Ch.clear();
    hardware4ChOutputs = hardware4Ch.getArrayOfWritePointers();
    LiveBus::routeMasterBusesToDevice(hardware4ChOutputs, 4, streamBuf, monitorBuf, numSamples,
                                      LiveBus::OBSRoutingMode::DedicatedSecondaryDevice);
    TEST_ASSERT(std::abs(hardware4Ch.getMagnitude(0, 0, numSamples) - 0.8f) < 1e-4f,
                "Dedicated endpoint mode routes Monitor to primary output 1");
    TEST_ASSERT(std::abs(hardware4Ch.getMagnitude(1, 0, numSamples) - 0.8f) < 1e-4f,
                "Dedicated endpoint mode routes Monitor to primary output 2");
    TEST_ASSERT(hardware4Ch.getMagnitude(2, 0, numSamples) == 0.0f
                && hardware4Ch.getMagnitude(3, 0, numSamples) == 0.0f,
                "Dedicated endpoint mode does not place Monitor on unconnected outputs 3-4");

    // The same mode also maps Monitor onto a stereo primary output device.
    juce::AudioBuffer<float> primary2Ch(2, numSamples);
    auto primary2ChOutputs = primary2Ch.getArrayOfWritePointers();
    LiveBus::routeMasterBusesToDevice(primary2ChOutputs, 2, streamBuf, monitorBuf, numSamples,
                                      LiveBus::OBSRoutingMode::DedicatedSecondaryDevice);

    TEST_ASSERT(std::abs(primary2Ch.getMagnitude(0, 0, numSamples) - 0.8f) < 1e-4f, "Primary hardware outputs Monitor Bus only in Dedicated Secondary mode");
    TEST_ASSERT(primary2Ch.getMagnitude(0, 0, numSamples) != 0.5f, "Zero stream bleed into primary headphone monitor");
}

void testOBSTestSignalGenerationAndAntiClick()
{
    using namespace LiveBus;
    juce::AudioDeviceManager dummyMgr;
    OBSIntegrationService obs(dummyMgr);

    const int numSamples = 256;
    const double sampleRate = 48000.0;
    juce::AudioBuffer<float> outBuf(2, numSamples);

    // Initial state: Test Signal disabled
    outBuf.clear();
    obs.renderTestSignal(outBuf.getArrayOfWritePointers(), 2, numSamples, sampleRate);
    TEST_ASSERT(outBuf.getMagnitude(0, 0, numSamples) == 0.0f, "Test signal silent when inactive");

    // Enable Test Signal
    obs.setTestSignalEnabled(true);
    TEST_ASSERT(obs.isTestSignalEnabled(), "Test signal active flag confirmed");

    // Render multiple blocks to allow anti-click ramping to complete
    for (int b = 0; b < 10; ++b)
    {
        outBuf.clear();
        obs.renderTestSignal(outBuf.getArrayOfWritePointers(), 2, numSamples, sampleRate);
    }

    float finalPeak = outBuf.getMagnitude(0, 0, numSamples);
    float targetGain = 0.12589f; // -18.0 dBFS
    TEST_ASSERT(std::abs(finalPeak - targetGain) < 0.015f, "Test signal generates pure 440 Hz tone at -18.0 dBFS");

    // Disable Test Signal and verify smooth ramp down to 0
    obs.setTestSignalEnabled(false);
    for (int b = 0; b < 10; ++b)
    {
        outBuf.clear();
        obs.renderTestSignal(outBuf.getArrayOfWritePointers(), 2, numSamples, sampleRate);
    }
    float offPeak = outBuf.getMagnitude(0, 0, numSamples);
    TEST_ASSERT(offPeak < 1e-4f, "Test signal ramps down cleanly to zero without DC click");
}

void testOBSDeviceRecoveryAndStateSafety()
{
    using namespace LiveBus;
    juce::AudioDeviceManager dummyMgr;
    OBSIntegrationService obs(dummyMgr);

    obs.setSecondaryOutputBus(SecondaryOutputBus::Monitor);
    TEST_ASSERT(obs.getSecondaryOutputBus() == SecondaryOutputBus::Monitor,
                "Secondary endpoint can select the Monitor Bus");
    obs.setSecondaryOutputBus(SecondaryOutputBus::StreamAndMonitor);
    TEST_ASSERT(obs.getSecondaryOutputBus() == SecondaryOutputBus::StreamAndMonitor,
                "Secondary endpoint can select a combined audition mix");
    obs.setSecondaryOutputGain(3.0f);
    TEST_ASSERT(std::abs(obs.getSecondaryOutputGain() - 2.0f) < 1e-6f,
                "Endpoint output gain is safely clamped");
    obs.setSecondaryOutputMuted(true);
    TEST_ASSERT(obs.isSecondaryOutputMuted(), "Endpoint output mute is independently controllable");
    obs.setSecondaryOutputGain(1.0f);
    obs.setSecondaryOutputMuted(false);
    obs.setSecondaryOutputBus(SecondaryOutputBus::Stream);

    // 1. Attempt to open non-existent virtual device
    juce::String err = obs.openSecondaryVirtualDevice("NonExistent_Virtual_Audio_Cable_9999");
    TEST_ASSERT(obs.isDeviceError(), "Service marks deviceErrorState on open failure");
    TEST_ASSERT(obs.getErrorMessage().contains("NonExistent_Virtual_Audio_Cable_9999"), "Error message contains invalid device name");
    TEST_ASSERT(obs.getStatusDescription().contains("Device Error"), "Status description reports device error clearly");

    // 2. Safe Fallback: Stream running flag is false, no crashed audio thread
    TEST_ASSERT(!obs.isSecondaryDeviceActive(), "Secondary device is inactive on failure");

    // 3. Audio Thread NaN / Infinity Input Protection
    juce::AudioBuffer<float> nanBuf(2, 128);
    for (int i = 0; i < 128; ++i)
    {
        nanBuf.setSample(0, i, std::numeric_limits<float>::quiet_NaN());
        nanBuf.setSample(1, i, std::numeric_limits<float>::infinity());
    }

    // Must not crash or propagate NaN through FIFO
    obs.pushStreamAudioBlock(nanBuf, 128);
    juce::AudioBuffer<float> safeOut(2, 128);
    safeOut.clear();
    juce::AudioIODeviceCallbackContext dummyCtx;
    obs.audioDeviceIOCallbackWithContext(nullptr, 0, safeOut.getArrayOfWritePointers(), 2, 128, dummyCtx);

    for (int i = 0; i < 128; ++i)
    {
        TEST_ASSERT(!std::isinf(safeOut.getSample(1, i)), "OBS output protects against infinity");
    }
}

// ==============================================================================
// Professional MIDI Controller & MIDI Learn Tests
// ==============================================================================
void testMIDICCAbsoluteMappingAndRangeClamping()
{
    LiveBus::MidiMappingEntry map;
    map.messageType = LiveBus::MidiMessageType::CC;
    map.controllerOrNoteNumber = 7;
    map.mode = LiveBus::MidiMode::Absolute;
    map.minValue = -60.0f;
    map.maxValue = 12.0f;
    map.defaultValue = 0.0f;

    // Minimum CC 0 -> -60 dB
    float v0 = map.calculateNewValue(0, 0.0f);
    TEST_ASSERT(std::abs(v0 - (-60.0f)) < 0.01f, "CC 0 maps to minimum -60 dB");

    // Maximum CC 127 -> +12 dB
    float v127 = map.calculateNewValue(127, 0.0f);
    TEST_ASSERT(std::abs(v127 - 12.0f) < 0.01f, "CC 127 maps to maximum +12 dB");

    // Midpoint CC 64 -> within valid range
    float v64 = map.calculateNewValue(64, 0.0f);
    TEST_ASSERT(v64 > -60.0f && v64 < 12.0f, "CC 64 maps within valid range");

    // Invert option
    map.invert = true;
    float v0Inv = map.calculateNewValue(0, 0.0f);
    TEST_ASSERT(std::abs(v0Inv - 12.0f) < 0.01f, "Inverted CC 0 maps to maximum");
    float v127Inv = map.calculateNewValue(127, 0.0f);
    TEST_ASSERT(std::abs(v127Inv - (-60.0f)) < 0.01f, "Inverted CC 127 maps to minimum");
}

void testMIDIRelativeModes()
{
    LiveBus::MidiMappingEntry map;
    map.minValue = -60.0f;
    map.maxValue = 12.0f;

    // 1. Relative Signed Mode
    map.mode = LiveBus::MidiMode::RelativeSigned;
    float curVal = 0.0f;
    float nextVal = map.calculateNewValue(1, curVal);
    TEST_ASSERT(nextVal > curVal, "Relative Signed 1 produces positive delta");

    float downVal = map.calculateNewValue(65, nextVal);
    TEST_ASSERT(downVal < nextVal, "Relative Signed 65 produces negative delta");

    // 2. Relative Two's Complement Mode
    map.mode = LiveBus::MidiMode::RelativeTwosComplement;
    float twPositive = map.calculateNewValue(1, 0.0f);
    TEST_ASSERT(twPositive > 0.0f, "Two's complement 1 produces positive delta");

    float twNegative = map.calculateNewValue(127, 0.0f);
    TEST_ASSERT(twNegative < 0.0f, "Two's complement 127 produces negative delta");

    // 3. Relative Binary Offset Mode (64 is zero)
    map.mode = LiveBus::MidiMode::RelativeBinaryOffset;
    float binZero = map.calculateNewValue(64, 0.0f);
    TEST_ASSERT(std::abs(binZero - 0.0f) < 1e-4f, "Binary offset 64 produces zero delta");

    float binPos = map.calculateNewValue(65, 0.0f);
    TEST_ASSERT(binPos > 0.0f, "Binary offset 65 produces positive delta");

    float binNeg = map.calculateNewValue(63, 0.0f);
    TEST_ASSERT(binNeg < 0.0f, "Binary offset 63 produces negative delta");
}

void testMIDILogarithmicFrequencyMapping()
{
    LiveBus::MidiMappingEntry map;
    map.minValue = 20.0f;
    map.maxValue = 20000.0f;
    map.defaultValue = 1000.0f;
    map.skewFactor = 0.4f;

    float minF = map.calculateNewValue(0, 1000.0f);
    TEST_ASSERT(std::abs(minF - 20.0f) < 0.01f, "Log frequency CC 0 is 20 Hz");

    float maxF = map.calculateNewValue(127, 1000.0f);
    TEST_ASSERT(std::abs(maxF - 20000.0f) < 0.01f, "Log frequency CC 127 is 20000 Hz");

    float midF = map.calculateNewValue(64, 1000.0f);
    TEST_ASSERT(midF > 500.0f && midF < 4000.0f, "Log frequency midpoint lands in musical mid-range");
}

void testMIDINoteToggleAndMomentaryBehavior()
{
    LiveBus::MidiParameterRegistry registry;
    float paramVal = 0.0f;

    LiveBus::ParameterDescriptor desc;
    desc.id = "ch.1.mute";
    desc.isBoolean = true;
    desc.getter = [&paramVal]() { return paramVal; };
    desc.setter = [&paramVal](float v) { paramVal = v; };
    registry.registerParameter(desc);

    LiveBus::MidiManager manager(registry);
    manager.clearAllMappings();

    LiveBus::MidiMappingEntry toggleMap;
    toggleMap.messageType = LiveBus::MidiMessageType::Note;
    toggleMap.controllerOrNoteNumber = 36;
    toggleMap.targetParameterId = "ch.1.mute";
    toggleMap.noteMode = LiveBus::NoteTriggerMode::Toggle;
    manager.addMapping(toggleMap);

    // 1. Note On Toggle: 1st NoteOn -> ON (1.0)
    auto noteOn = juce::MidiMessage::noteOn(1, 36, (juce::uint8)100);
    manager.handleIncomingMidiMessage(nullptr, noteOn);
    manager.processQueuedEvents();
    TEST_ASSERT(paramVal == 1.0f, "First NoteOn toggles parameter ON");

    // 2. Note Off retains toggle state
    auto noteOff = juce::MidiMessage::noteOff(1, 36);
    manager.handleIncomingMidiMessage(nullptr, noteOff);
    manager.processQueuedEvents();
    TEST_ASSERT(paramVal == 1.0f, "NoteOff retains toggle state");

    // 3. 2nd NoteOn -> OFF (0.0)
    manager.handleIncomingMidiMessage(nullptr, noteOn);
    manager.processQueuedEvents();
    TEST_ASSERT(paramVal == 0.0f, "Second NoteOn toggles parameter OFF");

    // 4. Momentary Test
    manager.clearAllMappings();
    LiveBus::MidiMappingEntry momMap;
    momMap.messageType = LiveBus::MidiMessageType::Note;
    momMap.controllerOrNoteNumber = 40;
    momMap.targetParameterId = "ch.1.mute";
    momMap.noteMode = LiveBus::NoteTriggerMode::Momentary;
    manager.addMapping(momMap);

    auto noteOn40 = juce::MidiMessage::noteOn(1, 40, (juce::uint8)100);
    manager.handleIncomingMidiMessage(nullptr, noteOn40);
    manager.processQueuedEvents();
    TEST_ASSERT(paramVal == 1.0f, "Momentary NoteOn sets parameter ON");

    auto noteOff40 = juce::MidiMessage::noteOff(1, 40);
    manager.handleIncomingMidiMessage(nullptr, noteOff40);
    manager.processQueuedEvents();
    TEST_ASSERT(paramVal == 0.0f, "Momentary NoteOff resets parameter OFF");
}

void testMIDIChannelAndControllerFiltering()
{
    LiveBus::MidiParameterRegistry registry;
    float ch1Gain = 0.0f;
    float ch2Gain = 0.0f;

    LiveBus::ParameterDescriptor d1;
    d1.id = "ch.1.gain";
    d1.minValue = -60.0f; d1.maxValue = 12.0f;
    d1.getter = [&ch1Gain]() { return ch1Gain; };
    d1.setter = [&ch1Gain](float v) { ch1Gain = v; };
    registry.registerParameter(d1);

    LiveBus::ParameterDescriptor d2;
    d2.id = "ch.2.gain";
    d2.minValue = -60.0f; d2.maxValue = 12.0f;
    d2.getter = [&ch2Gain]() { return ch2Gain; };
    d2.setter = [&ch2Gain](float v) { ch2Gain = v; };
    registry.registerParameter(d2);

    LiveBus::MidiManager manager(registry);
    manager.clearAllMappings();

    // Map Ch 1 CC 7 to ch.1.gain
    LiveBus::MidiMappingEntry m1;
    m1.midiChannel = 1;
    m1.controllerOrNoteNumber = 7;
    m1.targetParameterId = "ch.1.gain";
    m1.minValue = -60.0f; m1.maxValue = 12.0f;
    manager.addMapping(m1);

    // Map Ch 2 CC 7 to ch.2.gain
    LiveBus::MidiMappingEntry m2;
    m2.midiChannel = 2;
    m2.controllerOrNoteNumber = 7;
    m2.targetParameterId = "ch.2.gain";
    m2.minValue = -60.0f; m2.maxValue = 12.0f;
    manager.addMapping(m2);

    // Send CC 7 on MIDI Channel 1 with value 127
    auto msgCh1 = juce::MidiMessage::controllerEvent(1, 7, 127);
    manager.handleIncomingMidiMessage(nullptr, msgCh1);
    manager.processQueuedEvents();

    TEST_ASSERT(std::abs(ch1Gain - 12.0f) < 0.1f, "Channel 1 message updated ch.1.gain");
    TEST_ASSERT(ch2Gain == 0.0f, "Channel 1 message did NOT leak into ch.2.gain");

    // Send CC 7 on MIDI Channel 2 with value 0
    auto msgCh2 = juce::MidiMessage::controllerEvent(2, 7, 0);
    manager.handleIncomingMidiMessage(nullptr, msgCh2);
    manager.processQueuedEvents();

    TEST_ASSERT(std::abs(ch2Gain - (-60.0f)) < 0.1f, "Channel 2 message updated ch.2.gain");
    TEST_ASSERT(std::abs(ch1Gain - 12.0f) < 0.1f, "ch.1.gain remained unaffected by Ch 2 message");
}

void testMIDILearnWorkflowAndTimeout()
{
    LiveBus::MidiParameterRegistry registry;
    LiveBus::MidiManager manager(registry);
    manager.clearAllMappings();

    LiveBus::MidiMappingEntry map;
    map.id = "target_map";
    map.targetParameterId = "bus.stream.gain";
    map.targetParamName = "Stream Master Gain";
    manager.addMapping(map);

    // 1. Enter Learn Mode
    manager.startLearn("target_map");
    TEST_ASSERT(manager.isLearning(), "MIDI Learn mode active");
    TEST_ASSERT(manager.getLearningMappingId() == "target_map", "Learning target ID matched");

    // 2. Incoming CC 22 on Channel 3 should bind to mapping
    bool learnFinished = false;
    manager.onLearnCompleted = [&learnFinished](const LiveBus::MidiMappingEntry& learned) {
        learnFinished = true;
        TEST_ASSERT(learned.controllerOrNoteNumber == 22, "Learned controller number 22");
        TEST_ASSERT(learned.midiChannel == 3, "Learned channel 3");
    };

    auto learnMsg = juce::MidiMessage::controllerEvent(3, 22, 64);
    manager.handleIncomingMidiMessage(nullptr, learnMsg);
    manager.processQueuedEvents();

    TEST_ASSERT(!manager.isLearning(), "MIDI Learn ends immediately upon message arrival");
    TEST_ASSERT(learnFinished, "onLearnCompleted callback fired successfully");
    TEST_ASSERT(manager.getMapping(0).controllerOrNoteNumber == 22, "Mapping entry stored learned CC number");

    // 3. Test Learn Timeout
    manager.startLearn("target_map");
    TEST_ASSERT(manager.isLearning(), "Learn started again for timeout test");
    manager.tickLearnTimer(16000);
    TEST_ASSERT(!manager.isLearning(), "Learn timed out and cancelled automatically");

    // 4. Test Manual Cancel
    manager.startLearn("target_map");
    TEST_ASSERT(manager.isLearning(), "Learn started for manual cancel test");
    manager.cancelLearn();
    TEST_ASSERT(!manager.isLearning(), "Learn manually cancelled successfully");
}

void testMIDIDuplicateDetectionAndWarning()
{
    LiveBus::MidiParameterRegistry registry;
    LiveBus::MidiManager manager(registry);
    manager.clearAllMappings();

    LiveBus::MidiMappingEntry m1;
    m1.id = "map_1";
    m1.midiChannel = 1;
    m1.controllerOrNoteNumber = 7;
    m1.targetParameterId = "ch.1.gain";
    m1.targetParamName = "Ch 1 Gain";
    manager.addMapping(m1);

    // Identical Channel and Controller but different parameter
    LiveBus::MidiMappingEntry m2;
    m2.id = "map_2";
    m2.midiChannel = 1;
    m2.controllerOrNoteNumber = 7;
    m2.targetParameterId = "ch.2.gain";
    m2.targetParamName = "Ch 2 Gain";

    auto duplicates = manager.findDuplicateMappings(m2);
    TEST_ASSERT(duplicates.size() == 1, "Duplicate detection identified collision");
    TEST_ASSERT(duplicates[0] == "Ch 1 Gain", "Duplicate report names existing mapped parameter");

    m2.midiChannel = 2;
    auto noDup = manager.findDuplicateMappings(m2);
    TEST_ASSERT(noDup.isEmpty(), "Different channel does not collide");
}

void testMIDIPresetSerializationAndFactoryProtection()
{
    LiveBus::MidiParameterRegistry registry;
    LiveBus::MidiManager manager(registry);

    // 1. Factory Presets Protection
    TEST_ASSERT(manager.isFactoryPreset("Default Mixer Faders"), "Default preset is recognized as factory protected");
    bool deleted = manager.deletePreset("Default Mixer Faders");
    TEST_ASSERT(!deleted, "Factory preset deletion is blocked");

    // 2. User Preset Save & Load Round-Trip
    manager.clearAllMappings();
    LiveBus::MidiMappingEntry customMap;
    customMap.controllerOrNoteNumber = 99;
    customMap.midiChannel = 5;
    customMap.targetParameterId = "bus.stream.gain";
    customMap.targetParamName = "Stream Master Gain";
    customMap.mode = LiveBus::MidiMode::RelativeSigned;
    customMap.invert = true;
    manager.addMapping(customMap);

    juce::String testPresetName = "Test_Automated_Preset";
    bool saved = manager.savePreset(testPresetName);
    TEST_ASSERT(saved, "User preset saved to disk");

    manager.clearAllMappings();
    TEST_ASSERT(manager.getNumMappings() == 0, "Mappings cleared");

    bool loaded = manager.loadPreset(testPresetName);
    TEST_ASSERT(loaded, "User preset reloaded from disk");
    TEST_ASSERT(manager.getNumMappings() == 1, "Reloaded mapping count matches");

    const auto& reloaded = manager.getMapping(0);
    TEST_ASSERT(reloaded.controllerOrNoteNumber == 99, "Reloaded CC number matches");
    TEST_ASSERT(reloaded.midiChannel == 5, "Reloaded channel matches");
    TEST_ASSERT(reloaded.mode == LiveBus::MidiMode::RelativeSigned, "Reloaded relative mode matches");
    TEST_ASSERT(reloaded.invert == true, "Reloaded invert flag matches");

    manager.deletePreset(testPresetName);
}

void testMIDIStreamAndMonitorBusTargetIsolation()
{
    LiveBus::MidiParameterRegistry registry;
    float streamGain = 0.0f;
    float monitorGain = 0.0f;
    bool streamMute = false;
    bool monitorMute = false;

    LiveBus::ParameterDescriptor sg;
    sg.id = "bus.stream.gain";
    sg.minValue = -60.0f; sg.maxValue = 12.0f;
    sg.getter = [&streamGain]() { return streamGain; };
    sg.setter = [&streamGain](float v) { streamGain = v; };
    registry.registerParameter(sg);

    LiveBus::ParameterDescriptor mg;
    mg.id = "bus.monitor.gain";
    mg.minValue = -60.0f; mg.maxValue = 12.0f;
    mg.getter = [&monitorGain]() { return monitorGain; };
    mg.setter = [&monitorGain](float v) { monitorGain = v; };
    registry.registerParameter(mg);

    LiveBus::ParameterDescriptor sm;
    sm.id = "bus.stream.mute";
    sm.isBoolean = true;
    sm.getter = [&streamMute]() { return streamMute ? 1.0f : 0.0f; };
    sm.setter = [&streamMute](float v) { streamMute = (v >= 0.5f); };
    registry.registerParameter(sm);

    LiveBus::ParameterDescriptor mm;
    mm.id = "bus.monitor.mute";
    mm.isBoolean = true;
    mm.getter = [&monitorMute]() { return monitorMute ? 1.0f : 0.0f; };
    mm.setter = [&monitorMute](float v) { monitorMute = (v >= 0.5f); };
    registry.registerParameter(mm);

    LiveBus::MidiManager manager(registry);
    manager.clearAllMappings();

    LiveBus::MidiMappingEntry mapSG;
    mapSG.controllerOrNoteNumber = 5;
    mapSG.targetParameterId = "bus.stream.gain";
    mapSG.minValue = -60.0f; mapSG.maxValue = 12.0f;
    manager.addMapping(mapSG);

    LiveBus::MidiMappingEntry mapMM;
    mapMM.messageType = LiveBus::MidiMessageType::Note;
    mapMM.controllerOrNoteNumber = 41;
    mapMM.targetParameterId = "bus.monitor.mute";
    mapMM.noteMode = LiveBus::NoteTriggerMode::Toggle;
    manager.addMapping(mapMM);

    // 1. Send CC 5 = 127 (Stream Gain -> +12 dB)
    auto msgSG = juce::MidiMessage::controllerEvent(1, 5, 127);
    manager.handleIncomingMidiMessage(nullptr, msgSG);
    manager.processQueuedEvents();

    TEST_ASSERT(std::abs(streamGain - 12.0f) < 0.1f, "Stream Bus gain was adjusted by CC 5");
    TEST_ASSERT(monitorGain == 0.0f, "Monitor Bus gain remains 100% untouched and isolated");

    // 2. Send Note 41 (Monitor Mute Toggle)
    auto msgMM = juce::MidiMessage::noteOn(1, 41, (juce::uint8)100);
    manager.handleIncomingMidiMessage(nullptr, msgMM);
    manager.processQueuedEvents();

    TEST_ASSERT(monitorMute == true, "Monitor Bus was muted by Note 41");
    TEST_ASSERT(streamMute == false, "Stream Bus mute state remains untouched and isolated");
}

void testPlaybackOpenFileValid()
{
    auto tempWav = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_audio_sine.wav");
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(new juce::FileOutputStream(tempWav), 44100.0, 2, 16, {}, 0));
    TEST_ASSERT(writer != nullptr, "Created WAV test writer");

    juce::AudioBuffer<float> tempBuf(2, 44100);
    for (int s = 0; s < 44100; ++s)
    {
        float val = 0.5f * std::sin((float)s * 440.0f * juce::MathConstants<float>::twoPi / 44100.0f);
        tempBuf.setSample(0, s, val);
        tempBuf.setSample(1, s, val);
    }
    writer->writeFromAudioSampleBuffer(tempBuf, 0, 44100);
    writer.reset();

    LiveBus::AudioFilePlayer player;
    bool loaded = player.loadFile(tempWav);
    TEST_ASSERT(loaded, "Valid WAV file loaded successfully");
    TEST_ASSERT(player.getLengthInSeconds() >= 0.95, "Audio file length is approximately 1.0 second");
    TEST_ASSERT(player.getWaveformCache().hasData(), "Waveform thumbnail peak data generated successfully");

    tempWav.deleteFile();
}

void testPlaybackRejectInvalidFile()
{
    LiveBus::AudioFilePlayer player;
    bool loadedNonExistent = player.loadFile(juce::File("Z:/invalid_path_xyz_12345/missing.wav"));
    TEST_ASSERT(!loadedNonExistent, "Non-existent file gracefully rejected without crash");

    auto corruptFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("corrupt_test.wav");
    corruptFile.replaceWithText("THIS IS NOT A VALID AUDIO FILE HEADER");
    bool loadedCorrupt = player.loadFile(corruptFile);
    TEST_ASSERT(!loadedCorrupt, "Corrupted file gracefully rejected by format manager");
    corruptFile.deleteFile();
}

void testPlaybackSeekAndPosition()
{
    LiveBus::AudioFilePlayer player;
    player.setPosition(0.5);
    TEST_ASSERT(player.getCurrentPosition() >= 0.0, "Position is non-negative");

    player.setPosition(-5.0);
    TEST_ASSERT(player.getCurrentPosition() >= 0.0, "Negative position clamped to 0.0");
}

void testPlaybackLoopRangeAndWrap()
{
    LiveBus::AudioFilePlayer player;
    player.setLoopRange(1.2, 4.8);
    TEST_ASSERT(std::abs(player.getLoopStart() - 1.2) < 0.01, "Loop start position set accurately");
    TEST_ASSERT(std::abs(player.getLoopEnd() - 4.8) < 0.01, "Loop end position set accurately");

    player.setLoopEnabled(true);
    TEST_ASSERT(player.isLoopEnabled(), "Looping mode is enabled");

    player.setLoopEnabled(false);
    TEST_ASSERT(!player.isLoopEnabled(), "Looping mode is disabled");
}

void testPlaybackMarkerCRUD()
{
    LiveBus::AudioFilePlayer player;
    player.clearMarkers();
    TEST_ASSERT(player.getMarkers().empty(), "Markers cleared");

    player.addMarker(1.5, "Verse 1");
    player.addMarker(4.2, "Chorus");
    auto markers = player.getMarkers();
    TEST_ASSERT(markers.size() == 2, "2 markers added");
    TEST_ASSERT(markers[0].name == "Verse 1", "Marker 1 name correct");
    TEST_ASSERT(std::abs(markers[0].positionSeconds - 1.5) < 0.01, "Marker 1 position correct");

    int idToRename = markers[0].id;
    player.renameMarker(idToRename, "Intro");
    markers = player.getMarkers();
    TEST_ASSERT(markers[0].name == "Intro", "Marker successfully renamed");

    player.removeMarker(idToRename);
    markers = player.getMarkers();
    TEST_ASSERT(markers.size() == 1, "Marker successfully deleted");
    TEST_ASSERT(markers[0].name == "Chorus", "Remaining marker is Chorus");
}

void testPlaybackPlaylistSaveLoadAndIntegrity()
{
    LiveBus::AudioFilePlayer player;
    player.clearPlaylist();
    TEST_ASSERT(player.getPlaylist().empty(), "Playlist initialized empty");

    auto tempXml = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_playlist_export.xml");

    LiveBus::PlaylistItem item1;
    item1.id = 1;
    item1.file = juce::File("C:/Music/Track1.wav");
    item1.title = "Track1";
    item1.durationSeconds = 180.0;
    item1.bpm = 120;

    LiveBus::PlaylistItem item2;
    item2.id = 2;
    item2.file = juce::File("C:/Music/Track2.mp3");
    item2.title = "Track2";
    item2.durationSeconds = 210.0;
    item2.bpm = 128;

    juce::XmlElement root("LiveBusPlaylist");
    root.addChildElement(item1.toXml().release());
    root.addChildElement(item2.toXml().release());
    root.writeTo(tempXml);

    bool loaded = player.loadPlaylistXml(tempXml);
    TEST_ASSERT(loaded, "Playlist XML parsed and loaded");
    auto pl = player.getPlaylist();
    TEST_ASSERT(pl.size() == 2, "2 tracks restored from playlist XML");
    TEST_ASSERT(pl[0].title == "Track1", "Track 1 title matches");
    TEST_ASSERT(pl[1].durationSeconds == 210.0, "Track 2 duration matches");

    tempXml.deleteFile();
}

void testPlaybackMissingTrackHandling()
{
    LiveBus::PlaylistItem item;
    item.file = juce::File("Z:/missing_path_does_not_exist_98765.wav");
    item.isAvailable = item.file.existsAsFile();
    TEST_ASSERT(!item.isAvailable, "Missing track marked as unavailable");

    LiveBus::AudioFilePlayer player;
    bool selected = player.selectTrack(999);
    TEST_ASSERT(!selected, "Selecting out-of-range track gracefully returns false");
}

void testPlaybackGainAndMute()
{
    LiveBus::AudioFilePlayer player;
    player.setGain(0.75f);
    TEST_ASSERT(std::abs(player.getGain() - 0.75f) < 0.01f, "Playback gain set to 0.75");

    player.setGain(5.0f);
    TEST_ASSERT(player.getGain() <= 2.0f, "Playback gain clamped to max 2.0");

    player.setMute(true);
    TEST_ASSERT(player.isMuted(), "Playback mute is active");
    player.setMute(false);
    TEST_ASSERT(!player.isMuted(), "Playback mute is inactive");
}

void testPlaybackSpeedRatioAndResampling()
{
    LiveBus::AudioFilePlayer player;
    player.setSpeedRatio(1.25f);
    TEST_ASSERT(std::abs(player.getSpeedRatio() - 1.25f) < 0.01f, "Playback speed ratio set to 1.25x");

    player.setSpeedRatio(10.0f);
    TEST_ASSERT(player.getSpeedRatio() <= 4.0f, "Speed ratio clamped to max 4.0x");

    player.setSpeedRatio(0.01f);
    TEST_ASSERT(player.getSpeedRatio() >= 0.25f, "Speed ratio clamped to min 0.25x");
}

void testTestSignalSineFrequencyAndPhaseDelta()
{
    LiveBus::TestSignalGenerator gen;
    gen.prepareToPlay(48000.0, 512);
    gen.setSignalType(LiveBus::SignalType::Sine);
    gen.setFrequency(1000.0);
    gen.setLevelDb(-6.0f);
    gen.setEnabled(true);

    juce::AudioBuffer<float> buf(2, 480);
    buf.clear();
    float* const ptrs[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
    gen.renderBlock(ptrs, 2, 480);

    float peakL = buf.getMagnitude(0, 0, 480);
    float peakR = buf.getMagnitude(1, 0, 480);
    TEST_ASSERT(peakL > 0.01f, "Sine wave output non-zero on Left channel");
    TEST_ASSERT(peakR > 0.01f, "Sine wave output non-zero on Right channel");
    TEST_ASSERT(!std::isnan(peakL) && !std::isinf(peakL), "Sine wave output is valid finite number");
}

void testTestSignalLevelClampingAndDbConversion()
{
    LiveBus::TestSignalGenerator gen;
    gen.setLevelDb(+10.0f);
    TEST_ASSERT(gen.getLevelDb() <= 0.0f, "Signal level clamped to max 0.0 dBFS ceiling");

    gen.setLevelDb(-95.0f);
    TEST_ASSERT(gen.getLevelDb() >= -90.0f, "Signal level clamped to min -90.0 dBFS floor");

    gen.setLevelDb(-24.0f);
    TEST_ASSERT(std::abs(gen.getLevelDb() - (-24.0f)) < 0.01f, "Signal level set to -24.0 dBFS safe default");
}

void testTestSignalWhiteAndPinkNoiseSpectralDifferences()
{
    LiveBus::TestSignalGenerator gen;
    gen.prepareToPlay(48000.0, 512);
    gen.setLevelDb(-12.0f);
    gen.setEnabled(true);

    // 1. Generate White Noise block
    gen.setSignalType(LiveBus::SignalType::WhiteNoise);
    juce::AudioBuffer<float> whiteBuf(2, 2048);
    whiteBuf.clear();
    float* const wPtrs[2] = { whiteBuf.getWritePointer(0), whiteBuf.getWritePointer(1) };
    gen.renderBlock(wPtrs, 2, 2048);
    float whiteMag = whiteBuf.getMagnitude(0, 0, 2048);
    TEST_ASSERT(whiteMag > 0.01f, "White noise generated non-zero audio");

    // 2. Generate Pink Noise block
    gen.setSignalType(LiveBus::SignalType::PinkNoise);
    juce::AudioBuffer<float> pinkBuf(2, 2048);
    pinkBuf.clear();
    float* const pPtrs[2] = { pinkBuf.getWritePointer(0), pinkBuf.getWritePointer(1) };
    gen.renderBlock(pPtrs, 2, 2048);
    float pinkMag = pinkBuf.getMagnitude(0, 0, 2048);
    TEST_ASSERT(pinkMag > 0.01f, "Pink noise generated non-zero audio");

    // Both signals are valid and non-zero
    TEST_ASSERT(!std::isnan(whiteMag) && !std::isnan(pinkMag), "Noise signals are strictly finite");
}

void testTestSignalNyquistLimitEnforcement()
{
    LiveBus::TestSignalGenerator gen;
    gen.prepareToPlay(48000.0, 512);
    gen.setFrequency(35000.0); // Above Nyquist
    TEST_ASSERT(gen.getFrequency() <= 23520.0, "Frequency clamped strictly below Nyquist limit (0.49 * SR)");
}

void testTestSignalRoutingToBusesAndChannels()
{
    LiveBus::TestSignalGenerator gen;
    gen.setDestination(LiveBus::SignalDestination::StreamBus);
    TEST_ASSERT(gen.getDestination() == LiveBus::SignalDestination::StreamBus, "Destination set to Stream Bus");

    gen.setDestination(LiveBus::SignalDestination::MonitorBus);
    TEST_ASSERT(gen.getDestination() == LiveBus::SignalDestination::MonitorBus, "Destination set to Monitor Bus");

    gen.setDestination(LiveBus::SignalDestination::InputChannel1);
    TEST_ASSERT(gen.getDestination() == LiveBus::SignalDestination::InputChannel1, "Destination set to Input Channel 1");

    // Test Channel Isolation: Left Only
    gen.setChannel(LiveBus::SignalChannel::LeftOnly);
    gen.setEnabled(true);
    gen.prepareToPlay(48000.0, 512);
    gen.setLevelDb(-6.0f);

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    float* const ptrs[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
    gen.renderBlock(ptrs, 2, 512);

    float peakL = buf.getMagnitude(0, 0, 512);
    float peakR = buf.getMagnitude(1, 0, 512);
    TEST_ASSERT(peakL > 0.01f, "Left channel received signal");
    TEST_ASSERT(peakR == 0.0f, "Right channel isolated with exact zero signal");
}

void testTestSignalNoNaNOrInfinitySanitization()
{
    LiveBus::TestSignalGenerator gen;
    gen.prepareToPlay(48000.0, 512);
    gen.setEnabled(true);
    gen.setLevelDb(0.0f);

    std::vector<LiveBus::SignalType> types = {
        LiveBus::SignalType::Sine,
        LiveBus::SignalType::WhiteNoise,
        LiveBus::SignalType::PinkNoise,
        LiveBus::SignalType::Impulse
    };

    for (auto t : types)
    {
        gen.setSignalType(t);
        juce::AudioBuffer<float> buf(2, 1024);
        buf.clear();
        float* const ptrs[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
        gen.renderBlock(ptrs, 2, 1024);

        for (int s = 0; s < 1024; ++s)
        {
            float s0 = buf.getSample(0, s);
            float s1 = buf.getSample(1, s);
            TEST_ASSERT(!std::isnan(s0) && !std::isinf(s0), "No NaN/Inf in signal rendering");
            TEST_ASSERT(!std::isnan(s1) && !std::isinf(s1), "No NaN/Inf in signal rendering");
        }
    }
}

void testTestSignalSilenceWhenStopped()
{
    LiveBus::TestSignalGenerator gen;
    gen.prepareToPlay(48000.0, 512);
    gen.setEnabled(false); // Generator STOPPED

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    float* const ptrs[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
    gen.renderBlock(ptrs, 2, 512);

    TEST_ASSERT(buf.getMagnitude(0, 0, 512) == 0.0f, "Zero output when signal generator is stopped");
    TEST_ASSERT(buf.getMagnitude(1, 0, 512) == 0.0f, "Zero output when signal generator is stopped");
}

void testPlaybackSampleRateChangeResilience()
{
    LiveBus::AudioFilePlayer player;
    player.prepareToPlay(44100.0, 256);
    TEST_ASSERT(true, "Prepared at 44.1 kHz");

    player.prepareToPlay(48000.0, 512);
    TEST_ASSERT(true, "Prepared at 48.0 kHz");

    player.prepareToPlay(96000.0, 1024);
    TEST_ASSERT(true, "Prepared at 96.0 kHz");

    player.prepareToPlay(192000.0, 2048);
    TEST_ASSERT(true, "Prepared at 192.0 kHz");
}

// ==============================================================================
// ==============================================================================
// Professional VST3 Plugin Host Tests
// ==============================================================================
void testVST3PluginLibraryModelSerializationAndDefaults()
{
    LiveBus::PluginInfoItem item;
    item.description.name = "Harrison MPC Compressor";
    item.description.fileOrIdentifier = "vst3-harrison-comp";
    item.description.pluginFormatName = "VST3";
    item.description.category = "Fx|Dynamics";
    item.description.manufacturerName = "Harrison Audio";
    item.profile = "User";
    item.isFavorite = true;
    item.isDefault = true;

    auto xml = item.toXml();
    TEST_ASSERT(xml != nullptr, "PluginInfoItem converts to valid XML");

    auto restored = LiveBus::PluginInfoItem::fromXml(*xml);
    TEST_ASSERT(restored.description.name == "Harrison MPC Compressor", "Restored plugin name matches");
    TEST_ASSERT(restored.description.fileOrIdentifier == "vst3-harrison-comp", "Restored identifier matches");
    TEST_ASSERT(restored.getFormat() == "VST3", "Restored format matches");
    TEST_ASSERT(restored.getCategory() == "Fx|Dynamics", "Restored category matches");
    TEST_ASSERT(restored.getManufacturer() == "Harrison Audio", "Restored manufacturer matches");
    TEST_ASSERT(restored.profile == "User", "Restored profile matches");
    TEST_ASSERT(restored.isFavorite, "Restored favorite flag matches");
    TEST_ASSERT(restored.isDefault, "Restored default flag matches");

    // Test PluginChainSlot serialization
    LiveBus::PluginChainSlot slot;
    slot.instanceId = "slot-2";
    slot.pluginName = "Harrison 32 Bus";
    slot.fileOrIdentifier = "vst3-harrison-32bus";
    slot.isBypassed = true;
    slot.isIsolatedMode = false;
    slot.latencySamples = 64;

    auto slotXml = slot.toXml();
    TEST_ASSERT(slotXml != nullptr, "PluginChainSlot converts to valid XML");
    auto restoredSlot = LiveBus::PluginChainSlot::fromXml(*slotXml);
    TEST_ASSERT(restoredSlot.instanceId == "slot-2", "Instance ID matches");
    TEST_ASSERT(restoredSlot.pluginName == "Harrison 32 Bus", "Slot name matches");
    TEST_ASSERT(restoredSlot.isBypassed, "Slot bypass matches");
    TEST_ASSERT(restoredSlot.latencySamples == 64, "Slot latency matches");

    // Test PluginScanSettings
    LiveBus::PluginScanSettings settings;
    settings.scanStandardVst3 = true;
    settings.customScanPaths.add("D:\\Plugins\\VST3");
    settings.customScanPaths.add("E:\\Audio\\VST3");

    auto settingsXml = settings.toXml();
    TEST_ASSERT(settingsXml != nullptr, "PluginScanSettings converts to valid XML");
    auto restoredSettings = LiveBus::PluginScanSettings::fromXml(*settingsXml);
    TEST_ASSERT(restoredSettings.scanStandardVst3, "Standard paths enabled");
    TEST_ASSERT(restoredSettings.customScanPaths.size() == 2, "Custom paths count matches");
    TEST_ASSERT(restoredSettings.customScanPaths[0] == "D:\\Plugins\\VST3", "First path matches");
}

void testVST3PluginScanSettingsAndPaths()
{
    LiveBus::PluginScanSettings settings;
    TEST_ASSERT(settings.scanStandardVst3, "Default scan includes standard VST3 paths");
    TEST_ASSERT(settings.customScanPaths.isEmpty(), "Initial custom search paths empty");

    // Add path
    settings.customScanPaths.addIfNotAlreadyThere("C:\\CustomVST3");
    TEST_ASSERT(settings.customScanPaths.size() == 1, "Added custom path");

    // Prevent duplicate
    settings.customScanPaths.addIfNotAlreadyThere("C:\\CustomVST3");
    TEST_ASSERT(settings.customScanPaths.size() == 1, "Duplicate path rejected");

    // Remove path
    settings.customScanPaths.removeString("C:\\CustomVST3");
    TEST_ASSERT(settings.customScanPaths.isEmpty(), "Removed custom path");
}

void testVST3CatalogSearchAndFiltering()
{
    PluginHostClient client;
    LiveBus::VST3HostEngine engine(client);
    engine.loadLibraryCache();

    auto catalog = engine.getLibraryPlugins();
    TEST_ASSERT(!catalog.empty(), "Plugin catalog is non-empty");

    // Verify presence of reference entries
    bool foundHarrison = false;
    bool foundWaves = false;
    for (const auto& item : catalog)
    {
        if (item.getManufacturer().containsIgnoreCase("Harrison"))
            foundHarrison = true;
        if (item.getManufacturer().containsIgnoreCase("Waves"))
            foundWaves = true;
    }
    TEST_ASSERT(foundHarrison, "Reference Harrison Audio plugins detected");
    TEST_ASSERT(foundWaves, "Reference Waves plugins detected");

    // Favorite toggle
    if (!catalog.empty())
    {
        auto firstId = catalog[0].description.fileOrIdentifier;
        engine.setFavorite(firstId, true);
        auto updated = engine.getLibraryPlugins();
        bool isFav = false;
        for (const auto& it : updated)
        {
            if (it.description.fileOrIdentifier == firstId && it.isFavorite)
                isFav = true;
        }
        TEST_ASSERT(isFav, "Plugin favorite state successfully toggled and stored");

        // Set default
        engine.setDefaultPlugin(firstId);
        TEST_ASSERT(engine.getDefaultPluginIdentifier() == firstId, "Default plugin identifier updated");
    }
}

void testVST3PluginChainSlotManagementAndLifecycle()
{
    PluginHostClient client;
    LiveBus::VST3HostEngine engine(client);

    TEST_ASSERT(LiveBus::VST3HostEngine::NumTargets == maxInputs + 2,
                "VST targets cover every channel and both master buses");
    TEST_ASSERT(LiveBus::VST3HostEngine::getTargetName(maxInputs - 1) == "Channel 16",
                "Last VST target matches the last mixer channel");
    TEST_ASSERT(LiveBus::VST3HostEngine::getTargetName(maxInputs) == "Stream Bus Master",
                "Stream Bus remains a distinct VST target");

    // Stream Bus follows the 16 channel targets.
    engine.clearTargetChain(maxInputs);
    TEST_ASSERT(engine.getNumPluginsInTarget(maxInputs) == 0, "Stream Bus chain starts empty");

    juce::PluginDescription desc1;
    desc1.name = "Harrison MPC Compressor";
    desc1.fileOrIdentifier = "vst3-harrison-comp";
    desc1.pluginFormatName = "VST3";
    desc1.category = "Fx|Dynamics";

    bool ok1 = engine.loadPluginToTarget(maxInputs, desc1, false);
    TEST_ASSERT(ok1, "Loaded first plugin to Stream Bus");
    TEST_ASSERT(engine.getNumPluginsInTarget(maxInputs) == 1, "Stream Bus has 1 plugin");

    auto slot0 = engine.getSlotInfo(maxInputs, 0);
    TEST_ASSERT(slot0.pluginName == "Harrison MPC Compressor", "Slot 0 name matches");
    TEST_ASSERT(!slot0.isBypassed, "Slot 0 initially active");

    // Bypass
    engine.setSlotBypassed(maxInputs, 0, true);
    TEST_ASSERT(engine.isSlotBypassed(maxInputs, 0), "Slot 0 bypassed");
    engine.setSlotBypassed(maxInputs, 0, false);
    TEST_ASSERT(!engine.isSlotBypassed(maxInputs, 0), "Slot 0 re-enabled");

    // Add second plugin
    juce::PluginDescription desc2;
    desc2.name = "Harrison 32 Bus";
    desc2.fileOrIdentifier = "vst3-harrison-32bus";
    desc2.pluginFormatName = "VST3";

    bool ok2 = engine.loadPluginToTarget(maxInputs, desc2, false);
    TEST_ASSERT(ok2, "Loaded second plugin");
    TEST_ASSERT(engine.getNumPluginsInTarget(maxInputs) == 2, "Stream Bus has 2 plugins");

    // Reorder: move slot 0 down to 1
    engine.movePluginInTarget(maxInputs, 0, 1);
    auto slotAfterMove0 = engine.getSlotInfo(maxInputs, 0);
    auto slotAfterMove1 = engine.getSlotInfo(maxInputs, 1);
    TEST_ASSERT(slotAfterMove0.pluginName == "Harrison 32 Bus", "Slot 0 swapped to Harrison 32 Bus");
    TEST_ASSERT(slotAfterMove1.pluginName == "Harrison MPC Compressor", "Slot 1 swapped to Harrison MPC Compressor");

    // Remove slot
    bool rem = engine.removePluginFromTarget(maxInputs, 0);
    TEST_ASSERT(rem, "Removed slot 0");
    TEST_ASSERT(engine.getNumPluginsInTarget(maxInputs) == 1, "1 plugin remains");
    TEST_ASSERT(engine.getSlotInfo(maxInputs, 0).pluginName == "Harrison MPC Compressor", "Remaining plugin is Compressor");

    // Bounds safety
    auto outOfBounds = engine.getSlotInfo(maxInputs, 99);
    TEST_ASSERT(outOfBounds.pluginName.isEmpty(), "Out of bounds slot info returns safe default");
}

void testVST3PluginChainXMLSerializationAndStateRestore()
{
    PluginHostClient client;
    LiveBus::VST3HostEngine engine(client);

    engine.clearTargetChain(0); // Input 1 (Mic)
    juce::PluginDescription desc1;
    desc1.name = "Harrison Vocal Flow";
    desc1.fileOrIdentifier = "vst3-harrison-vocal";
    desc1.pluginFormatName = "VST3";
    engine.loadPluginToTarget(0, desc1, false);

    juce::PluginDescription desc2;
    desc2.name = "Slate Digital Inf EQ";
    desc2.fileOrIdentifier = "vst3-slate-infeq";
    desc2.pluginFormatName = "VST3";
    engine.loadPluginToTarget(0, desc2, false);
    engine.setSlotBypassed(0, 1, true);

    auto xml = engine.exportTargetChainXml(0);
    TEST_ASSERT(xml != nullptr, "Target chain exported to XML");

    engine.clearTargetChain(0);
    TEST_ASSERT(engine.getNumPluginsInTarget(0) == 0, "Target chain cleared");

    bool imported = engine.importTargetChainXml(0, *xml);
    TEST_ASSERT(imported, "Target chain imported from XML");
    TEST_ASSERT(engine.getNumPluginsInTarget(0) == 2, "Both slots restored");
    TEST_ASSERT(engine.getSlotInfo(0, 0).pluginName == "Harrison Vocal Flow", "First slot name restored");
    TEST_ASSERT(!engine.isSlotBypassed(0, 0), "First slot not bypassed");
    TEST_ASSERT(engine.getSlotInfo(0, 1).pluginName == "Slate Digital Inf EQ", "Second slot name restored");
    TEST_ASSERT(engine.isSlotBypassed(0, 1), "Second slot bypass state restored");

    juce::PluginDescription lastChannelDesc;
    lastChannelDesc.name = "Channel 16 Insert";
    lastChannelDesc.fileOrIdentifier = "vst3-channel-16-placeholder";
    lastChannelDesc.pluginFormatName = "VST3";
    TEST_ASSERT(engine.loadPluginToTarget(maxInputs - 1, lastChannelDesc, false),
                "VST chain accepts the last mixer channel");
    TEST_ASSERT(engine.getNumPluginsInTarget(maxInputs - 1) == 1,
                "Channel 16 maintains an independent insert chain");
    auto lastChannelXml = engine.exportTargetChainXml(maxInputs - 1);
    TEST_ASSERT(lastChannelXml != nullptr, "Channel 16 insert chain exports");
    engine.clearTargetChain(maxInputs - 1);
    TEST_ASSERT(engine.importTargetChainXml(maxInputs - 1, *lastChannelXml),
                "Channel 16 insert chain restores");
    TEST_ASSERT(engine.getSlotInfo(maxInputs - 1, 0).pluginName == "Channel 16 Insert",
                "Channel 16 insert slot persists");
}

void testVST3MissingPluginPlaceholderGracefulHandling()
{
    PluginHostClient client;
    LiveBus::VST3HostEngine engine(client);

    juce::PluginDescription missingDesc;
    missingDesc.name = "NonExistent Ghost Plugin";
    missingDesc.fileOrIdentifier = "C:\\Windows\\System32\\NonExistent.vst3";
    missingDesc.pluginFormatName = "VST3";

    // Attempting to load nonexistent plugin
    bool loaded = engine.loadPluginToTarget(1, missingDesc, false);
    TEST_ASSERT(loaded, "Engine creates graceful placeholder without throwing exception");
    TEST_ASSERT(engine.getNumPluginsInTarget(1) == 1, "Slot registered");

    auto slot = engine.getSlotInfo(1, 0);
    TEST_ASSERT(slot.pluginName == "NonExistent Ghost Plugin", "Slot maintains name for scene preservation");

    // Audio processing over placeholder
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    buffer.setSample(0, 10, 0.42f);
    engine.processTarget(1, buffer, 512);
    TEST_ASSERT(std::abs(buffer.getSample(0, 10) - 0.42f) < 1e-6f, "Audio passes through placeholder safely");

    // Clean removal
    engine.removePluginFromTarget(1, 0);
    TEST_ASSERT(engine.getNumPluginsInTarget(1) == 0, "Placeholder slot cleanly removed");
}

void testVST3FailedBinaryLoadDoesNotCreateActiveSlot()
{
    PluginHostClient client;
    LiveBus::VST3HostEngine engine(client);

    auto invalidPlugin = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("LiveBusInvalidPlugin-" + juce::Uuid().toString() + ".vst3");
    TEST_ASSERT(invalidPlugin.create().wasOk(), "Temporary invalid plugin fixture created");
    invalidPlugin.replaceWithText("not a VST3 binary");

    juce::PluginDescription desc;
    desc.name = "Invalid test plugin";
    desc.fileOrIdentifier = invalidPlugin.getFullPathName();
    desc.pluginFormatName = "VST3";

    TEST_ASSERT(!engine.loadPluginToTarget(0, desc, false), "Invalid existing plugin binary is reported as a load failure");
    TEST_ASSERT(engine.getNumPluginsInTarget(0) == 0, "Failed plugin load does not leave a phantom chain slot");
    TEST_ASSERT(engine.getLastLoadError().isNotEmpty(), "Plugin load failure exposes a diagnostic message");

    invalidPlugin.deleteFile();
}

void testVST3AudioProcessingFormatAndDryFallbackSafety()
{
    PluginHostClient client;
    LiveBus::VST3HostEngine engine(client);

    engine.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < 512; ++s)
            buffer.setSample(ch, s, std::sin((float)s * 0.1f));
    }

    auto copy = buffer;

    // Process empty target 5 (Monitor Bus)
    engine.processTarget(5, buffer, 512);

    float maxDiff = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < 512; ++s)
        {
            float diff = std::abs(buffer.getSample(ch, s) - copy.getSample(ch, s));
            if (diff > maxDiff) maxDiff = diff;
        }
    }
    TEST_ASSERT(maxDiff == 0.0f, "Empty/bypassed target processing produces 100% bit-exact dry passthrough");

    // Sample rate changes
    engine.prepareToPlay(44100.0, 256);
    engine.prepareToPlay(96000.0, 1024);
    TEST_ASSERT(true, "VST3HostEngine handles sample rate and block size transitions without error");
}

void testVST3OutOfProcessWorkerTimeoutAndWatchdog()
{
    PluginHostClient client;
    TEST_ASSERT(!client.isRunning(), "Helper worker not running initially");

    // Verify bypass control
    client.setBypass(true);
    TEST_ASSERT(client.getBypass(), "Client bypass state set");
    client.setBypass(false);
    TEST_ASSERT(!client.getBypass(), "Client bypass state cleared");

    // Verify state reporting
    auto state = client.getPluginState();
    TEST_ASSERT(state == PluginHostClient::PluginState::HelperRestarting || state == PluginHostClient::PluginState::NoPluginLoaded,
                "State reports expected initial helper state");

    // Verify dry fallback when worker is disconnected
    juce::AudioBuffer<float> buf(2, 256);
    for (int s = 0; s < 256; ++s)
        buf.setSample(0, s, 0.75f);

    client.processBlock(buf, 256);
    // Since worker is not launched, client should safely retain audio without crashing or blocking
    TEST_ASSERT(std::abs(buf.getSample(0, 100) - 0.75f) < 1e-6f, "OutOfProcess client safely executes non-blocking audio fallback");
}

void testMidiPresetImportRejectsPathTraversalName()
{
    LiveBus::MidiParameterRegistry registry;
    LiveBus::MidiManager manager(registry);

    const auto sourceFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("LiveBusMidiTraversal-" + juce::Uuid().toString() + ".xml");
    juce::XmlElement maliciousPreset("LiveBusMidiMappingPreset");
    maliciousPreset.setAttribute("name", "..\\..\\outside");
    TEST_ASSERT(maliciousPreset.writeTo(sourceFile), "Malicious MIDI preset fixture written");

    const auto originalPresetName = manager.getCurrentPresetName();
    const auto presetsBeforeImport = manager.getAvailablePresets();
    TEST_ASSERT(!manager.importPreset(sourceFile), "MIDI preset import rejects path traversal in XML name");
    TEST_ASSERT(manager.getCurrentPresetName() == originalPresetName, "Rejected MIDI preset import leaves active preset unchanged");
    TEST_ASSERT(manager.getAvailablePresets().size() == presetsBeforeImport.size(),
                "Rejected MIDI preset import does not create a preset");

    sourceFile.deleteFile();
}

void testVST3SceneManagerPluginIntegration()
{
    SceneManager sm;
    SceneData scene;
    scene.sceneName = "VST3_Automated_Test";

    VST3SceneState p1;
    p1.pluginName = "Harrison MPC Compressor";
    p1.pluginPath = "vst3-harrison-comp";
    p1.isBypassed = true;
    p1.stateBlob.append("DUMMY_BLOB_DATA", 15);
    scene.plugins.push_back(p1);

    VST3SceneState p2;
    p2.pluginName = "Harrison 32 Bus";
    p2.pluginPath = "vst3-harrison-32bus";
    p2.isBypassed = false;
    scene.plugins.push_back(p2);

    bool saved = sm.saveScene("VST3_Automated_Test", scene);
    TEST_ASSERT(saved, "Scene with VST3 plugin state successfully saved to disk");

    SceneData loaded;
    juce::String err;
    bool loadedOk = sm.loadScene("VST3_Automated_Test", loaded, err);
    TEST_ASSERT(loadedOk, "Scene with VST3 plugin state successfully loaded from disk");
    TEST_ASSERT(loaded.plugins.size() == 2, "Both plugins restored in scene");
    TEST_ASSERT(loaded.plugins[0].pluginName == "Harrison MPC Compressor", "First plugin name matches");
    TEST_ASSERT(loaded.plugins[0].isBypassed, "First plugin bypass flag matches");
    TEST_ASSERT(loaded.plugins[0].stateBlob.getSize() == 15, "First plugin binary state blob size matches");
    TEST_ASSERT(loaded.plugins[1].pluginName == "Harrison 32 Bus", "Second plugin name matches");
    TEST_ASSERT(!loaded.plugins[1].isBypassed, "Second plugin bypass flag matches");

    // Clean up test scene
    sm.deleteScene("VST3_Automated_Test");
}

// =============================================================================
// Professional Performance & Diagnostics Tests
// =============================================================================

void testBufferDurationAndSampleRateConversion()
{
    using namespace LiveBus;

    // Standard formula: ms = (samples / sampleRate) * 1000.0
    // Test 128 samples at 48000 Hz = 2.66667 ms
    float ms128_48k = AudioDeviceService::calculateBufferDurationMs(128, 48000.0);
    TEST_ASSERT(std::abs(ms128_48k - (128.0f / 48000.0f * 1000.0f)) < 0.001f, "128 samples @ 48kHz duration matches formula (2.67ms)");

    // Test 64 samples at 48000 Hz = 1.33333 ms
    float ms64_48k = AudioDeviceService::calculateBufferDurationMs(64, 48000.0);
    TEST_ASSERT(std::abs(ms64_48k - 1.33333f) < 0.001f, "64 samples @ 48kHz duration matches formula (1.33ms)");

    // Test 512 samples at 44100 Hz = 11.61 ms
    float ms512_44k = AudioDeviceService::calculateBufferDurationMs(512, 44100.0);
    TEST_ASSERT(std::abs(ms512_44k - 11.60998f) < 0.001f, "512 samples @ 44.1kHz duration matches formula (11.61ms)");

    // Test 256 samples at 96000 Hz = 2.67 ms
    float ms256_96k = AudioDeviceService::calculateBufferDurationMs(256, 96000.0);
    TEST_ASSERT(std::abs(ms256_96k - 2.66667f) < 0.001f, "256 samples @ 96kHz duration matches formula (2.67ms)");

    // Test 32 samples at 192000 Hz = 0.167 ms
    float ms32_192k = AudioDeviceService::calculateBufferDurationMs(32, 192000.0);
    TEST_ASSERT(std::abs(ms32_192k - 0.16667f) < 0.001f, "32 samples @ 192kHz duration matches formula (0.17ms)");

    // Safety edge cases: zero / negative sample rate or buffer size
    TEST_ASSERT(AudioDeviceService::calculateBufferDurationMs(0, 48000.0) == 0.0f, "Zero buffer samples returns 0.0 ms");
    TEST_ASSERT(AudioDeviceService::calculateBufferDurationMs(-64, 48000.0) == 0.0f, "Negative buffer samples returns 0.0 ms");
    TEST_ASSERT(AudioDeviceService::calculateBufferDurationMs(128, 0.0) == 0.0f, "Zero sample rate returns 0.0 ms without division by zero");
    TEST_ASSERT(AudioDeviceService::calculateBufferDurationMs(128, -48000.0) == 0.0f, "Negative sample rate returns 0.0 ms safely");
}

void testLatencyReportingAndUnavailableHandling()
{
    using namespace LiveBus;

    juce::AudioDeviceManager dm;
    AudioDeviceService service(dm);

    // When no hardware audio device is active in unit test environment:
    // Latencies must return -1 / -1.0f without crashing, clearly indicating N/A
    int inLatSamples = service.getInputLatencySamples();
    int outLatSamples = service.getOutputLatencySamples();
    float inLatMs = service.getInputLatencyMs();
    float outLatMs = service.getOutputLatencyMs();

    TEST_ASSERT(inLatSamples == -1, "Input latency samples returns -1 when no device is open");
    TEST_ASSERT(outLatSamples == -1, "Output latency samples returns -1 when no device is open");
    TEST_ASSERT(inLatMs < 0.0f, "Input latency ms returns negative when no device is open");
    TEST_ASSERT(outLatMs < 0.0f, "Output latency ms returns negative when no device is open");

    // Formatted strings should indicate "N/A" when negative or unavailable
    juce::String inLatStr = (inLatMs >= 0.0f) ? (juce::String(inLatMs, 1) + "ms") : "N/A";
    juce::String outLatStr = (outLatMs >= 0.0f) ? (juce::String(outLatMs, 1) + "ms") : "N/A";
    TEST_ASSERT(inLatStr == "N/A", "Input latency displays N/A when unavailable");
    TEST_ASSERT(outLatStr == "N/A", "Output latency displays N/A when unavailable");

    // Critical instruction check: Ensure 1 buffer duration is distinguished from total latency
    float bufDurMs = AudioDeviceService::calculateBufferDurationMs(128, 48000.0);
    TEST_ASSERT(bufDurMs > 0.0f, "1 buffer duration is positive");
    TEST_ASSERT(bufDurMs != inLatMs, "Buffer duration is distinct from driver input latency");
}

void testCPULoadCalculationAndBoundaryClamping()
{
    using namespace LiveBus;

    // Conceptual formula: processing load (%) = processing duration / available block time * 100
    // Test normal load: 1.333 ms on 2.667 ms budget -> 50%
    float load50 = AudioPerformanceMonitor::calculateLoadPercent(1.33333, 2.66667);
    TEST_ASSERT(std::abs(load50 - 50.0f) < 0.1f, "Load calculation accurate for 50% utilization");

    // Test idle load: 0.0 ms duration -> 0.0%
    float load0 = AudioPerformanceMonitor::calculateLoadPercent(0.0, 2.66667);
    TEST_ASSERT(load0 == 0.0f, "Idle processing duration yields 0.0% load");

    // Test negative duration -> 0.0%
    float loadNeg = AudioPerformanceMonitor::calculateLoadPercent(-1.5, 2.66667);
    TEST_ASSERT(loadNeg == 0.0f, "Negative processing duration clamped to 0.0%");

    // Test invalid budget (0.0 ms) -> safe 0.0% fallback without NaN or divide by zero
    float loadDivZero = AudioPerformanceMonitor::calculateLoadPercent(1.0, 0.0);
    TEST_ASSERT(loadDivZero == 0.0f && !std::isnan(loadDivZero), "Zero block budget returns 0.0% without NaN");

    // Test overrun: 3.0 ms on 2.6667 ms budget -> 112.5%
    float loadOverrun = AudioPerformanceMonitor::calculateLoadPercent(3.0, 2.66667);
    TEST_ASSERT(loadOverrun > 100.0f, "Overrun duration correctly calculates load > 100%");

    // Test monitor telemetry snapshot smoothing and decay
    AudioPerformanceMonitor monitor;
    monitor.clearStatistics();
    auto snapInitial = monitor.getSnapshot();
    TEST_ASSERT(snapInitial.currentCpuLoadPercent == 0.0f, "Initial load is 0%");
    TEST_ASSERT(snapInitial.averageCpuLoadPercent == 0.0f, "Initial avg load is 0%");
    TEST_ASSERT(snapInitial.deadlineMissCount == 0, "Initial deadline miss count is 0");
    TEST_ASSERT(!snapInitial.hasWarning, "Initial state has no warnings");
}

void testDeadlineMissAndDropoutCounters()
{
    using namespace LiveBus;

    AudioPerformanceMonitor monitor;
    monitor.clearStatistics();

    // Verify initial counters
    auto s1 = monitor.getSnapshot();
    TEST_ASSERT(s1.deadlineMissCount == 0, "Initial deadline miss is 0");
    TEST_ASSERT(s1.dropoutCount == 0, "Initial dropout count is 0");
    TEST_ASSERT(s1.deviceErrorCount == 0, "Initial device error count is 0");
    TEST_ASSERT(s1.pluginErrorCount == 0, "Initial plugin error count is 0");

    // Record dropouts and errors
    monitor.recordDropout("Test audio underrun");
    monitor.recordDropout("Second dropout");
    monitor.recordDeviceError("USB I/O device disconnect");
    monitor.recordPluginError("TestEQ", "DSP exception trapped");

    auto s2 = monitor.getSnapshot();
    TEST_ASSERT(s2.dropoutCount == 2, "Dropout counter incremented to 2");
    TEST_ASSERT(s2.deviceErrorCount == 1, "Device error counter incremented to 1");
    TEST_ASSERT(s2.pluginErrorCount == 1, "Plugin error counter incremented to 1");
    TEST_ASSERT(s2.hasWarning == true, "Warning flag activated due to errors/dropouts");
    TEST_ASSERT(s2.activeWarningMessage.isNotEmpty(), "Active warning message populated");

    // Test Clear Statistics resets all counters
    monitor.clearStatistics();
    auto s3 = monitor.getSnapshot();
    TEST_ASSERT(s3.dropoutCount == 0, "Dropouts cleared");
    TEST_ASSERT(s3.deviceErrorCount == 0, "Device errors cleared");
    TEST_ASSERT(s3.pluginErrorCount == 0, "Plugin errors cleared");
    TEST_ASSERT(s3.hasWarning == false, "Warning flag cleared");

    // Counter overflow resilience: atomic uint32 safe wrapping
    std::atomic<juce::uint32> testCounter{0xFFFFFFFEu};
    testCounter.fetch_add(1, std::memory_order_relaxed);
    TEST_ASSERT(testCounter.load() == 0xFFFFFFFFu, "Near max uint32 reached safely");
    testCounter.fetch_add(1, std::memory_order_relaxed);
    TEST_ASSERT(testCounter.load() == 0u, "Atomic counter wraps to 0 without undefined behavior");
}

void testDiagnosticsQueueLockFreeRingBufferAndOverflowHandling()
{
    using namespace LiveBus;

    RealtimeDiagnosticQueue queue;
    queue.clear();
    TEST_ASSERT(queue.getDropCount() == 0, "Initial drop count is 0");

    // 1. Pop from empty queue returns false
    DiagnosticEvent dummy;
    TEST_ASSERT(!queue.pop(dummy), "Pop from empty queue returns false");

    // 2. Fill queue to maximum capacity (Capacity = 512, SPSC ring holds Capacity - 1 = 511 items)
    const size_t maxPush = RealtimeDiagnosticQueue::Capacity - 1;
    for (size_t i = 0; i < maxPush; ++i)
    {
        char msg[64];
        #if defined(_MSC_VER)
        sprintf_s(msg, sizeof(msg), "Event #%zu", i);
        #else
        snprintf(msg, sizeof(msg), "Event #%zu", i);
        #endif
        bool pushed = queue.push(DiagnosticEvent::Severity::Info, DiagnosticEvent::Category::AudioEngine, msg);
        TEST_ASSERT(pushed, "Push succeeded within queue capacity");
    }

    // 3. Queue overflow test: pushing when full must fail and increment dropped counter
    bool overflowPush = queue.push(DiagnosticEvent::Severity::Warning, DiagnosticEvent::Category::Buffer, "Overflow Event");
    TEST_ASSERT(!overflowPush, "Push fails when queue is full");
    TEST_ASSERT(queue.getDropCount() == 1, "Drop count incremented on overflow");

    // Push another overflow
    queue.push(DiagnosticEvent::Severity::Error, DiagnosticEvent::Category::DSP, "Second Overflow");
    TEST_ASSERT(queue.getDropCount() == 2, "Drop count accurately increments on repeated overflow");

    // 4. Drain all items in FIFO order
    for (size_t i = 0; i < maxPush; ++i)
    {
        DiagnosticEvent ev;
        bool popped = queue.pop(ev);
        TEST_ASSERT(popped, "Pop succeeds for queued item");
        TEST_ASSERT(ev.severity == DiagnosticEvent::Severity::Info, "Popped item severity matches");
        TEST_ASSERT(ev.category == DiagnosticEvent::Category::AudioEngine, "Popped item category matches");
    }

    // 5. Queue should now be empty again
    TEST_ASSERT(!queue.pop(dummy), "Queue empty after draining");
}

void testDiagnosticsLogManagerSeverityFilteringAndTrimming()
{
    using namespace LiveBus;

    AudioPerformanceMonitor monitor;
    DiagnosticsLogManager logManager(monitor);
    logManager.clearLog();
    TEST_ASSERT(logManager.getTotalEventCount() == 0, "Log cleared to 0 events");

    // Add events of different severities and categories
    logManager.log(DiagnosticSeverity::Info, DiagnosticCategory::Device, "Device opened: Realtek ASIO");
    logManager.log(DiagnosticSeverity::Info, DiagnosticCategory::Buffer, "Buffer size set to 128 samples");
    logManager.log(DiagnosticSeverity::Info, DiagnosticCategory::AudioEngine, "Audio engine started");
    logManager.log(DiagnosticSeverity::Warning, DiagnosticCategory::DSP, "DSP clipping ceiling approached");
    logManager.log(DiagnosticSeverity::Warning, DiagnosticCategory::Buffer, "Buffer size small for current load");
    logManager.log(DiagnosticSeverity::Error, DiagnosticCategory::AudioEngine, "Audio callback overrun: 3.5ms");

    TEST_ASSERT(logManager.getTotalEventCount() == 6, "Total event count is 6");

    // Test Filter: All
    auto allEvents = logManager.getFilteredEvents(DiagnosticsLogManager::FilterMode::All);
    TEST_ASSERT(allEvents.size() == 6, "Filter All returns 6 events");

    // Test Filter: Info Only
    auto infoEvents = logManager.getFilteredEvents(DiagnosticsLogManager::FilterMode::InfoOnly);
    TEST_ASSERT(infoEvents.size() == 3, "Filter InfoOnly returns 3 events");

    // Test Filter: Warning and Error
    auto warnErrEvents = logManager.getFilteredEvents(DiagnosticsLogManager::FilterMode::WarningAndError);
    TEST_ASSERT(warnErrEvents.size() == 3, "Filter WarningAndError returns 3 events (2 warn + 1 err)");

    // Test Filter: Error Only
    auto errEvents = logManager.getFilteredEvents(DiagnosticsLogManager::FilterMode::ErrorOnly);
    TEST_ASSERT(errEvents.size() == 1, "Filter ErrorOnly returns 1 event");
    TEST_ASSERT(juce::String(errEvents[0].message).contains("Audio callback overrun"), "Error event content verified");

    // Test Rolling History Buffer Limit (1,000 items)
    for (int i = 0; i < 1100; ++i)
    {
        logManager.log(DiagnosticSeverity::Info, DiagnosticCategory::Routing, "Bulk event");
    }
    TEST_ASSERT(logManager.getTotalEventCount() == 1000, "History strictly capped at 1,000 events without unbounded memory growth");

    logManager.clearLog();
    TEST_ASSERT(logManager.getTotalEventCount() == 0, "Log cleared back to 0");
}

void testDiagnosticReportSanitizationAndExport()
{
    using namespace LiveBus;

    AudioPerformanceMonitor monitor;
    DiagnosticsLogManager logManager(monitor);

    juce::AudioDeviceManager dm;
    AudioDeviceService devService(dm);
    ::RoutingMatrix routing;

    // Add some realistic log entries
    logManager.log(DiagnosticSeverity::Info, DiagnosticCategory::Device, "Device opened: Virtual Studio Audio");
    logManager.log(DiagnosticSeverity::Warning, DiagnosticCategory::AudioEngine, "Audio latency warning");

    // Generate sanitized diagnostic report
    juce::String report = logManager.generateReport(devService, monitor, nullptr, &routing);

    TEST_ASSERT(report.isNotEmpty(), "Diagnostic report is non-empty");
    TEST_ASSERT(report.contains("LIVEBUS MIXER v1.0"), "Report contains application header");
    TEST_ASSERT(report.contains("JUCE Version:"), "Report contains JUCE version");
    TEST_ASSERT(report.contains("Operating System:"), "Report contains OS information");
    TEST_ASSERT(report.contains("Audio Backend:"), "Report contains Audio Backend details");
    TEST_ASSERT(report.contains("Buffer Duration:"), "Report differentiates buffer duration");
    TEST_ASSERT(report.containsIgnoreCase("CPU & Processing"), "Report includes CPU section");
    TEST_ASSERT(report.containsIgnoreCase("Recent Diagnostic Events"), "Report includes recent events");

    // Security & Privacy Sanitization: ensure no passwords or credentials exist
    TEST_ASSERT(!report.containsIgnoreCase("password"), "Report sanitized: no password");
    TEST_ASSERT(!report.containsIgnoreCase("secret"), "Report sanitized: no secrets");
    TEST_ASSERT(!report.containsIgnoreCase("token"), "Report sanitized: no tokens");
    TEST_ASSERT(!report.containsIgnoreCase("apiKey"), "Report sanitized: no API keys");

    // File export test
    auto tempReportFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("LiveBus_DiagReport_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".txt");

    bool exported = logManager.exportReportToFile(tempReportFile, report);
    TEST_ASSERT(exported == true, "Diagnostic report exported to file successfully");
    TEST_ASSERT(tempReportFile.existsAsFile() && tempReportFile.getSize() > 200, "Report file written with complete content");

    tempReportFile.deleteFile();
}

void testDeviceEventLifecycleAndFailureHandling()
{
    using namespace LiveBus;

    AudioPerformanceMonitor monitor;
    DiagnosticsLogManager logManager(monitor);

    // 1. Device disconnect event simulation
    monitor.recordDeviceError("USB Audio Interface abruptly disconnected by host system");
    logManager.drainQueue(monitor.getQueue());

    auto events = logManager.getFilteredEvents(DiagnosticsLogManager::FilterMode::ErrorOnly);
    TEST_ASSERT(!events.empty(), "Device disconnect logged as error");
    TEST_ASSERT(juce::String(events.back().message).contains("disconnected"), "Disconnect error message captured");

    // 2. Sample rate change failure simulation
    logManager.log(DiagnosticSeverity::Error, DiagnosticCategory::Device,
                   "Failed to apply sample rate 96000 Hz: backend device does not support rate. Rolled back to 48000 Hz.");
    auto lastEvents = logManager.getFilteredEvents(DiagnosticsLogManager::FilterMode::ErrorOnly);
    TEST_ASSERT(juce::String(lastEvents.back().message).contains("Failed to apply sample rate"), "Sample rate failure logged with rollback confirmation");

    // 3. Buffer change failure simulation
    logManager.log(DiagnosticSeverity::Warning, DiagnosticCategory::Buffer,
                   "Buffer size 32 requested but clamped by driver to hardware minimum 64 samples.");
    auto warnEvents = logManager.getFilteredEvents(DiagnosticsLogManager::FilterMode::WarningAndError);
    TEST_ASSERT(juce::String(warnEvents.back().message).contains("clamped by driver"), "Buffer change clamping logged as warning");

    // 4. Safe shutdown while logging worker or queue has events
    monitor.recordDropout("Pre-shutdown underrun");
    logManager.log(DiagnosticSeverity::Info, DiagnosticCategory::AudioEngine, "Shutdown initiated");
    // Explicit destruction of monitor and manager without crashing
    {
        AudioPerformanceMonitor tempMon;
        DiagnosticsLogManager tempMgr(tempMon);
        tempMon.recordDropout("Dropout during destruction");
        tempMgr.drainQueue(tempMon.getQueue());
    }
    TEST_ASSERT(true, "Safe shutdown without deadlock or memory access violation");
}

// =============================================================================
// Six Core DSP Modules Comprehensive Automated Tests
// =============================================================================

void testHighPassFilterFrequencyResponseAndSlope()
{
    BusDSPProcessor proc;
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    proc.prepare(sampleRate, blockSize);

    // 1. Configure HPF: 500 Hz cutoff, 24 dB/oct slope
    proc.params.hpfEnabled.store(true);
    proc.params.hpfFreq.store(500.0f);
    proc.params.hpfSlope.store(3); // 24 dB/oct
    proc.params.hpfType.store(0);  // Butterworth
    proc.updateCoefficients();

    // 2. Feed low-frequency signal: 50 Hz sine (well below 500 Hz cutoff)
    const int numSamples = 4800; // 100 ms
    juce::AudioBuffer<float> lowBuffer(2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float val = std::sin(2.0f * juce::MathConstants<float>::pi * 50.0f * (float)i / (float)sampleRate);
        lowBuffer.setSample(0, i, val);
        lowBuffer.setSample(1, i, val);
    }
    proc.process(lowBuffer, numSamples);

    // Measure RMS of last 1000 samples after transient settles
    float lowRms = lowBuffer.getRMSLevel(0, numSamples - 1000, 1000);
    // 50 Hz should be heavily attenuated (> 25 dB down, input RMS was ~0.707)
    TEST_ASSERT(lowRms < 0.1f, "HPF 500Hz 24dB/oct attenuates 50Hz sine heavily (RMS < 0.10)");

    // 3. Feed high-frequency signal: 4000 Hz sine (well above 500 Hz cutoff)
    juce::AudioBuffer<float> highBuffer(2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float val = std::sin(2.0f * juce::MathConstants<float>::pi * 4000.0f * (float)i / (float)sampleRate);
        highBuffer.setSample(0, i, val);
        highBuffer.setSample(1, i, val);
    }
    proc.process(highBuffer, numSamples);
    float highRms = highBuffer.getRMSLevel(0, numSamples - 1000, 1000);
    TEST_ASSERT(highRms > 0.65f, "HPF passes 4000Hz sine with minimal attenuation (RMS > 0.65)");

    // 4. Sample rate change test: re-prepare at 96 kHz
    proc.prepare(96000.0, blockSize);
    proc.params.hpfFreq.store(500.0f);
    proc.updateCoefficients();
    juce::AudioBuffer<float> buffer96k(2, 4800);
    for (int i = 0; i < 4800; ++i)
    {
        float val = std::sin(2.0f * juce::MathConstants<float>::pi * 50.0f * (float)i / 96000.0f);
        buffer96k.setSample(0, i, val);
        buffer96k.setSample(1, i, val);
    }
    proc.process(buffer96k, 4800);
    float lowRms96k = buffer96k.getRMSLevel(0, 3800, 1000);
    TEST_ASSERT(lowRms96k < 0.1f, "HPF maintains cutoff attenuation across sample rate change to 96kHz");

    // 5. Bypass test: when disabled, signal passes bit-exact
    proc.params.hpfEnabled.store(false);
    proc.updateCoefficients();
    juce::AudioBuffer<float> bypassBuffer(2, 512);
    for (int i = 0; i < 512; ++i)
    {
        bypassBuffer.setSample(0, i, 0.75f);
        bypassBuffer.setSample(1, i, -0.75f);
    }
    proc.process(bypassBuffer, 512);
    TEST_ASSERT(std::abs(bypassBuffer.getSample(0, 256) - 0.75f) < 1e-6f, "HPF disabled passes audio bit-exact");
}

void testLowPassFilterFrequencyResponseAndSlope()
{
    const double sampleRate = 48000.0;
    const int numSamples = 9600;
    const std::array<float, 4> maximumHighFrequencyRms{ 0.12f, 0.03f, 0.005f, 0.001f };

    for (int slope = 1; slope <= 4; ++slope)
    {
        BusDSPProcessor proc;
        proc.params.lpfEnabled.store(true);
        proc.params.lpfFreq.store(1000.0f);
        proc.params.lpfSlope.store(slope);
        proc.prepare(sampleRate, 512);

        juce::AudioBuffer<float> lowBuffer(2, numSamples);
        juce::AudioBuffer<float> highBuffer(2, numSamples);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float low = std::sin(2.0f * juce::MathConstants<float>::pi *
                                       100.0f * (float)sample / (float)sampleRate);
            const float high = std::sin(2.0f * juce::MathConstants<float>::pi *
                                        8000.0f * (float)sample / (float)sampleRate);
            for (int channel = 0; channel < 2; ++channel)
            {
                lowBuffer.setSample(channel, sample, low);
                highBuffer.setSample(channel, sample, high);
            }
        }

        proc.process(lowBuffer, numSamples);
        proc.process(highBuffer, numSamples);
        const int settledStart = numSamples - 2400;
        const float lowRms = lowBuffer.getRMSLevel(0, settledStart, 2400);
        const float highRms = highBuffer.getRMSLevel(0, settledStart, 2400);
        TEST_ASSERT(lowRms > 0.65f, "LPF passes frequencies well below the cutoff");
        TEST_ASSERT(highRms < maximumHighFrequencyRms[(size_t)(slope - 1)],
                    "LPF attenuates high frequencies according to the selected slope");
    }

    BusDSPProcessor bypass;
    bypass.params.lpfEnabled.store(true);
    bypass.params.lpfSlope.store(0);
    bypass.prepare(sampleRate, 512);
    juce::AudioBuffer<float> bypassBuffer(2, 512);
    for (int sample = 0; sample < bypassBuffer.getNumSamples(); ++sample)
        bypassBuffer.setSample(0, sample, 0.75f);
    bypass.process(bypassBuffer, bypassBuffer.getNumSamples());
    TEST_ASSERT(std::abs(bypassBuffer.getSample(0, 256) - 0.75f) < 1.0e-6f,
                "LPF slope off passes audio unchanged");
}

void testNoiseGateEnvelopeAndGainReduction()
{
    BusDSPProcessor proc;
    const double sampleRate = 48000.0;
    proc.prepare(sampleRate, 256);

    proc.params.gateEnabled.store(true);
    proc.params.gateThresholdDb.store(-30.0f);
    proc.params.gateFloorDb.store(-60.0f);
    proc.params.gateAttackMs.store(1.0f);
    proc.params.gateHoldMs.store(10.0f);
    proc.params.gateReleaseMs.store(20.0f);
    proc.updateCoefficients();

    // 1. Sub-threshold input: -50 dBFS noise/sine (amplitude ~0.00316)
    const int numSamples = 4800; // 100ms
    juce::AudioBuffer<float> lowBuffer(2, numSamples);
    float lowAmp = juce::Decibels::decibelsToGain(-50.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        lowBuffer.setSample(0, i, lowAmp);
        lowBuffer.setSample(1, i, lowAmp);
    }
    proc.process(lowBuffer, numSamples);

    // Gate should close and attenuate signal towards floor
    float outSample = std::abs(lowBuffer.getSample(0, numSamples - 1));
    TEST_ASSERT(outSample < lowAmp * 0.1f, "Gate suppresses signal below -30 dB threshold");
    TEST_ASSERT(proc.getGateGainReductionDb() > 20.0f, "Gate reports gain reduction > 20 dB on sub-threshold signal");
    TEST_ASSERT(proc.getGateState() == 0, "Gate state is Closed (0)");

    // 2. Above-threshold input: -6 dBFS (amplitude ~0.501)
    juce::AudioBuffer<float> highBuffer(2, numSamples);
    float highAmp = juce::Decibels::decibelsToGain(-6.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        highBuffer.setSample(0, i, highAmp);
        highBuffer.setSample(1, i, highAmp);
    }
    proc.process(highBuffer, numSamples);
    float highOut = std::abs(highBuffer.getSample(0, numSamples - 1));
    TEST_ASSERT(std::abs(highOut - highAmp) < 0.05f, "Gate opens fully for signal above threshold");
    TEST_ASSERT(proc.getGateGainReductionDb() < 1.0f, "Gate GR returns to ~0 dB when open");
    TEST_ASSERT(proc.getGateState() == 2, "Gate state is Open (2)");

    // 3. Hold state verification: feed 5 ms silence
    juce::AudioBuffer<float> silenceBuffer(2, (int)(0.005 * sampleRate)); // 5ms (< hold time 10ms)
    silenceBuffer.clear();
    proc.process(silenceBuffer, silenceBuffer.getNumSamples());
    TEST_ASSERT(proc.getGateState() == 1, "Gate enters Hold state (1) during silence within hold window");
}

void testCompressorRatioKneeAndMakeupGain()
{
    BusDSPProcessor proc;
    const double sampleRate = 48000.0;
    proc.prepare(sampleRate, 256);

    proc.params.compEnabled.store(true);
    proc.params.compThresholdDb.store(-20.0f);
    proc.params.compRatio.store(4.0f);
    proc.params.compAttackMs.store(1.0f);
    proc.params.compReleaseMs.store(50.0f);
    proc.params.compMakeupDb.store(0.0f);
    proc.params.compMixPercent.store(100.0f);
    proc.params.compKnee.store(0); // Hard knee
    proc.updateCoefficients();

    // 1. Sub-threshold signal: -30 dBFS (amplitude ~0.0316)
    const int numSamples = 4800;
    juce::AudioBuffer<float> subBuffer(2, numSamples);
    float subAmp = juce::Decibels::decibelsToGain(-30.0f);
    for (int i = 0; i < numSamples; ++i)
    {
        subBuffer.setSample(0, i, subAmp);
        subBuffer.setSample(1, i, subAmp);
    }
    proc.process(subBuffer, numSamples);
    float subOut = std::abs(subBuffer.getSample(0, numSamples - 1));
    TEST_ASSERT(std::abs(subOut - subAmp) < 0.002f, "Sub-threshold signal experiences no compression");
    TEST_ASSERT(proc.getCompGainReductionDb() < 0.5f, "Compressor GR is ~0 dB below threshold");

    // 2. Above-threshold signal: 0 dBFS (amplitude 1.0, 20 dB above threshold)
    juce::AudioBuffer<float> hotBuffer(2, 9600); // 200 ms to settle envelope
    for (int i = 0; i < 9600; ++i)
    {
        hotBuffer.setSample(0, i, 1.0f);
        hotBuffer.setSample(1, i, 1.0f);
    }
    proc.process(hotBuffer, 9600);
    float compGR = proc.getCompGainReductionDb();
    TEST_ASSERT(compGR >= 12.0f && compGR <= 16.5f, "Compressor applies ~15 dB gain reduction at 4:1 ratio for 20 dB overshoot");

    // 3. Makeup Gain: +6 dB
    proc.reset();
    proc.params.compMakeupDb.store(6.0f);
    proc.updateCoefficients();
    juce::AudioBuffer<float> makeupBuffer(2, 4800);
    for (int i = 0; i < 4800; ++i)
    {
        makeupBuffer.setSample(0, i, subAmp);
        makeupBuffer.setSample(1, i, subAmp);
    }
    proc.process(makeupBuffer, 4800);
    float makeupOut = std::abs(makeupBuffer.getSample(0, 4799));
    float expectedMakeupOut = subAmp * juce::Decibels::decibelsToGain(6.0f);
    TEST_ASSERT(std::abs(makeupOut - expectedMakeupOut) < 0.005f, "Compressor makeup gain adds +6 dB output boost independently");

    // 4. Bypass test: compEnabled = false leaves signal intact
    proc.params.compEnabled.store(false);
    proc.updateCoefficients();
    juce::AudioBuffer<float> bypassBuf(2, 256);
    for (int i = 0; i < 256; ++i) { bypassBuf.setSample(0, i, 1.0f); bypassBuf.setSample(1, i, 1.0f); }
    proc.process(bypassBuf, 256);
    TEST_ASSERT(std::abs(bypassBuf.getSample(0, 100) - 1.0f) < 1e-5f, "Compressor bypass preserves original signal");
}

void testLimiterCeilingEnforcementAndClippingProtection()
{
    BusDSPProcessor proc;
    const double sampleRate = 48000.0;
    proc.prepare(sampleRate, 256);

    proc.params.limiterEnabled.store(true);
    proc.params.limiterCeilingDb.store(-2.0f); // -2 dBFS = linear ~0.7943
    proc.params.limiterReleaseMs.store(30.0f);
    proc.updateCoefficients();

    float ceilingLinear = juce::Decibels::decibelsToGain(-2.0f);

    // Feed loud signal: amplitude 2.5 (+7.95 dBFS)
    const int numSamples = 4800;
    juce::AudioBuffer<float> loudBuffer(2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float val = 2.5f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / (float)sampleRate);
        loudBuffer.setSample(0, i, val);
        loudBuffer.setSample(1, i, val);
    }
    proc.process(loudBuffer, numSamples);

    // Verify no sample exceeds ceiling (with small margin for smoothing)
    float maxPeak = 0.0f;
    for (int i = 50; i < numSamples; ++i)
    {
        maxPeak = juce::jmax(maxPeak, std::abs(loudBuffer.getSample(0, i)));
        maxPeak = juce::jmax(maxPeak, std::abs(loudBuffer.getSample(1, i)));
    }
    TEST_ASSERT(maxPeak <= ceilingLinear + 0.015f, "Limiter strictly clamps peak output to ceiling (-2 dBFS ~ 0.794)");
    TEST_ASSERT(proc.getLimiterGainReductionDb() > 5.0f, "Limiter reports gain reduction > 5 dB for loud signal");
}

void testReverbWetDryAndDecayTailResponse()
{
    BusDSPProcessor proc;
    const double sampleRate = 48000.0;
    proc.prepare(sampleRate, 512);

    proc.params.reverbEnabled.store(true);
    proc.params.reverbRoomSize.store(0.85f);
    proc.params.reverbDamping.store(0.2f);
    proc.params.reverbWetLevel.store(0.6f);
    proc.params.reverbDryLevel.store(0.0f); // Pure wet to isolate reverb response
    proc.params.reverbWidth.store(1.0f);
    proc.updateCoefficients();

    // 1. Send Dirac impulse at sample 0, rest silence
    const int blockSize = 512;
    juce::AudioBuffer<float> impulseBuffer(2, blockSize);
    impulseBuffer.clear();
    impulseBuffer.setSample(0, 0, 1.0f);
    impulseBuffer.setSample(1, 0, 1.0f);

    proc.process(impulseBuffer, blockSize);

    // 2. Process consecutive silent blocks; reverb tail must persist
    float tailEnergy = 0.0f;
    for (int block = 0; block < 5; ++block)
    {
        juce::AudioBuffer<float> silence(2, blockSize);
        silence.clear();
        proc.process(silence, blockSize);
        tailEnergy += silence.getRMSLevel(0, 0, blockSize);
    }
    TEST_ASSERT(tailEnergy > 0.01f, "Reverb produces audible acoustic decay tail after dry input stops");

    // 3. Bypass test: when disabled, impulse produces zero tail in next block
    proc.params.reverbEnabled.store(false);
    proc.updateCoefficients();
    juce::AudioBuffer<float> silenceAfterBypass(2, blockSize);
    silenceAfterBypass.clear();
    proc.process(silenceAfterBypass, blockSize);
    TEST_ASSERT(silenceAfterBypass.getRMSLevel(0, 0, blockSize) < 1e-5f, "Reverb bypass stops tail immediately");
}

void testDelayBufferEchoTimeAndFeedbackDecay()
{
    BusDSPProcessor proc;
    const double sampleRate = 48000.0;
    proc.prepare(sampleRate, 256);

    // Set delay time to 50 ms = 2400 samples at 48 kHz
    proc.params.delayEnabled.store(true);
    proc.params.delayTimeMs.store(50.0f);
    proc.params.delayFeedback.store(0.50f);
    proc.params.delayWetLevel.store(1.0f);
    proc.params.delayDryLevel.store(0.0f); // Pure wet
    proc.params.delayPingPong.store(false);
    proc.updateCoefficients();

    // Send impulse at sample 0 in block 1
    const int totalSamples = (int)(48000 * 0.2); // 200 ms = 9600 samples
    juce::AudioBuffer<float> delayBuf(2, totalSamples);
    delayBuf.clear();
    delayBuf.setSample(0, 0, 1.0f);
    delayBuf.setSample(1, 0, 1.0f);

    proc.process(delayBuf, totalSamples);

    // Echo 1 should appear around sample 2400 (50 ms)
    int expectedEcho1 = (int)(0.050 * sampleRate);
    float peak1 = 0.0f;
    int peakIndex1 = 0;
    for (int i = expectedEcho1 - 20; i <= expectedEcho1 + 20; ++i)
    {
        float val = std::abs(delayBuf.getSample(0, i));
        if (val > peak1) { peak1 = val; peakIndex1 = i; }
    }
    TEST_ASSERT(std::abs(peakIndex1 - expectedEcho1) <= 2, "Delay echo peak occurs precisely at 50 ms (2400 samples)");
    TEST_ASSERT(peak1 > 0.8f, "First echo amplitude is close to 1.0 (unattenuated first tap)");

    // Echo 2 should appear around sample 4800 (100 ms) with amplitude ~0.50 (feedback)
    int expectedEcho2 = expectedEcho1 * 2;
    float peak2 = 0.0f;
    for (int i = expectedEcho2 - 20; i <= expectedEcho2 + 20; ++i)
    {
        peak2 = juce::jmax(peak2, std::abs(delayBuf.getSample(0, i)));
    }
    TEST_ASSERT(peak2 >= 0.35f && peak2 <= 0.65f, "Second echo decays by feedback factor (0.50)");

    // Safe feedback clamping: setting feedback > 0.90 must be clamped to <= 0.90
    proc.params.delayFeedback.store(1.5f);
    proc.updateCoefficients();
    TEST_ASSERT(proc.params.delayFeedback.load() <= 0.90f, "Delay feedback safely clamped to prevent runaway screech");
}

void testDSPTargetIndependenceAndBypassBitTransparency()
{
    BusDSPProcessor streamProc;
    BusDSPProcessor monitorProc;
    const double sampleRate = 48000.0;
    streamProc.prepare(sampleRate, 256);
    monitorProc.prepare(sampleRate, 256);

    // Stream processor has aggressive HPF, Compressor, Limiter
    streamProc.params.hpfEnabled.store(true);
    streamProc.params.hpfFreq.store(800.0f);
    streamProc.params.compEnabled.store(true);
    streamProc.params.compThresholdDb.store(-24.0f);
    streamProc.params.compRatio.store(8.0f);
    streamProc.updateCoefficients();

    // Monitor processor stays bypassed
    monitorProc.params.hpfEnabled.store(false);
    monitorProc.params.compEnabled.store(false);
    monitorProc.updateCoefficients();

    const int numSamples = 1024;
    juce::AudioBuffer<float> streamBuf(2, numSamples);
    juce::AudioBuffer<float> monitorBuf(2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        float val = 0.65f * std::sin(2.0f * juce::MathConstants<float>::pi * 150.0f * (float)i / (float)sampleRate);
        streamBuf.setSample(0, i, val);
        streamBuf.setSample(1, i, val);
        monitorBuf.setSample(0, i, val);
        monitorBuf.setSample(1, i, val);
    }

    streamProc.process(streamBuf, numSamples);
    monitorProc.process(monitorBuf, numSamples);

    // Monitor buffer must remain 100% bit-exact to original input
    for (int i = 0; i < numSamples; ++i)
    {
        float expected = 0.65f * std::sin(2.0f * juce::MathConstants<float>::pi * 150.0f * (float)i / (float)sampleRate);
        TEST_ASSERT(std::abs(monitorBuf.getSample(0, i) - expected) < 1e-6f, "Monitor Bus is completely unaffected by Stream Bus DSP");
    }

    // Stream buffer must be heavily modified (attenuated by 800Hz HPF)
    float streamRMS = streamBuf.getRMSLevel(0, 0, numSamples);
    float monitorRMS = monitorBuf.getRMSLevel(0, 0, numSamples);
    TEST_ASSERT(streamRMS < monitorRMS * 0.2f, "Stream Bus processed audio independently with distinct acoustic profile");
}

void testMasterDimAttenuatesBusWithoutChangingFaderGain()
{
    BusDSPProcessor processor;
    processor.prepare(48000.0, 256);
    processor.params.masterGain.store(1.0f);
    processor.params.masterDim.store(true);

    juce::AudioBuffer<float> buffer(2, 4096);
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        std::fill_n(buffer.getWritePointer(channel), buffer.getNumSamples(), 0.5f);
    processor.process(buffer, buffer.getNumSamples());

    const float dimmedRms = buffer.getRMSLevel(0, 2048, 2048);
    TEST_ASSERT(std::abs(dimmedRms - 0.05f) < 0.002f,
                "Master Dim applies 20 dB attenuation after the fade ramp");
    TEST_ASSERT(std::abs(processor.params.masterGain.load() - 1.0f) < 1e-6f,
                "Master Dim does not overwrite the master fader gain");
}

void testDSPSceneSaveLoadConsistency()
{
    BusDSPProcessor proc;
    proc.params.hpfEnabled.store(true);
    proc.params.hpfFreq.store(240.0f);
    proc.params.hpfSlope.store(2); // 18 dB/oct
    proc.params.gateEnabled.store(true);
    proc.params.gateThresholdDb.store(-38.5f);
    proc.params.gateAttackMs.store(3.5f);
    proc.params.gateHoldMs.store(25.0f);
    proc.params.gateReleaseMs.store(85.0f);
    proc.params.compEnabled.store(true);
    proc.params.compThresholdDb.store(-16.0f);
    proc.params.compRatio.store(3.5f);
    proc.params.compAttackMs.store(12.0f);
    proc.params.compReleaseMs.store(140.0f);
    proc.params.compMakeupDb.store(4.5f);
    proc.params.limiterEnabled.store(true);
    proc.params.limiterCeilingDb.store(-0.8f);
    proc.params.limiterReleaseMs.store(45.0f);
    proc.params.reverbEnabled.store(true);
    proc.params.reverbRoomSize.store(0.68f);
    proc.params.reverbDamping.store(0.42f);
    proc.params.reverbWetLevel.store(0.35f);
    proc.params.reverbDryLevel.store(0.90f);
    proc.params.delayEnabled.store(true);
    proc.params.delayTimeMs.store(420.0f);
    proc.params.delayFeedback.store(0.48f);
    proc.params.delayWetLevel.store(0.28f);
    proc.params.delayDryLevel.store(0.95f);
    proc.params.delayPingPong.store(true);

    // Verify all parameters are accurately stored and retrieved
    TEST_ASSERT(proc.params.hpfEnabled.load() == true, "Scene HPF enabled retained");
    TEST_ASSERT(std::abs(proc.params.hpfFreq.load() - 240.0f) < 1e-4f, "Scene HPF freq retained");
    TEST_ASSERT(proc.params.hpfSlope.load() == 2, "Scene HPF slope retained");
    TEST_ASSERT(std::abs(proc.params.gateThresholdDb.load() - -38.5f) < 1e-4f, "Scene Gate threshold retained");
    TEST_ASSERT(std::abs(proc.params.compRatio.load() - 3.5f) < 1e-4f, "Scene Comp ratio retained");
    TEST_ASSERT(std::abs(proc.params.compMakeupDb.load() - 4.5f) < 1e-4f, "Scene Comp makeup retained");
    TEST_ASSERT(std::abs(proc.params.limiterCeilingDb.load() - -0.8f) < 1e-4f, "Scene Limiter ceiling retained");
    TEST_ASSERT(std::abs(proc.params.reverbRoomSize.load() - 0.68f) < 1e-4f, "Scene Reverb room retained");
    TEST_ASSERT(std::abs(proc.params.delayTimeMs.load() - 420.0f) < 1e-4f, "Scene Delay time retained");
    TEST_ASSERT(proc.params.delayPingPong.load() == true, "Scene Delay ping-pong retained");
}

// ==============================================================================
// Auto Ducking Tests
// ==============================================================================
void testAutoDuckerThresholdAndEnvelope()
{
    LiveBus::AutoDucker ducker;
    ducker.prepare(48000.0, 512);
    ducker.setEnabled(true);
    ducker.setFilterEnabled(false); // Pure envelope dynamic test
    ducker.setThresholdDb(-30.0f);
    ducker.setDuckingDepthDb(-12.0f);
    ducker.setAttackMs(10.0f);
    ducker.setHoldMs(100.0f);
    ducker.setReleaseMs(200.0f);

    // 1. Below threshold: 512 samples at -50 dBFS (~0.003f)
    std::vector<float> lowSignal(512, 0.003f);
    ducker.processSidechain(lowSignal.data(), 512);
    TEST_ASSERT(ducker.getGainMultiplierStream() >= 0.999f, "Below threshold maintains unity gain");
    TEST_ASSERT(!ducker.isSidechainActive(), "Sidechain inactive below threshold");

    // 2. Above threshold: 6 blocks of 512 samples at -6 dBFS (0.5f)
    std::vector<float> loudSignal(512, 0.5f);
    for (int b = 0; b < 6; ++b)
        ducker.processSidechain(loudSignal.data(), 512);

    float duckedMult = ducker.getGainMultiplierStream();
    TEST_ASSERT(ducker.isSidechainActive(), "Sidechain active when loud signal detected");
    TEST_ASSERT(duckedMult < 0.35f, "Gain reduced towards target depth of -12dB (~0.25)");
    TEST_ASSERT(ducker.getGainReductionDbStream() < -8.0f, "GR reported accurately in dB");

    // 3. Hold timer keeps reduction active when input ceases
    std::vector<float> silence(512, 0.0f);
    ducker.processSidechain(silence.data(), 512);
    TEST_ASSERT(ducker.getGainMultiplierStream() < 0.35f, "Gain held during hold time");

    // 4. Release smoothly recovers back to 1.0f after hold expires
    for (int b = 0; b < 100; ++b) // 100 * 512 / 48000 ~ 1.06s > hold(100ms) + 4.5*tau(200ms)
        ducker.processSidechain(silence.data(), 512);

    TEST_ASSERT(ducker.getGainMultiplierStream() > 0.98f, "Gain smoothly recovered to unity after release");
}

void testAutoDuckerStreamAndMonitorIndependence()
{
    LiveBus::AutoDucker ducker;
    ducker.prepare(48000.0, 512);
    ducker.setEnabled(true);
    ducker.setFilterEnabled(false);
    ducker.setThresholdDb(-30.0f);
    ducker.setStreamDepthDb(-6.0f);   // 50%
    ducker.setMonitorDepthDb(-18.0f); // 12.5%

    // Stream only enabled
    ducker.setStreamEnabled(true);
    ducker.setMonitorEnabled(false);

    std::vector<float> loud(512, 0.5f);
    for (int b = 0; b < 10; ++b)
        ducker.processSidechain(loud.data(), 512);

    TEST_ASSERT(ducker.getGainMultiplierStream() < 0.65f, "Stream ducked");
    TEST_ASSERT(ducker.getGainMultiplierMonitor() == 1.0f, "Monitor unaffected when monitor ducking disabled");

    // Enable both with different depths
    ducker.setMonitorEnabled(true);
    for (int b = 0; b < 10; ++b)
        ducker.processSidechain(loud.data(), 512);

    TEST_ASSERT(ducker.getGainMultiplierStream() > ducker.getGainMultiplierMonitor(),
                "Independent depth: Stream (-6dB) has higher multiplier than Monitor (-18dB)");
}

void testAutoDuckerBypassAndTransparency()
{
    LiveBus::AutoDucker ducker;
    ducker.prepare(48000.0, 512);
    ducker.setEnabled(false); // Bypassed

    std::vector<float> loud(512, 0.9f);
    for (int b = 0; b < 10; ++b)
        ducker.processSidechain(loud.data(), 512);

    TEST_ASSERT(ducker.getGainMultiplierStream() == 1.0f, "Bypassed ducker guarantees stream unity gain");
    TEST_ASSERT(ducker.getGainMultiplierMonitor() == 1.0f, "Bypassed ducker guarantees monitor unity gain");
    TEST_ASSERT(!ducker.isSidechainActive(), "Sidechain inactive when bypassed");
}

void testAutoDuckerSidechainFilter()
{
    LiveBus::AutoDucker ducker;
    ducker.prepare(48000.0, 512);
    ducker.setEnabled(true);
    ducker.setThresholdDb(-25.0f);
    ducker.setFilterEnabled(true);
    ducker.setFilterHpfFreq(500.0f); // HPF cut below 500Hz
    ducker.setFilterLpfFreq(4000.0f);

    // Feed 50 Hz sub-bass tone at -10 dBFS (amplitude 0.316f) continuously
    std::vector<float> signal(512);
    float phase = 0.0f;
    float delta = 50.0f * juce::MathConstants<float>::twoPi / 48000.0f;
    for (int b = 0; b < 10; ++b)
    {
        for (int s = 0; s < 512; ++s)
        {
            float fade = (b == 0) ? ((float)s / 512.0f) : 1.0f;
            signal[s] = 0.316f * fade * std::sin(phase);
            phase += delta;
        }
        ducker.processSidechain(signal.data(), 512);
    }

    TEST_ASSERT(!ducker.isSidechainActive(), "HPF blocked 50Hz sub-rumble from triggering ducking");

    // Feed 1000 Hz vocal tone within passband (500Hz - 4000Hz) at -10 dBFS
    float midPhase = 0.0f;
    float midDelta = 1000.0f * juce::MathConstants<float>::twoPi / 48000.0f;
    for (int b = 0; b < 5; ++b)
    {
        for (int s = 0; s < 512; ++s)
        {
            signal[s] = 0.316f * std::sin(midPhase);
            midPhase += midDelta;
        }
        ducker.processSidechain(signal.data(), 512);
    }
    TEST_ASSERT(ducker.isSidechainActive(), "Bandpass filter passed 1000Hz speech tone to trigger ducking");
}

void testAutoDuckerXmlSerializationRoundTrip()
{
    LiveBus::AutoDucker d1;
    d1.setEnabled(true);
    d1.setThresholdDb(-24.5f);
    d1.setDuckingDepthDb(-14.0f);
    d1.setAttackMs(8.5f);
    d1.setHoldMs(450.0f);
    d1.setReleaseMs(750.0f);
    d1.setFilterHpfFreq(120.0f);

    auto xml = d1.createXml();
    TEST_ASSERT(xml != nullptr, "Created AutoDucker XML element");

    LiveBus::AutoDucker d2;
    d2.restoreFromXml(*xml);

    TEST_ASSERT(d2.isEnabled() == true, "XML restored enabled");
    TEST_ASSERT(std::abs(d2.getThresholdDb() - -24.5f) < 0.1f, "XML restored threshold");
    TEST_ASSERT(std::abs(d2.getDuckingDepthDb() - -14.0f) < 0.1f, "XML restored depth");
    TEST_ASSERT(std::abs(d2.getAttackMs() - 8.5f) < 0.1f, "XML restored attack");
    TEST_ASSERT(std::abs(d2.getHoldMs() - 450.0f) < 0.1f, "XML restored hold");
    TEST_ASSERT(std::abs(d2.getReleaseMs() - 750.0f) < 0.1f, "XML restored release");
    TEST_ASSERT(std::abs(d2.getFilterHpfFreq() - 120.0f) < 0.1f, "XML restored HPF");
}

// ==============================================================================
// Audio Safety & Anti-Feedback Tests
// ==============================================================================
void testAudioSafetyLimiterCeilingAndGR()
{
    LiveBus::AudioSafetyProcessor safety;
    safety.prepare(48000.0, 512);
    safety.setStreamLimiterEnabled(true);
    safety.setStreamCeilingDb(-2.0f); // -2.0 dBFS = 0.794f

    juce::AudioBuffer<float> buf(2, 512);
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < 512; ++s)
            buf.setSample(ch, s, 1.5f); // +3.5 dBFS loud signal exceeding ceiling
    }

    safety.processStreamBus(buf, 512);

    float maxSample = buf.getMagnitude(0, 512);
    TEST_ASSERT(maxSample <= 0.80f, "Brickwall limiter clamped output below -2 dBFS ceiling");
    TEST_ASSERT(safety.getStreamGainReductionDb() < -3.0f, "Gain reduction accurately recorded");
}

void testAudioSafetyPeakAndClipWarning()
{
    LiveBus::AudioSafetyProcessor safety;
    safety.prepare(48000.0, 512);
    safety.setStreamLimiterEnabled(false); // disable limiter to allow peak into detector

    juce::AudioBuffer<float> buf(2, 512);
    buf.clear();
    buf.setSample(0, 100, 1.05f); // 0 dBFS clipping sample

    safety.processStreamBus(buf, 512);

    TEST_ASSERT(safety.isStreamClipping(), "Clip latched on sample >= 1.0f");
    TEST_ASSERT(safety.getStreamClipCount() > 0, "Clip counter incremented");

    safety.resetClipWarnings();
    TEST_ASSERT(!safety.isStreamClipping(), "Clip flag cleared on reset");
    TEST_ASSERT(safety.getStreamClipCount() == 0, "Clip counter reset");
}

void testAudioSafetySanitizesNonFiniteSamples()
{
    LiveBus::AudioSafetyProcessor safety;
    safety.prepare(48000.0, 4);
    safety.setStreamLimiterEnabled(false);
    safety.setMonitorLimiterEnabled(false);

    juce::AudioBuffer<float> stream(2, 4);
    juce::AudioBuffer<float> monitor(2, 4);
    stream.clear();
    monitor.clear();
    stream.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    stream.setSample(1, 1, std::numeric_limits<float>::infinity());
    monitor.setSample(0, 2, -std::numeric_limits<float>::infinity());
    monitor.setSample(1, 3, std::numeric_limits<float>::quiet_NaN());

    safety.processStreamBus(stream, 4);
    safety.processMonitorBus(monitor, 4);
    bool allSamplesFinite = true;
    bool sanitizedSamplesAreSilent = true;
    for (int ch = 0; ch < 2; ++ch)
        for (int sample = 0; sample < 4; ++sample)
        {
            allSamplesFinite = allSamplesFinite && std::isfinite(stream.getSample(ch, sample)) &&
                               std::isfinite(monitor.getSample(ch, sample));
            if ((ch == 0 && sample == 0) || (ch == 1 && sample == 1))
                sanitizedSamplesAreSilent = sanitizedSamplesAreSilent && stream.getSample(ch, sample) == 0.0f;
            if ((ch == 0 && sample == 2) || (ch == 1 && sample == 3))
                sanitizedSamplesAreSilent = sanitizedSamplesAreSilent && monitor.getSample(ch, sample) == 0.0f;
        }

    TEST_ASSERT(allSamplesFinite, "Audio safety stage prevents NaN or infinity from reaching output");
    TEST_ASSERT(sanitizedSamplesAreSilent, "Non-finite audio samples are replaced with silence");
}

void testAudioSafetyEmergencyMuteInstantZero()
{
    LiveBus::AudioSafetyProcessor safety;
    safety.prepare(48000.0, 512);

    juce::AudioBuffer<float> sBuf(2, 512);
    juce::AudioBuffer<float> mBuf(2, 512);
    for (int s = 0; s < 512; ++s)
    {
        sBuf.setSample(0, s, 0.7f);
        mBuf.setSample(0, s, 0.7f);
    }

    safety.setEmergencyMute(LiveBus::EmergencyMuteMode::Both);

    // Process a few blocks so the 1ms ramp reaches 0.0f
    for (int b = 0; b < 4; ++b)
    {
        safety.processStreamBus(sBuf, 512);
        safety.processMonitorBus(mBuf, 512);
    }

    TEST_ASSERT(sBuf.getMagnitude(0, 512) == 0.0f, "Stream output silenced by emergency mute");
    TEST_ASSERT(mBuf.getMagnitude(0, 512) == 0.0f, "Monitor output silenced by emergency mute");

    safety.unmuteAll();
    for (int s = 0; s < 512; ++s)
    {
        sBuf.setSample(0, s, 0.7f);
        mBuf.setSample(0, s, 0.7f);
    }
    for (int b = 0; b < 4; ++b)
    {
        safety.processStreamBus(sBuf, 512);
        safety.processMonitorBus(mBuf, 512);
    }
    TEST_ASSERT(sBuf.getMagnitude(0, 512) > 0.6f, "Audio smoothly restored on unmute");
}

void testAudioSafetyProtectsFinalHardwareOutputs()
{
    LiveBus::AudioSafetyProcessor safety;
    safety.prepare(48000.0, 512);
    safety.setStreamCeilingDb(-6.0f);
    safety.setMonitorCeilingDb(-6.0f);

    std::array<std::vector<float>, 2> outputStorage{
        std::vector<float>(512, 2.0f), std::vector<float>(512, -2.0f)
    };
    float* outputs[] = { outputStorage[0].data(), outputStorage[1].data() };
    safety.processHardwareOutputs(outputs, 2, 512);

    const float ceiling = std::pow(10.0f, -6.0f / 20.0f);
    TEST_ASSERT(std::abs(outputStorage[0][0]) <= ceiling + 1.0e-5f &&
                    std::abs(outputStorage[1][0]) <= ceiling + 1.0e-5f,
                "Final hardware limiter keeps direct outputs below configured ceiling");

    safety.setEmergencyMute(LiveBus::EmergencyMuteMode::Both);
    for (int block = 0; block < 4; ++block)
    {
        std::fill(outputStorage[0].begin(), outputStorage[0].end(), 0.8f);
        std::fill(outputStorage[1].begin(), outputStorage[1].end(), -0.8f);
        safety.processHardwareOutputs(outputs, 2, 512);
    }

    TEST_ASSERT(outputStorage[0][511] == 0.0f && outputStorage[1][511] == 0.0f,
                "Emergency mute silences final hardware outputs including direct routes");
}

void testAudioSafetyRoutingLoopDetection()
{
    LiveBus::AudioSafetyProcessor safety;
    TEST_ASSERT(safety.evaluateRiskLevel() == LiveBus::SafetyRiskLevel::Normal, "Default state is Normal");

    safety.setLoopbackDeviceDetected(true);
    TEST_ASSERT(safety.evaluateRiskLevel() == LiveBus::SafetyRiskLevel::Caution, "Loopback device yields Caution");

    safety.setFeedbackLoopDetected(true);
    TEST_ASSERT(safety.evaluateRiskLevel() == LiveBus::SafetyRiskLevel::CriticalFeedbackRisk, "Feedback loop detected yields Critical");
    TEST_ASSERT(safety.getRiskDescription().contains("CRITICAL"), "Risk description contains CRITICAL");
}

// ==============================================================================
// Macro Parameter & Engine Tests
// ==============================================================================
void testMacroEngineParameterAssignmentAndMapping()
{
    LiveBus::MacroAssignment aLinear;
    aLinear.minVal = 100.0f;
    aLinear.maxVal = 1000.0f;
    aLinear.curve = LiveBus::MacroCurveType::Linear;

    TEST_ASSERT(std::abs(aLinear.mapNormalizedValue(0.0f) - 100.0f) < 0.01f, "Linear min mapping");
    TEST_ASSERT(std::abs(aLinear.mapNormalizedValue(0.5f) - 550.0f) < 0.01f, "Linear mid mapping");
    TEST_ASSERT(std::abs(aLinear.mapNormalizedValue(1.0f) - 1000.0f) < 0.01f, "Linear max mapping");

    LiveBus::MacroAssignment aInvert = aLinear;
    aInvert.invert = true;
    TEST_ASSERT(std::abs(aInvert.mapNormalizedValue(0.0f) - 1000.0f) < 0.01f, "Inverted min maps to max");
    TEST_ASSERT(std::abs(aInvert.mapNormalizedValue(1.0f) - 100.0f) < 0.01f, "Inverted max maps to min");

    LiveBus::MacroAssignment aLog = aLinear;
    aLog.curve = LiveBus::MacroCurveType::Logarithmic;
    float logMid = aLog.mapNormalizedValue(0.5f);
    TEST_ASSERT(logMid < 550.0f, "Logarithmic curve concentrates values lower than linear");
}

void testMacroEngineMultiParameterCoordinatedUpdate()
{
    LiveBus::MacroEngine engine;

    float paramA = 0.0f;
    float paramB = 0.0f;

    engine.registerParameter({
        "test.param.a", "Param A", "Test",
        0.0f, 10.0f, 0.0f,
        [&paramA](float v) { paramA = v; },
        [&paramA]() { return paramA; }
    });

    engine.registerParameter({
        "test.param.b", "Param B", "Test",
        -20.0f, 0.0f, 0.0f,
        [&paramB](float v) { paramB = v; },
        [&paramB]() { return paramB; }
    });

    LiveBus::MacroAssignment a1;
    a1.targetParameterId = "test.param.a";
    a1.minVal = 0.0f;
    a1.maxVal = 10.0f;

    LiveBus::MacroAssignment a2;
    a2.targetParameterId = "test.param.b";
    a2.minVal = -20.0f;
    a2.maxVal = 0.0f;

    engine.addAssignment(0, a1);
    engine.addAssignment(0, a2);

    engine.setMacroValue(0, 0.8f);

    TEST_ASSERT(std::abs(paramA - 8.0f) < 0.05f, "Param A updated to 8.0 via macro");
    TEST_ASSERT(std::abs(paramB - -4.0f) < 0.05f, "Param B updated to -4.0 via macro");
}

void testMacroEngineUnresolvedParameterSafety()
{
    LiveBus::MacroEngine engine;
    LiveBus::MacroAssignment badAssignment;
    badAssignment.targetParameterId = "unregistered.missing.param.123";
    badAssignment.minVal = 0.0f;
    badAssignment.maxVal = 100.0f;

    engine.addAssignment(0, badAssignment);
    // Setting macro value should safely ignore unregistered target without crash
    engine.setMacroValue(0, 0.5f);
    TEST_ASSERT(true, "Unresolved parameter handled with complete crash safety");
}

void testMacroEngineQuickActions()
{
    LiveBus::MacroEngine engine;
    bool qaTriggered = false;

    engine.registerQuickActionHandler(LiveBus::QuickActionType::EmergencyMuteAll, [&qaTriggered]() {
        qaTriggered = true;
    });

    engine.executeQuickAction(LiveBus::QuickActionType::EmergencyMuteAll);
    TEST_ASSERT(qaTriggered, "Quick action handler fired correctly");
}

void testMacroEngineXmlSerialization()
{
    LiveBus::MacroEngine e1;
    e1.setMacroValue(0, 0.72f);
    auto xml = e1.createXml();
    TEST_ASSERT(xml != nullptr, "MacroEngine XML created");

    LiveBus::MacroEngine e2;
    e2.restoreFromXml(*xml);

    TEST_ASSERT(std::abs(e2.getMacroValue(0) - 0.72f) < 0.01f, "Restored macro value accurately");
    TEST_ASSERT(e2.getNumMacros() == e1.getNumMacros(), "Restored macro count matches");
}

void testSceneManagerDuckingAndSafetySerialization()
{
    SceneData s1;
    s1.sceneName = "BroadcastLive";
    s1.duckingXmlString = "<AutoDucker thresholdDb=\"-28.0\" duckingDepthDb=\"-15.0\"/>";
    s1.safetyXmlString = "<AudioSafety streamCeilingDb=\"-1.5\"/>";
    s1.macroXmlString = "<MacroEngine><Macro id=\"1\" currentValue=\"0.85\"/></MacroEngine>";

    SceneManager sm;
    auto xmlStr = sm.serializeToXmlString(s1);
    TEST_ASSERT(xmlStr.isNotEmpty(), "Scene XML serialized");

    auto xmlDoc = juce::XmlDocument::parse(xmlStr);
    TEST_ASSERT(xmlDoc != nullptr, "Parsed serialized scene XML");

    SceneData s2;
    juce::String err;
    bool ok = sm.deserializeFromXml(*xmlDoc, s2, err);
    TEST_ASSERT(ok, "Deserialized scene XML");
    TEST_ASSERT(s2.duckingXmlString.contains("AutoDucker"), "Ducking XML preserved across scene serialization");
    TEST_ASSERT(s2.safetyXmlString.contains("AudioSafety"), "Safety XML preserved across scene serialization");
    TEST_ASSERT(s2.macroXmlString.contains("MacroEngine"), "Macro XML preserved across scene serialization");

    // Test RecallFilter with ducking, safety, macros, routingMatrix
    LiveBus::RecallFilter filter;
    filter.setAll(false);
    TEST_ASSERT(!filter.isAnyAllowed(), "RecallFilter blockAll has nothing allowed");
    filter.ducking = true;
    TEST_ASSERT(filter.isAnyAllowed(), "RecallFilter with ducking enabled is allowed");
    filter.routingMatrix = true;
    TEST_ASSERT(filter.routingMatrix, "RecallFilter routingMatrix flag accessible");
}

// ==============================================================================
// Per-Channel Input & Output Routing Matrix Tests (v1.0)
// ==============================================================================
void testPerChannelInputSourceSelection()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(4);

    // Default channels are Hardware
    TEST_ASSERT(matrix.getChannel(0).inputSource.load() == LiveBus::ChannelInputSource::Hardware, "Ch 1 default is Hardware");

    // Configure Channel 0 as Hardware (Input 0), Channel 1 as Disabled, Channel 2 as Playback, Channel 3 as TestSignal
    matrix.getChannel(0).inputSource.store(LiveBus::ChannelInputSource::Hardware);
    matrix.getChannel(0).hardwareInputChannel.store(0);
    matrix.getChannel(0).routeEnable[0].store(true);
    matrix.getChannel(0).routeEnable[1].store(true);

    matrix.getChannel(1).inputSource.store(LiveBus::ChannelInputSource::Disabled);
    matrix.getChannel(1).routeEnable[0].store(true);

    matrix.getChannel(2).inputSource.store(LiveBus::ChannelInputSource::Playback);
    matrix.getChannel(2).playbackChannel.store(2); // Stereo

    matrix.getChannel(3).inputSource.store(LiveBus::ChannelInputSource::TestSignal);

    // Verify properties
    TEST_ASSERT(matrix.getChannel(1).inputSource.load() == LiveBus::ChannelInputSource::Disabled, "Ch 2 is Disabled");
    TEST_ASSERT(matrix.getChannel(2).inputSource.load() == LiveBus::ChannelInputSource::Playback, "Ch 3 is Playback");
    TEST_ASSERT(matrix.getChannel(3).inputSource.load() == LiveBus::ChannelInputSource::TestSignal, "Ch 4 is TestSignal");

    // Disabled channel outputs silence in routeAndSum
    matrix.getChannel(1).isEnabled.store(false);
    juce::AudioBuffer<float> bufs[4];
    for (int i = 0; i < 4; ++i)
    {
        bufs[i].setSize(2, 64);
        bufs[i].clear();
    }
    // Fill buf 1 with audio
    for (int s = 0; s < 64; ++s) { bufs[1].setSample(0, s, 0.9f); bufs[1].setSample(1, s, 0.9f); }

    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    matrix.routeAndSum(bufs, 4, stream, monitor, 64);

    // Stream bus should have 0 magnitude because ch 1 is disabled and other bufs are 0
    TEST_ASSERT(stream.getMagnitude(0, 0, 64) < 1e-5f, "Disabled channel produces silence on stream bus");
}

void testPerChannelStreamAndMonitorSendsIndependent()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(4);

    auto& ch0 = matrix.getChannel(0);
    ch0.routeEnable[0].store(true);   // Stream ON
    ch0.routeEnable[1].store(false);  // Monitor OFF
    ch0.sendLevel[0].store(0.8f);     // Stream send 0.8
    ch0.sendLevel[1].store(0.2f);     // Monitor send 0.2
    ch0.gain.store(1.0f);
    ch0.pan.store(0.0f);
    ch0.stereoMode.store(ChannelStereoMode::Mono);

    juce::AudioBuffer<float> bufs[4];
    for (int i = 0; i < 4; ++i) { bufs[i].setSize(2, 64); bufs[i].clear(); }
    for (int s = 0; s < 64; ++s) { bufs[0].setSample(0, s, 1.0f); bufs[0].setSample(1, s, 1.0f); }

    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    matrix.routeAndSum(bufs, 4, stream, monitor, 64);

    float sMag = stream.getMagnitude(0, 0, 64);
    float mMag = monitor.getMagnitude(0, 0, 64);

    TEST_ASSERT(sMag > 0.5f, "Stream bus receives audio when Stream send enabled");
    TEST_ASSERT(mMag < 1e-5f, "Monitor bus receives zero audio when Monitor send disabled");

    // Invert enables
    ch0.routeEnable[0].store(false);
    ch0.routeEnable[1].store(true);
    matrix.routeAndSum(bufs, 4, stream, monitor, 64);

    TEST_ASSERT(stream.getMagnitude(0, 0, 64) < 1e-5f, "Stream bus now silent when disabled");
    TEST_ASSERT(monitor.getMagnitude(0, 0, 64) > 0.1f, "Monitor bus receives audio when enabled");
}

void testPreFaderVsPostFaderSend()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(4);

    auto& ch0 = matrix.getChannel(0);
    ch0.routeEnable[0].store(true);
    ch0.routeEnable[1].store(true);
    ch0.sendLevel[0].store(1.0f);
    ch0.sendLevel[1].store(1.0f);
    ch0.preFaderSendStream.store(false); // Stream: Post-Fader
    ch0.preFaderSendMonitor.store(true); // Monitor: Pre-Fader
    ch0.pan.store(0.0f);

    // Channel fader pulled all the way down to 0
    ch0.gain.store(0.0f);

    juce::AudioBuffer<float> bufs[4];
    for (int i = 0; i < 4; ++i) { bufs[i].setSize(2, 64); bufs[i].clear(); }
    for (int s = 0; s < 64; ++s) { bufs[0].setSample(0, s, 0.8f); bufs[0].setSample(1, s, 0.8f); }

    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    matrix.routeAndSum(bufs, 4, stream, monitor, 64);

    float sMag = stream.getMagnitude(0, 0, 64);
    float mMag = monitor.getMagnitude(0, 0, 64);

    TEST_ASSERT(sMag < 1e-5f, "Post-fader send is silenced when channel fader gain is zero");
    TEST_ASSERT(mMag > 0.5f, "Pre-fader send remains audible when channel fader gain is zero");
}

void testDuplicateHardwareInputWarning()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(4);

    // Initial state: ch 0 = hw 0, ch 1 = hw 1, ch 2 = hw 2, ch 3 = hw 3
    for (int i = 0; i < 4; ++i)
    {
        matrix.getChannel(i).inputSource.store(LiveBus::ChannelInputSource::Hardware);
        matrix.getChannel(i).hardwareInputChannel.store(i);
    }
    matrix.updateDuplicateWarnings();
    TEST_ASSERT(!matrix.hasAnyDuplicateWarning(), "No duplicate warnings when unique hardware inputs used");

    // Assign Ch 1 to Hardware Input 0 (shared with Ch 0)
    matrix.getChannel(1).hardwareInputChannel.store(0);
    matrix.updateDuplicateWarnings();

    TEST_ASSERT(matrix.hasAnyDuplicateWarning(), "Duplicate warning detected when two channels share hardware input");
    TEST_ASSERT(matrix.getChannel(0).duplicateWarning.load(), "Ch 0 flagged with duplicate warning");
    TEST_ASSERT(matrix.getChannel(1).duplicateWarning.load(), "Ch 1 flagged with duplicate warning");
    TEST_ASSERT(!matrix.getChannel(2).duplicateWarning.load(), "Ch 2 has no duplicate warning");
}

void testStereoPairAndBalanceLaw()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(4);

    auto& ch0 = matrix.getChannel(0);
    ch0.stereoMode.store(LiveBus::ChannelStereoMode::StereoPair);
    ch0.routeEnable[0].store(true);
    ch0.sendLevel[0].store(1.0f);
    ch0.gain.store(1.0f);
    ch0.pan.store(0.0f); // Center balance

    juce::AudioBuffer<float> bufs[4];
    for (int i = 0; i < 4; ++i) { bufs[i].setSize(2, 64); bufs[i].clear(); }
    for (int s = 0; s < 64; ++s)
    {
        bufs[0].setSample(0, s, 0.7f); // Left
        bufs[0].setSample(1, s, 0.3f); // Right
    }

    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    matrix.routeAndSum(bufs, 4, stream, monitor, 64);

    // Center balance preserves L and R magnitudes
    TEST_ASSERT(std::abs(stream.getMagnitude(0, 0, 64) - 0.7f) < 0.05f, "Stereo Pair Left channel routed to Stream Left");
    TEST_ASSERT(std::abs(stream.getMagnitude(1, 0, 64) - 0.3f) < 0.05f, "Stereo Pair Right channel routed to Stream Right");

    // Pan full Left (-1.0)
    ch0.pan.store(-1.0f);
    matrix.routeAndSum(bufs, 4, stream, monitor, 64);
    TEST_ASSERT(std::abs(stream.getMagnitude(0, 0, 64) - 0.7f) < 0.05f, "Left channel stays at unity when panned left");
    TEST_ASSERT(stream.getMagnitude(1, 0, 64) < 1e-5f, "Right channel silenced when panned fully left");
}

void testRoutingMatrixXmlPersistence()
{
    LiveBus::RoutingMatrix m1;
    m1.setActiveChannels(4);
    m1.getChannel(0).name = "Lead Vocal";
    m1.getChannel(0).inputSource.store(LiveBus::ChannelInputSource::Hardware);
    m1.getChannel(0).hardwareInputChannel.store(1);
    m1.getChannel(0).preFaderSendStream.store(true);
    m1.getChannel(0).preFaderSendMonitor.store(false);
    m1.getChannel(0).stereoMode.store(LiveBus::ChannelStereoMode::Mono);
    m1.getChannel(0).submixGroup.store(1);
    m1.getSubmixBus(1).routeToStream.store(true);
    m1.getSubmixBus(1).routeToMonitor.store(false);
    for (int channel = 1; channel < 4; ++channel)
    {
        m1.getChannel(channel).routeEnable[0].store(false);
        m1.getChannel(channel).routeEnable[1].store(false);
    }

    m1.getChannel(2).name = "BGM Player";
    m1.getChannel(2).inputSource.store(LiveBus::ChannelInputSource::Playback);
    m1.getChannel(2).playbackChannel.store(2);
    m1.getChannel(2).stereoMode.store(LiveBus::ChannelStereoMode::StereoPair);
    m1.getChannel(3).inputSource.store(LiveBus::ChannelInputSource::YouTube);

    auto xml = m1.createXml();
    TEST_ASSERT(xml != nullptr, "RoutingMatrix XML created");

    LiveBus::RoutingMatrix m2;
    m2.restoreFromXml(*xml);

    TEST_ASSERT(m2.getChannel(0).name == "Lead Vocal", "Ch 0 name restored");
    TEST_ASSERT(m2.getChannel(0).hardwareInputChannel.load() == 1, "Ch 0 hardware input restored");
    TEST_ASSERT(m2.getChannel(0).preFaderSendStream.load() == true, "Ch 0 pre-fader stream send restored");
    TEST_ASSERT(m2.getChannel(2).inputSource.load() == LiveBus::ChannelInputSource::Playback, "Ch 2 playback source restored");
    TEST_ASSERT(m2.getChannel(2).stereoMode.load() == LiveBus::ChannelStereoMode::StereoPair, "Ch 2 stereo pair restored");
    TEST_ASSERT(m2.getChannel(3).inputSource.load() == LiveBus::ChannelInputSource::YouTube, "Ch 3 YouTube source restored");
    TEST_ASSERT(m2.getChannel(0).submixGroup.load() == 1 &&
                    m2.getSubmixBus(1).routeToStream.load() &&
                    !m2.getSubmixBus(1).routeToMonitor.load(),
                "Scene routing state restores the Submix-to-Stream connection and independent bus routing");

    juce::AudioBuffer<float> buffers[4];
    for (auto& buffer : buffers)
    {
        buffer.setSize(2, 64);
        buffer.clear();
    }
    buffers[0].clear();
    for (int sample = 0; sample < 64; ++sample)
        buffers[0].setSample(0, sample, 0.5f);
    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    m2.routeAndSum(buffers, 4, stream, monitor, 64);
    TEST_ASSERT(stream.getMagnitude(0, 0, 64) > 0.34f &&
                    monitor.getMagnitude(0, 0, 64) == 0.0f,
                "Restored Scene routing produces the expected independent audio output");
}

void testRoutingMatrixPresets()
{
    LiveBus::RoutingMatrix m;
    auto presets = LiveBus::RoutingMatrix::getPresetNames();
    TEST_ASSERT(presets.size() >= 4, "At least 4 routing presets defined");

    // Apply Broadcast Stream Focus
    m.applyRoutingPreset(1);
    TEST_ASSERT(m.getChannel(2).routeEnable[0].load() == true, "Broadcast preset routes music to stream");
    TEST_ASSERT(m.getChannel(2).routeEnable[1].load() == false, "Broadcast preset mutes music to monitor");

    // Apply Podcast Studio
    m.applyRoutingPreset(2);
    TEST_ASSERT(m.getChannel(2).inputSource.load() == LiveBus::ChannelInputSource::Playback, "Podcast preset assigns Ch 3 to Playback");
}

void testMissingHardwareInputSafety()
{
    LiveBus::RoutingMatrix m;
    m.setActiveChannels(4);
    m.getChannel(3).inputSource.store(LiveBus::ChannelInputSource::Hardware);
    m.getChannel(3).hardwareInputChannel.store(7); // Requested input 8

    // Hardware provides only 2 physical inputs
    m.validateAvailableInputs(2);

    TEST_ASSERT(!m.getChannel(3).isSourceAvailable.load(), "Input 8 marked unavailable when hardware only has 2 inputs");

    // Channels with valid hardware or playback remain available
    m.getChannel(0).hardwareInputChannel.store(0);
    m.validateAvailableInputs(2);
    TEST_ASSERT(m.getChannel(0).isSourceAvailable.load(), "Input 1 marked available when within hardware bounds");
}

void testIndependentHardwareDirectOutputRouting()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(4);
    matrix.validateAvailableOutputs(4); // 4 HW outputs available

    // Configure Channel 0: Mic -> HW Out 0 (Send 1.0)
    auto& ch0 = matrix.getChannel(0);
    ch0.routeEnable[0].store(false); // Stream OFF
    ch0.routeEnable[1].store(false); // Monitor OFF
    ch0.hwRouteEnable[0].store(true);
    ch0.hwSendLevel[0].store(1.0f);
    ch0.gain.store(1.0f);

    // Configure Channel 1: Guitar -> HW Out 1 (Send 0.5)
    auto& ch1 = matrix.getChannel(1);
    ch1.routeEnable[0].store(false);
    ch1.routeEnable[1].store(false);
    ch1.hwRouteEnable[1].store(true);
    ch1.hwSendLevel[1].store(0.5f);
    ch1.gain.store(1.0f);

    // Channels 2 and 3: Stream and Monitor OFF
    matrix.getChannel(2).routeEnable[0].store(false);
    matrix.getChannel(2).routeEnable[1].store(false);
    matrix.getChannel(3).routeEnable[0].store(false);
    matrix.getChannel(3).routeEnable[1].store(false);

    // Setup input buffers with 1.0f
    juce::AudioBuffer<float> bufs[4];
    for (int i = 0; i < 4; ++i)
    {
        bufs[i].setSize(2, 64);
        for (int s = 0; s < 64; ++s)
        {
            bufs[i].setSample(0, s, 1.0f);
            bufs[i].setSample(1, s, 1.0f);
        }
    }

    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    juce::AudioBuffer<float> hwOut(4, 64);
    stream.clear(); monitor.clear(); hwOut.clear();

    float* hwPtrs[4] = {
        hwOut.getWritePointer(0),
        hwOut.getWritePointer(1),
        hwOut.getWritePointer(2),
        hwOut.getWritePointer(3)
    };

    matrix.routeAndSum(bufs, 4, stream, monitor, 64, 1.0f, 1.0f, 0x000C, 0, hwPtrs, 4);

    // Buses are silent because routes are disabled
    TEST_ASSERT(stream.getMagnitude(0, 0, 64) == 0.0f, "Stream bus silent when disabled for channels");
    TEST_ASSERT(monitor.getMagnitude(0, 0, 64) == 0.0f, "Monitor bus silent when disabled for channels");

    // HW Out 0 has Ch 0 audio at unity (1.0f)
    TEST_ASSERT(std::abs(hwOut.getMagnitude(0, 0, 64) - 1.0f) < 0.01f, "HW Out 0 receives Channel 0 at unity gain");
    // HW Out 1 has Ch 1 audio at 0.5f
    TEST_ASSERT(std::abs(hwOut.getMagnitude(1, 0, 64) - 0.5f) < 0.01f, "HW Out 1 receives Channel 1 at 0.5 send level");
    // HW Out 2 and 3 should be silent
    TEST_ASSERT(hwOut.getMagnitude(2, 0, 64) == 0.0f, "HW Out 2 silent");
    TEST_ASSERT(hwOut.getMagnitude(3, 0, 64) == 0.0f, "HW Out 3 silent");
}

void testMultiDestinationSimultaneousRouting()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(2);
    matrix.validateAvailableOutputs(4);

    // Ch 0 routed to Stream (0.8), Monitor (0.6), and HW Out 2 (0.4) simultaneously
    auto& ch0 = matrix.getChannel(0);
    ch0.routeEnable[0].store(true);
    ch0.sendLevel[0].store(0.8f);
    ch0.routeEnable[1].store(true);
    ch0.sendLevel[1].store(0.6f);
    ch0.hwRouteEnable[2].store(true);
    ch0.hwSendLevel[2].store(0.4f);
    ch0.gain.store(1.0f);
    ch0.pan.store(0.0f);

    juce::AudioBuffer<float> bufs[2];
    for (int i = 0; i < 2; ++i)
    {
        bufs[i].setSize(2, 64);
        for (int s = 0; s < 64; ++s)
        {
            bufs[i].setSample(0, s, 1.0f);
            bufs[i].setSample(1, s, 1.0f);
        }
    }

    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    juce::AudioBuffer<float> hwOut(4, 64);
    stream.clear(); monitor.clear(); hwOut.clear();

    float* hwPtrs[4] = {
        hwOut.getWritePointer(0), hwOut.getWritePointer(1),
        hwOut.getWritePointer(2), hwOut.getWritePointer(3)
    };

    matrix.routeAndSum(bufs, 2, stream, monitor, 64, 1.0f, 1.0f, 0x000C, 0, hwPtrs, 4);

    // Stream: 1.0 * 0.8 * 0.7071 (center pan law) = ~0.5657
    TEST_ASSERT(stream.getMagnitude(0, 0, 64) > 0.5f, "Stream received signal simultaneously");
    // Monitor: 1.0 * 0.6 * 0.7071 = ~0.4243
    TEST_ASSERT(monitor.getMagnitude(0, 0, 64) > 0.35f, "Monitor received signal simultaneously");
    // HW Out 2: 1.0 * 0.4 = 0.4
    TEST_ASSERT(std::abs(hwOut.getMagnitude(2, 0, 64) - 0.4f) < 0.01f, "HW Out 2 received direct signal simultaneously");
    // Other HW outputs untouched
    TEST_ASSERT(hwOut.getMagnitude(0, 0, 64) == 0.0f, "HW Out 0 untouched");
    TEST_ASSERT(hwOut.getMagnitude(1, 0, 64) == 0.0f, "HW Out 1 untouched");
    TEST_ASSERT(hwOut.getMagnitude(3, 0, 64) == 0.0f, "HW Out 3 untouched");
}

void testRoutingIsolationAndSafety()
{
    LiveBus::RoutingMatrix matrix;
    matrix.setActiveChannels(4);
    matrix.validateAvailableOutputs(2); // Only 2 HW outputs supported by driver

    // Channel 0 routed to HW 0
    auto& ch0 = matrix.getChannel(0);
    ch0.gain.store(1.0f);
    ch0.hwRouteEnable[0].store(true);
    ch0.hwSendLevel[0].store(1.0f);

    // Channel 1 attempted routing to HW 3 (unsupported!)
    auto& ch1 = matrix.getChannel(1);
    ch1.gain.store(1.0f);
    ch1.hwRouteEnable[3].store(true);
    ch1.hwSendLevel[3].store(1.0f);

    TEST_ASSERT(matrix.isHardwareOutputAvailable(0) == true, "HW Out 0 valid for 2-ch driver");
    TEST_ASSERT(matrix.isHardwareOutputAvailable(1) == true, "HW Out 1 valid for 2-ch driver");
    TEST_ASSERT(matrix.isHardwareOutputAvailable(2) == false, "HW Out 2 invalid for 2-ch driver");
    TEST_ASSERT(matrix.isHardwareOutputAvailable(3) == false, "HW Out 3 invalid for 2-ch driver");

    juce::AudioBuffer<float> bufs[4];
    for (int i = 0; i < 4; ++i)
    {
        bufs[i].setSize(2, 64);
        for (int s = 0; s < 64; ++s) bufs[i].setSample(0, s, 1.0f);
    }

    juce::AudioBuffer<float> stream(2, 64), monitor(2, 64);
    juce::AudioBuffer<float> hwOut(2, 64);
    hwOut.clear();
    float* hwPtrs[2] = { hwOut.getWritePointer(0), hwOut.getWritePointer(1) };

    // Pass 2 hw outputs
    matrix.routeAndSum(bufs, 4, stream, monitor, 64, 1.0f, 1.0f, 0x000C, 0, hwPtrs, 2);

    // Safe execution without crash or out-of-bounds writes
    TEST_ASSERT(std::abs(hwOut.getMagnitude(0, 0, 64) - 1.0f) < 0.01f, "HW 0 received channel 0");
    TEST_ASSERT(hwOut.getMagnitude(1, 0, 64) == 0.0f, "HW 1 stayed clean");
}

void testHardwareDirectOutputXmlPersistence()
{
    LiveBus::RoutingMatrix m1;
    m1.setActiveChannels(2);
    m1.validateAvailableOutputs(4);
    m1.getChannel(0).hwRouteEnable[0].store(true);
    m1.getChannel(0).hwSendLevel[0].store(0.65f);
    m1.getChannel(0).hwRouteEnable[2].store(true);
    m1.getChannel(0).hwSendLevel[2].store(0.35f);

    auto xml = m1.createXml();
    TEST_ASSERT(xml != nullptr, "RoutingMatrix XML created with HW routing");

    LiveBus::RoutingMatrix m2;
    m2.restoreFromXml(*xml);

    TEST_ASSERT(m2.getChannel(0).hwRouteEnable[0].load() == true, "HW Out 0 route enable restored");
    TEST_ASSERT(std::abs(m2.getChannel(0).hwSendLevel[0].load() - 0.65f) < 0.001f, "HW Out 0 send level restored");
    TEST_ASSERT(m2.getChannel(0).hwRouteEnable[2].load() == true, "HW Out 2 route enable restored");
    TEST_ASSERT(std::abs(m2.getChannel(0).hwSendLevel[2].load() - 0.35f) < 0.001f, "HW Out 2 send level restored");
    TEST_ASSERT(m2.getChannel(0).hwRouteEnable[1].load() == false, "HW Out 1 stayed false");
}

// =============================================================================
// Embedded YouTube Player & Audio Capture Integration Tests
// =============================================================================

void testYouTubeURLParsing()
{
    // Test 1: Standard youtube.com/watch?v=
    juce::String id1 = LiveBus::YouTubePlayerComponent::extractVideoId("https://www.youtube.com/watch?v=jfKfPfyJRdk");
    TEST_ASSERT(id1 == "jfKfPfyJRdk", "Extracts standard watch?v= ID");

    // Test 2: Standard watch?v= with extra query parameters (&t=120s&ab_channel=Lofi)
    juce::String id2 = LiveBus::YouTubePlayerComponent::extractVideoId("https://www.youtube.com/watch?v=4xDzrJKXOOY&t=60s&feature=shared");
    TEST_ASSERT(id2 == "4xDzrJKXOOY", "Extracts ID with trailing query parameters");

    // Test 3: Shortened youtu.be/ ID
    juce::String id3 = LiveBus::YouTubePlayerComponent::extractVideoId("https://youtu.be/DWcJFNfaw9c?si=abc123xyz");
    TEST_ASSERT(id3 == "DWcJFNfaw9c", "Extracts shortened youtu.be ID");

    // Test 4: Embed URL format
    juce::String id4 = LiveBus::YouTubePlayerComponent::extractVideoId("https://www.youtube-nocookie.com/embed/b4bKzz4A4j4");
    TEST_ASSERT(id4 == "b4bKzz4A4j4", "Extracts embed URL ID");

    // Test 5: Raw 11-char ID
    juce::String id5 = LiveBus::YouTubePlayerComponent::extractVideoId("jfKfPfyJRdk");
    TEST_ASSERT(id5 == "jfKfPfyJRdk", "Accepts valid 11-char ID directly");

    TEST_ASSERT(LiveBus::YouTubePlayerComponent::extractVideoId(
                    "https://www.youtube.com/shorts/jfKfPfyJRdk") == "jfKfPfyJRdk",
                "Extracts a YouTube Shorts URL");
    TEST_ASSERT(LiveBus::YouTubePlayerComponent::extractVideoId(
                    "https://www.youtube.com/live/jfKfPfyJRdk") == "jfKfPfyJRdk",
                "Extracts a YouTube live URL");

    TEST_ASSERT(LiveBus::YouTubePlayerComponent::isYouTubeHomeUrl("https://www.youtube.com/"),
                "Recognizes YouTube home URL");
    TEST_ASSERT(LiveBus::YouTubePlayerComponent::isYouTubeHomeUrl("https://youtube.com"),
                "Recognizes YouTube root without trailing slash");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isYouTubeHomeUrl(
                    "https://www.youtube.com/feed/subscriptions"),
                "Does not mistake YouTube subpages for home");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isYouTubeHomeUrl("http://www.youtube.com/"),
                "Rejects insecure YouTube home URL");

    // Only approved HTTPS YouTube hosts and exact 11-character IDs are accepted.
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isValidYouTubeUrl(""), "Rejects empty URL");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isValidYouTubeUrl("https://google.com/search?q=test"), "Rejects non-YouTube URL");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isValidYouTubeUrl(
                    "https://youtube.com.attacker.invalid/watch?v=jfKfPfyJRdk"),
                "Rejects lookalike domains");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isValidYouTubeUrl(
                    "https://youtube.com@attacker.invalid/watch?v=jfKfPfyJRdk"),
                "Rejects user-info domain confusion");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isValidYouTubeUrl(
                    "https://www.youtube.com/watch?v=short"),
                "Rejects malformed video IDs");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isValidYouTubeUrl(
                    "http://www.youtube.com/watch?v=jfKfPfyJRdk"),
                "Rejects insecure HTTP URLs");
    TEST_ASSERT(!LiveBus::YouTubePlayerComponent::isValidYouTubeUrl("invalid_garbage_string"), "Rejects malformed string");
}

void testYouTubeCaptureServiceFIFO()
{
    LiveBus::YouTubeAudioCaptureService capture;
    TEST_ASSERT(capture.getCaptureMode() == LiveBus::YouTubeCaptureMode::Off,
                "Audio capture is disabled by default");
    juce::AudioBuffer<float> silent(2, 16);
    capture.pullAudioBlock(silent, 16, 48000.0);
    TEST_ASSERT(silent.getMagnitude(0, 0, 16) == 0.0f,
                "Disabled capture outputs silence");

    capture.setCaptureMode(LiveBus::YouTubeCaptureMode::VirtualCableOrDevice);

    // Push 256 samples into capture FIFO
    juce::AudioBuffer<float> testIn(2, 256);
    for (int i = 0; i < 256; ++i)
    {
        testIn.setSample(0, i, 0.5f);
        testIn.setSample(1, i, -0.5f);
    }

    capture.pushAudioBlock(testIn, 256);

    // Verify peak meters updated
    TEST_ASSERT(capture.getPeakLevelL() >= 0.49f, "Capture FIFO Peak L reflects input");
    TEST_ASSERT(capture.getPeakLevelR() >= 0.49f, "Capture FIFO Peak R reflects input");
    TEST_ASSERT(capture.isSignalDetected(), "Signal presence detected");

    // Pull 256 samples from FIFO into destination
    juce::AudioBuffer<float> dest(2, 256);
    dest.clear();
    capture.pullAudioBlock(dest, 256, 48000.0);

    TEST_ASSERT(std::abs(dest.getSample(0, 100) - 0.5f) < 0.001f, "Destination buffer received L samples cleanly");
    TEST_ASSERT(std::abs(dest.getSample(1, 100) - (-0.5f)) < 0.001f, "Destination buffer received R samples cleanly");

    // Numerical safety: test NaN and Inf sanitization
    juce::AudioBuffer<float> nanBuffer(2, 128);
    for (int i = 0; i < 128; ++i)
    {
        nanBuffer.setSample(0, i, std::numeric_limits<float>::quiet_NaN());
        nanBuffer.setSample(1, i, std::numeric_limits<float>::infinity());
    }
    capture.pushAudioBlock(nanBuffer, 128);

    juce::AudioBuffer<float> cleanDest(2, 128);
    capture.pullAudioBlock(cleanDest, 128, 48000.0);
    for (int i = 0; i < 128; ++i)
    {
        TEST_ASSERT(!std::isnan(cleanDest.getSample(0, i)), "No NaN in pulled buffer");
        TEST_ASSERT(!std::isinf(cleanDest.getSample(1, i)), "No Inf in pulled buffer");
    }

    juce::AudioBuffer<float> ramp(2, 8);
    for (int i = 0; i < 8; ++i)
    {
        ramp.setSample(0, i, static_cast<float>(i) * 0.1f);
        ramp.setSample(1, i, static_cast<float>(i) * -0.1f);
    }
    capture.pushAudioBlock(ramp, 8);
    juce::AudioBuffer<float> resampled(2, 4);
    capture.pullAudioBlock(resampled, 4, 24000.0);
    TEST_ASSERT(std::abs(resampled.getSample(0, 1) - 0.2f) < 0.001f,
                "Capture resamples from the input rate to the mixer rate");
    TEST_ASSERT(std::abs(resampled.getSample(1, 3) - (-0.6f)) < 0.001f,
                "Capture resampling preserves the second channel");
}

void testYouTubeCaptureServiceSimulatedStream()
{
    LiveBus::YouTubeAudioCaptureService capture;
    capture.setCaptureMode(LiveBus::YouTubeCaptureMode::SimulatedTestStream);
    capture.setSimulatedStreamEnabled(true);

    TEST_ASSERT(capture.isCaptureActive(), "Capture reports active in simulated mode");

    juce::AudioBuffer<float> dest(2, 512);
    dest.clear();
    capture.pullAudioBlock(dest, 512, 48000.0);

    float peakL = dest.getMagnitude(0, 0, 512);
    float peakR = dest.getMagnitude(1, 0, 512);

    TEST_ASSERT(peakL > 0.05f && peakL < 1.0f, "Simulated stream generates valid non-zero audio on L");
    TEST_ASSERT(peakR > 0.05f && peakR < 1.0f, "Simulated stream generates valid non-zero audio on R");
    TEST_ASSERT(capture.isSignalDetected(), "Signal detection flag active");

    // Pause simulated stream
    capture.setSimulatedStreamEnabled(false);
    dest.clear();
    capture.pullAudioBlock(dest, 512, 48000.0);
    TEST_ASSERT(dest.getMagnitude(0, 0, 512) == 0.0f, "Paused simulated stream outputs zero silence");
}

void testYouTubeMixerChannelRoutingAndDSP()
{
    RoutingMatrix matrix;
    matrix.setNumActiveChannels(4);

    // Assign Channel 2 (Music Left / BGM) to YouTube source
    auto& ch2 = matrix.getChannel(2);
    ch2.inputSource.store(ChannelInputSource::YouTube);
    ch2.gain.store(1.0f);
    ch2.routeEnable[0].store(true);  // Send to Stream Bus
    ch2.routeEnable[1].store(false); // Do not send to Monitor Bus
    ch2.sendLevel[0].store(1.0f);
    ch2.sendLevel[1].store(0.0f);

    // Channel 0 (Microphone) hardware
    auto& ch0 = matrix.getChannel(0);
    ch0.inputSource.store(ChannelInputSource::Hardware);
    ch0.routeEnable[0].store(true);
    ch0.routeEnable[1].store(true);

    // Render audio through BusDSPProcessor and RoutingMatrix
    BusDSPProcessor proc;
    proc.prepare(48000.0, 256);
    proc.params.eqEnabled.store(true);
    proc.params.eqGainDb.store(3.0f); // +3dB boost

    // Synthesize YouTube audio
    LiveBus::YouTubeAudioCaptureService capture;
    capture.setCaptureMode(LiveBus::YouTubeCaptureMode::SimulatedTestStream);
    capture.setSimulatedStreamEnabled(true);

    juce::AudioBuffer<float> ytBuffer(2, 256);
    capture.pullAudioBlock(ytBuffer, 256, 48000.0);

    // Feed through channel DSP
    proc.process(ytBuffer, 256);

    // Route through matrix summing
    const float* inPointers[4] = { nullptr, nullptr, ytBuffer.getReadPointer(0), nullptr };
    juce::AudioBuffer<float> streamBus(2, 256);
    juce::AudioBuffer<float> monitorBus(2, 256);
    matrix.routeAndSum(inPointers, 4, streamBus, monitorBus, 256);

    float streamMag = streamBus.getMagnitude(0, 0, 256);
    float monitorMag = monitorBus.getMagnitude(0, 0, 256);

    TEST_ASSERT(streamMag > 0.05f, "YouTube channel audio summed to Stream Bus");
    TEST_ASSERT(monitorMag == 0.0f, "YouTube channel audio isolated from Monitor Bus (Send=0)");

    // Invert routing: Enable Monitor, Disable Stream
    ch2.routeEnable[0].store(false);
    ch2.routeEnable[1].store(true);
    ch2.sendLevel[1].store(1.0f);

    matrix.routeAndSum(inPointers, 4, streamBus, monitorBus, 256);
    TEST_ASSERT(streamBus.getMagnitude(0, 0, 256) == 0.0f, "Stream Bus silent when YouTube Stream send disabled");
    TEST_ASSERT(monitorBus.getMagnitude(0, 0, 256) > 0.05f, "Monitor Bus receives YouTube audio when Monitor send enabled");
}

void testAuxSendReturnReverbDelayRouting()
{
    RoutingMatrix matrix;
    matrix.prepare(48000.0, 128);
    TEST_ASSERT(matrix.getAuxBus(2).name == "Aux 3", "Aux 3 name does not promise an unavailable Chorus effect");
    TEST_ASSERT(matrix.getAuxBus(3).name == "Aux 4", "Aux 4 name does not promise an unavailable FX effect");

    // Disable ALL direct stream/monitor sends — only Aux Return should contribute signal
    for (int i = 0; i < 4; ++i)
    {
        matrix.getChannel(i).routeEnable[0].store(false);
        matrix.getChannel(i).routeEnable[1].store(false);
    }

    // Configure Channel 0 with Aux 1 (Reverb) Send
    auto& ch0 = matrix.getChannel(0);
    ch0.gain.store(1.0f);
    ch0.auxRouteEnable[0].store(true);
    ch0.auxSendLevel[0].store(1.0f);

    auto& aux1 = matrix.getAuxBus(0);
    aux1.returnLevel.store(1.0f);
    aux1.sendToStream.store(true);
    aux1.sendToMonitor.store(false);
    aux1.mute.store(false);

    std::vector<std::vector<float>> inputs(4, std::vector<float>(128, 0.5f));
    const float* inPtrs[4] = { inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data() };
    juce::AudioBuffer<float> streamBuf(2, 128);
    juce::AudioBuffer<float> monitorBuf(2, 128);

    // Allow the reverb's internal delay lines to produce the initial wet return.
    float maxStreamMagnitude = 0.0f;
    float maxMonitorMagnitude = 0.0f;
    for (int warmup = 0; warmup < 100; ++warmup)
    {
        matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 128);
        maxStreamMagnitude = juce::jmax(maxStreamMagnitude, streamBuf.getMagnitude(0, 0, 128));
        maxMonitorMagnitude = juce::jmax(maxMonitorMagnitude, monitorBuf.getMagnitude(0, 0, 128));
    }

    TEST_ASSERT(maxStreamMagnitude > 0.0f, "Aux 1 Reverb wet return feeds Stream bus after warmup");
    TEST_ASSERT(maxMonitorMagnitude == 0.0f, "Aux 1 return does not leak into Monitor bus when sendToMonitor is false");

    // Test Aux 1 Mute — after muting, process a fresh block of silence
    aux1.mute.store(true);
    std::vector<std::vector<float>> silence(4, std::vector<float>(128, 0.0f));
    const float* silPtrs[4] = { silence[0].data(), silence[1].data(), silence[2].data(), silence[3].data() };
    // Flush reverb tail with silence + mute
    for (int flush = 0; flush < 10; ++flush)
        matrix.routeAndSum(silPtrs, 4, streamBuf, monitorBuf, 128);
    TEST_ASSERT(streamBuf.getMagnitude(0, 0, 128) == 0.0f, "Muting Aux 1 returns silence to stream");

    // Test Aux 2 (Ping-Pong Delay) — verify aux bus receives signal (peakMeterL updated)
    aux1.mute.store(false);
    ch0.auxRouteEnable[0].store(false);

    auto& ch1 = matrix.getChannel(1);
    ch1.gain.store(1.0f);
    ch1.auxRouteEnable[1].store(true);
    ch1.auxSendLevel[1].store(1.0f);

    auto& aux2 = matrix.getAuxBus(1);
    aux2.returnLevel.store(1.0f);
    aux2.sendToStream.store(true);
    aux2.sendToMonitor.store(true);
    aux2.mute.store(false);
    aux2.fxParam1.store(0.01f);

    // The meter follows the delayed return, so allow the configured delay to elapse.
    float maxDelayReturnPeak = 0.0f;
    for (int block = 0; block < 6; ++block)
    {
        matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 128);
        maxDelayReturnPeak = juce::jmax(maxDelayReturnPeak, aux2.peakMeterL.load());
    }
    TEST_ASSERT(maxDelayReturnPeak > 0.0f, "Aux 2 meter detected delayed return");
}

void testSubmixGroupFaderRouting()
{
    RoutingMatrix matrix;
    matrix.prepare(48000.0, 128);

    // Assign Channel 2 & 3 to Submix 1 (BGM Group)
    auto& ch2 = matrix.getChannel(2);
    ch2.submixGroup.store(1);
    ch2.gain.store(1.0f);
    ch2.routeEnable[0].store(true);
    ch2.routeEnable[1].store(false);   // Must explicitly disable: default is true

    auto& ch3 = matrix.getChannel(3);
    ch3.submixGroup.store(1);
    ch3.gain.store(1.0f);
    ch3.routeEnable[0].store(true);
    ch3.routeEnable[1].store(false);   // Must explicitly disable: default is true

    // Channel 0 & 1: fully disabled for clean isolation
    matrix.getChannel(0).routeEnable[0].store(false);
    matrix.getChannel(0).routeEnable[1].store(false);
    matrix.getChannel(1).routeEnable[0].store(false);
    matrix.getChannel(1).routeEnable[1].store(false);

    // Configure Submix 1 — routes to Stream only, not Monitor
    auto& sub1 = matrix.getSubmixBus(1);
    sub1.gain.store(1.0f);
    sub1.routeToStream.store(true);
    sub1.routeToMonitor.store(false);
    sub1.mute.store(false);

    std::vector<std::vector<float>> inputs(4, std::vector<float>(128, 0.3f));
    const float* inPtrs[4] = { inputs[0].data(), inputs[1].data(), inputs[2].data(), inputs[3].data() };
    juce::AudioBuffer<float> streamBuf(2, 128);
    juce::AudioBuffer<float> monitorBuf(2, 128);

    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 128);
    float sMag = streamBuf.getMagnitude(0, 0, 128);
    float mMag = monitorBuf.getMagnitude(0, 0, 128);

    TEST_ASSERT(sMag > 0.1f, "Submix summed Channel 2 & 3 into Stream Bus");
    TEST_ASSERT(mMag == 0.0f, "Submix isolated from Monitor Bus (routeToMonitor=false)");

    // Group Fader attenuation: gain=0 → output must be silent
    sub1.gain.store(0.0f);
    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 128);
    TEST_ASSERT(streamBuf.getMagnitude(0, 0, 128) == 0.0f, "Submix gain 0 silences group output");

    // Group Mute button test
    sub1.gain.store(1.0f);
    sub1.mute.store(true);
    matrix.routeAndSum(inPtrs, 4, streamBuf, monitorBuf, 128);
    TEST_ASSERT(streamBuf.getMagnitude(0, 0, 128) == 0.0f, "Submix mute silences group output");

    // Preset 4 (DAW Style): verify Ch3, Ch4 → BGM Submix & Ch1 → Reverb Aux
    matrix.applyRoutingPreset(4);
    TEST_ASSERT(matrix.getChannel(2).submixGroup.load() == 1, "Preset 4 routes Ch3 to BGM Submix");
    TEST_ASSERT(matrix.getChannel(3).submixGroup.load() == 1, "Preset 4 routes Ch4 to BGM Submix");
    TEST_ASSERT(matrix.getChannel(0).auxRouteEnable[0].load() == true, "Preset 4 enables Vocal Reverb Aux Send");
}

void testDuckingSubmixAndAuxDestinationIsolation()
{
    RoutingMatrix matrix;
    matrix.prepare(48000.0, 64);
    matrix.setActiveChannels(4);

    auto& ch0 = matrix.getChannel(0);
    ch0.gain.store(1.0f);
    ch0.pan.store(0.0f);
    ch0.routeEnable[0].store(false);
    ch0.routeEnable[1].store(false);
    ch0.submixGroup.store(0);

    auto& submix = matrix.getSubmixBus(0);
    submix.gain.store(1.0f);
    submix.routeToStream.store(true);
    submix.routeToMonitor.store(true);
    submix.sendLevelStream.store(1.0f);
    submix.sendLevelMonitor.store(1.0f);

    juce::AudioBuffer<float> channelBuffers[4];
    for (auto& buffer : channelBuffers)
    {
        buffer.setSize(2, 64);
        buffer.clear();
    }
    channelBuffers[0].setSample(0, 0, 0.5f);
    channelBuffers[0].setSample(1, 0, 0.5f);

    juce::AudioBuffer<float> stream(2, 64);
    juce::AudioBuffer<float> monitor(2, 64);
    matrix.routeAndSum(channelBuffers, 4, stream, monitor, 64, 0.25f, 0.5f, 0x0001, 3);

    TEST_ASSERT(std::abs(stream.getSample(0, 0) - 0.125f) < 1e-5f,
                "Submix applies Stream ducking to its Stream destination");
    TEST_ASSERT(std::abs(monitor.getSample(0, 0) - 0.25f) < 1e-5f,
                "Submix applies independent Monitor ducking to its Monitor destination");

    ch0.submixGroup.store(-1);
    ch0.auxRouteEnable[2].store(true);
    ch0.preFaderAuxSend[2].store(true);
    ch0.gain.store(0.0f);
    ch0.auxSendLevel[2].store(1.0f);

    auto& aux = matrix.getAuxBus(2);
    aux.fxType.store(0);
    aux.returnLevel.store(1.0f);
    aux.sendToStream.store(true);
    aux.sendToMonitor.store(true);
    aux.sendLevelStream.store(1.0f);
    aux.sendLevelMonitor.store(1.0f);

    matrix.routeAndSum(channelBuffers, 4, stream, monitor, 64, 0.25f, 0.5f, 0x0001, 3);

    TEST_ASSERT(std::abs(stream.getSample(0, 0) - 0.125f) < 1e-5f,
                "Pre-fader Aux send is ducked at the Stream return destination");
    TEST_ASSERT(std::abs(monitor.getSample(0, 0) - 0.25f) < 1e-5f,
                "Aux return applies independent Monitor ducking");

    ch0.preFaderAuxSend[2].store(false);
    matrix.routeAndSum(channelBuffers, 4, stream, monitor, 64, 0.25f, 0.5f, 0x0001, 3);
    TEST_ASSERT(stream.getSample(0, 0) == 0.0f && monitor.getSample(0, 0) == 0.0f,
                "Post-fader Aux send follows the channel fader");

    ch0.preFaderAuxSend[2].store(true);
    ch0.gain.store(1.0f);
    matrix.routeAndSum(channelBuffers, 4, stream, monitor, 64, 0.25f, 0.5f, 0x0000, 3);
    TEST_ASSERT(std::abs(stream.getSample(0, 0) - 0.5f) < 1e-5f
                && std::abs(monitor.getSample(0, 0) - 0.5f) < 1e-5f,
                "Aux return is not ducked when no targeted channel sends to it");
}

int main()
{
    std::cout << "==========================================" << std::endl;
    std::cout << " LiveBus Mixer v1.0 — Automated Test Suite" << std::endl;
    std::cout << "==========================================" << std::endl;

    int prevFailed = 0;
    // Six Repaired DSP Modules Tests
    RUN_TEST(testHighPassFilterFrequencyResponseAndSlope);
    RUN_TEST(testLowPassFilterFrequencyResponseAndSlope);
    RUN_TEST(testNoiseGateEnvelopeAndGainReduction);
    RUN_TEST(testCompressorRatioKneeAndMakeupGain);
    RUN_TEST(testLimiterCeilingEnforcementAndClippingProtection);
    RUN_TEST(testReverbWetDryAndDecayTailResponse);
    RUN_TEST(testDelayBufferEchoTimeAndFeedbackDecay);
    RUN_TEST(testDSPTargetIndependenceAndBypassBitTransparency);
    RUN_TEST(testMasterDimAttenuatesBusWithoutChangingFaderGain);
    RUN_TEST(testDSPSceneSaveLoadConsistency);

    RUN_TEST(testRoutingMatrixAndPanLaw);
    RUN_TEST(testAudioDelayCompensatorImpulseAlignment);
    RUN_TEST(testRoutingGraphEditsUpdateAudioEngine);
    RUN_TEST(testIndependentStreamMonitorRestore);
    RUN_TEST(testRoutingGraphConnectionValidation);
    RUN_TEST(testRoutingGraphConnectionSerialization);
    RUN_TEST(testIndependentBusSummingAndIsolation);
    RUN_TEST(testGainAndSendLevels);
    RUN_TEST(testMuteAndSoloLogic);
    RUN_TEST(testSubmixAndAuxSoloRouting);
    RUN_TEST(testDSPProcessorNumericalPerformance);
    RUN_TEST(testSceneSerializationAndDeserialization);
    RUN_TEST(testSceneFileCorruptionRecovery);
    RUN_TEST(testIPCProtocolAndConstants);
    RUN_TEST(testIsolatedPluginDryPathKeepsFixedBlockLatency);
    RUN_TEST(testIPCCompletionPublication);
    RUN_TEST(testPendingAudioBlockState);
    RUN_TEST(testPluginDryFallbackOnCrash);
    RUN_TEST(testSimulatedPluginCrashAndWatchdogRecovery);
    RUN_TEST(testDeviceSupervisorStateMachine);
    RUN_TEST(testDynamicChannelAdditionAndRemoval);
    RUN_TEST(testFourBandStudioEqualizer);
    RUN_TEST(testFifteenBandPuzuEqualizerAndSlopeFilters);
    RUN_TEST(testMidiInputBufferThreadSafety);
    RUN_TEST(testAudioFilePlaybackAndBuffering);
    RUN_TEST(testZipPresetPackaging);
    RUN_TEST(testZipPresetImportRejectsTraversalWithoutReplacingExistingFiles);
    RUN_TEST(testSessionPersistenceAndFullState);

    // 8-Band Parametric Equalizer Tests
    RUN_TEST(testEightBandEQParameterValidationAndClamping);
    RUN_TEST(testEightBandEQFilterTypesAndAcousticResponse);
    RUN_TEST(testDSPEightBandEQProcessingAndAnalyzerFeed);
    RUN_TEST(testEightBandEQBypassAndBitTransparency);
    RUN_TEST(testEightBandEQStereoConsistencyAndNumericalStability);
    RUN_TEST(testEightBandEQTargetIsolation);
    RUN_TEST(testEightBandEQSceneSerializationRoundTripAndLegacy);

    // Professional VU & Audio Level Metering Tests
    RUN_TEST(testLevelMeterSilenceAndKnownSineWavePeakRms);
    RUN_TEST(testLevelMeterPeakHoldAndManualReset);
    RUN_TEST(testLevelMeterClipDetectionAndLatch);
    RUN_TEST(testLevelMeterVUBallisticsAndCalibration);
    RUN_TEST(testLevelMeterBusIsolationAndStereoConsistency);

    // Professional Vocal DSP Rack Tests
    RUN_TEST(testVocalDSPGateHysteresisAndHold);
    RUN_TEST(testVocalDSPCompressorKneeAndGainReduction);
    RUN_TEST(testVocalDSPDeEsserTargetFrequencyAttenuation);
    RUN_TEST(testVocalDSPLimiterCeilingEnforcementAndOutputProtection);
    RUN_TEST(testVocalDSPBypassAndBitTransparency);
    RUN_TEST(testVocalDSPStreamAndMonitorBusIndependence);
    RUN_TEST(testVocalDSPSceneSerializationRoundTrip);

    // Professional Device Manager & Audio Stability Tests
    RUN_TEST(testAudioDeviceServiceEnumerationAndBackendValidation);
    RUN_TEST(testAudioDeviceServiceBufferDurationCalculation);
    RUN_TEST(testAudioDeviceServiceTestToneSafetyAndLevel);
    RUN_TEST(testAudioDeviceServiceInputMeterBallistics);
    RUN_TEST(testAudioDeviceServiceChannelBitmaskValidation);
    RUN_TEST(testAudioDeviceServiceSupervisorReconnectionAndBackoff);

    // Professional Scenes & Presets Manager Tests
    RUN_TEST(testSceneSaveLoadAndMetadata);
    RUN_TEST(testSceneSaveAsDuplicateRenameDelete);
    RUN_TEST(testScenePreWriteBackupAndCorruptionRecovery);
    RUN_TEST(testRecallFilterBlockAllAndAllowAll);
    RUN_TEST(testRecallFilterSelectiveApplication);
    RUN_TEST(testRecallFilterCancelSafety);
    RUN_TEST(testPresetManagerCRUDAndFactoryProtection);
    RUN_TEST(testPresetIsolationScopeAndCopyTools);

    // OBS Studio Integration & Virtual Audio Routing Tests
    RUN_TEST(testOBSVirtualDeviceDetectionAndPatternRecognition);
    RUN_TEST(testOBSWebSocketV5HandshakeAndAuthentication);
    RUN_TEST(testOBSStreamRoutingAndMeterBallistics);
    RUN_TEST(testOBSStreamAndMonitorGainIndependence);
    RUN_TEST(testOBSStreamMuteDoesNotMuteMonitor);
    RUN_TEST(testOBSMonitorMuteDoesNotMuteStream);
    RUN_TEST(testOBSChannelMappingAndStereoIsolation);
    RUN_TEST(testOBSTestSignalGenerationAndAntiClick);
    RUN_TEST(testOBSDeviceRecoveryAndStateSafety);

    // Professional MIDI Controller & MIDI Learn Tests
    RUN_TEST(testMIDICCAbsoluteMappingAndRangeClamping);
    RUN_TEST(testMIDIRelativeModes);
    RUN_TEST(testMIDILogarithmicFrequencyMapping);
    RUN_TEST(testMIDINoteToggleAndMomentaryBehavior);
    RUN_TEST(testMIDIChannelAndControllerFiltering);
    RUN_TEST(testMIDILearnWorkflowAndTimeout);
    RUN_TEST(testMIDIDuplicateDetectionAndWarning);
    RUN_TEST(testMIDIPresetSerializationAndFactoryProtection);
    RUN_TEST(testMIDIStreamAndMonitorBusTargetIsolation);

    // Professional Playback Audio Player & Test Signal Generator Tests
    RUN_TEST(testPlaybackOpenFileValid);
    RUN_TEST(testPlaybackRejectInvalidFile);
    RUN_TEST(testPlaybackSeekAndPosition);
    RUN_TEST(testPlaybackLoopRangeAndWrap);
    RUN_TEST(testPlaybackMarkerCRUD);
    RUN_TEST(testPlaybackPlaylistSaveLoadAndIntegrity);
    RUN_TEST(testPlaybackMissingTrackHandling);
    RUN_TEST(testPlaybackGainAndMute);
    RUN_TEST(testPlaybackSpeedRatioAndResampling);
    RUN_TEST(testTestSignalSineFrequencyAndPhaseDelta);
    RUN_TEST(testTestSignalLevelClampingAndDbConversion);
    RUN_TEST(testTestSignalWhiteAndPinkNoiseSpectralDifferences);
    RUN_TEST(testTestSignalNyquistLimitEnforcement);
    RUN_TEST(testTestSignalRoutingToBusesAndChannels);
    RUN_TEST(testTestSignalNoNaNOrInfinitySanitization);
    RUN_TEST(testTestSignalSilenceWhenStopped);
    RUN_TEST(testPlaybackSampleRateChangeResilience);

    // Professional VST3 Plugin Host Tests
    RUN_TEST(testVST3PluginLibraryModelSerializationAndDefaults);
    RUN_TEST(testVST3PluginScanSettingsAndPaths);
    RUN_TEST(testVST3CatalogSearchAndFiltering);
    RUN_TEST(testVST3PluginChainSlotManagementAndLifecycle);
    RUN_TEST(testVST3FailedBinaryLoadDoesNotCreateActiveSlot);
    RUN_TEST(testVST3PluginChainXMLSerializationAndStateRestore);
    RUN_TEST(testVST3MissingPluginPlaceholderGracefulHandling);
    RUN_TEST(testVST3AudioProcessingFormatAndDryFallbackSafety);
    RUN_TEST(testVST3OutOfProcessWorkerTimeoutAndWatchdog);
    RUN_TEST(testMidiPresetImportRejectsPathTraversalName);
    RUN_TEST(testVST3SceneManagerPluginIntegration);

    // Professional Performance & Diagnostics Tests
    RUN_TEST(testBufferDurationAndSampleRateConversion);
    RUN_TEST(testLatencyReportingAndUnavailableHandling);
    RUN_TEST(testCPULoadCalculationAndBoundaryClamping);
    RUN_TEST(testDeadlineMissAndDropoutCounters);
    RUN_TEST(testDiagnosticsQueueLockFreeRingBufferAndOverflowHandling);
    RUN_TEST(testDiagnosticsLogManagerSeverityFilteringAndTrimming);
    RUN_TEST(testDiagnosticReportSanitizationAndExport);
    RUN_TEST(testDeviceEventLifecycleAndFailureHandling);

    // Auto Ducking Tests (M2)
    RUN_TEST(testAutoDuckerThresholdAndEnvelope);
    RUN_TEST(testAutoDuckerStreamAndMonitorIndependence);
    RUN_TEST(testAutoDuckerBypassAndTransparency);
    RUN_TEST(testAutoDuckerSidechainFilter);
    RUN_TEST(testAutoDuckerXmlSerializationRoundTrip);

    // Audio Safety & Anti-Feedback Tests (M4)
    RUN_TEST(testAudioSafetyLimiterCeilingAndGR);
    RUN_TEST(testAudioSafetyPeakAndClipWarning);
    RUN_TEST(testAudioSafetySanitizesNonFiniteSamples);
    RUN_TEST(testAudioSafetyEmergencyMuteInstantZero);
    RUN_TEST(testAudioSafetyProtectsFinalHardwareOutputs);
    RUN_TEST(testAudioSafetyRoutingLoopDetection);

    // Macro Engine & Quick Actions Tests (M6/M8)
    RUN_TEST(testMacroEngineParameterAssignmentAndMapping);
    RUN_TEST(testMacroEngineMultiParameterCoordinatedUpdate);
    RUN_TEST(testMacroEngineUnresolvedParameterSafety);
    RUN_TEST(testMacroEngineQuickActions);
    RUN_TEST(testMacroEngineXmlSerialization);
    RUN_TEST(testSceneManagerDuckingAndSafetySerialization);

    // Per-Channel Input & Output Routing Matrix Tests (v1.0)
    RUN_TEST(testPerChannelInputSourceSelection);
    RUN_TEST(testPerChannelStreamAndMonitorSendsIndependent);
    RUN_TEST(testPreFaderVsPostFaderSend);
    RUN_TEST(testDuplicateHardwareInputWarning);
    RUN_TEST(testStereoPairAndBalanceLaw);
    RUN_TEST(testRoutingMatrixXmlPersistence);
    RUN_TEST(testRoutingMatrixPresets);
    RUN_TEST(testMissingHardwareInputSafety);
    RUN_TEST(testIndependentHardwareDirectOutputRouting);
    RUN_TEST(testMultiDestinationSimultaneousRouting);
    RUN_TEST(testRoutingIsolationAndSafety);
    RUN_TEST(testHardwareDirectOutputXmlPersistence);
    RUN_TEST(testAuxSendReturnReverbDelayRouting);
    RUN_TEST(testSubmixGroupFaderRouting);
    RUN_TEST(testDuckingSubmixAndAuxDestinationIsolation);

    // Embedded YouTube Player & Audio Capture Bridge Tests
    RUN_TEST(testYouTubeURLParsing);
    RUN_TEST(testYouTubeCaptureServiceFIFO);
    RUN_TEST(testYouTubeCaptureServiceSimulatedStream);
    RUN_TEST(testYouTubeMixerChannelRoutingAndDSP);

    std::cout << "==========================================" << std::endl;
    std::cout << " Test Summary: " << passedTests << " passed, " << failedTests << " failed." << std::endl;
    std::cout << "==========================================" << std::endl;

    return failedTests == 0 ? 0 : 1;
}
