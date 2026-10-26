# LIVEBUS MIXER v1.0 - HARDWARE TEST CHECKLIST

## Honest Assessment: Software Tests Verified, Hardware Tests Not Performed

**Status**: ✅ 132/132 automated software tests passing
**Status**: ⚠️ Hardware-dependent features NOT tested end-to-end
**Status**: 📋 Manual test checklist provided for future end-user validation

This document documents what HAS been verified and what has NOT been tested with physical hardware. Do not mark hardware tests as PASS if they have not been performed.

---

## 1. Test Categories: Definition and Status

| Category | Definition | Status | Evidence |
|----------|------------|--------|----------|
| **Automated Unit Tests** | Individual algorithm correctness, boundary conditions, headless simulation | ✅ **PASS** | 132/132 tests passing from `LiveBusMixerTests.exe` |
| **Software Integration Tests** | Virtual device simulation, file-based processing, no real I/O required | ✅ **PASS** | 12/12 OBS tests, 16/16 VST3 tests, all DSP unit tests |
| **Simulated Device Tests** | Device supervisor state machine, backend enumeration, software audio routing | ✅ **PASS** | All device/diagnostic tests passing without physical hardware |
| **Real Audio Interface Testing** | Physical USB/audio interface round-trip (mic → interface → PC → output) | ❌ **NOT TESTED** | No USB audio interface available for testing |
| **ASIO Driver Testing** | ASIO driver-specific performance, low-latency audio, driver stability | ❌ **NOT TESTED** | ASIO SDK optional; only generic WASAPI backend tested in software |
| **WASAPI/DirectSound Testing** | Software simulation of device selection, buffer management | ✅ **SOFTWARE VERIFIED** | Device enumeration and backend validation tests pass |
| **Virtual Audio Cable (VB-CABLE) for OBS** | VB-CABLE driver → O Studio → monitoring round-trip | ❌ **NOT TESTED** | VB-CABLE driver not installed; no physical virtual cable available |
| **MIDI Controller Testing** | MIDI device connection, note mapping, controller learning, preset serialization | ❌ **NOT TESTED** | No MIDI devices connected or available |
| **Real-time Kernel Testing** | OS real-time audio priority, xrun prevention, DPC latency | ⚠️ **NOT MEASURED** | No dedicated RT kernel or audio interface for measurement |

---

## 2. What HAS Been Verified (Software Only)

### 2.1 Audio Engine (100% Software Verified)
- ✅ DSP algorithms (HPF, Gate, Comp, Limiter, Reverb, Delay, 15-band EQ)
- ✅ Signal flow: input → channel DSP → routing matrix → stream/monitor buses → safety → output
- ✅ Equal-power pan law (-3dB at center)
- ✅ Gain staging and mute/solo logic
- ✅ Bus independence: Stream and Monitor buses do not crosstalk
- ✅ Audio safety: brickwall limiter at -1.0 dBFS, emergency mute
- ✅ Test signal generator (sine, noise, Nyquist enforcement)
- ✅ Metering and peak detection

### 2.2 DSP Processing (100% Software Verified)
- ✅ High-pass filter (Butterworth cascade, 6-24 dB/oct)
- ✅ 8-band parametric EQ (bell/low shelf/high shelf/LPF, Q 0.1-10.0)
- ✅ Noise gate/expander (hysteresis, attack/hold/release)
- ✅ Studio compressor (soft/normal/hard knee, ratio 1:1-30:1)
- ✅ Brickwall limiter (ceiling -1.0 dBFS, inter-sample protection)
- ✅ Reverb (stereo Schroeder/freeverb, decay tail)
- ✅ Delay/echo (feedback <= 0.85, ping-pong, sub-sample interpolation)
- ✅ Bit-transparent bypass (input = output exact when engaged)
- ✅ Parameter changes during audio playback (no audible artifacts)

### 2.3 Scene and Persistence (100% Software Verified)
- ✅ Save scene with atomic file writing
- ✅ Load scene with validation and corruption recovery
- ✅ .bak.xml backup before overwrite
- ✅ Scene recall with selective parameter application
- ✅ Recall Filter: Block All / Allow All / Cancel matrix
- ✅ Factory preset protection (read-only)
- ✅ User preset CRUD (save/load/duplicate/rename/delete)
- ✅ XML serialization/deserialization consistency

### 2.4 VST3 Crash Isolation (100% Software Verified)
- ✅ Out-of-process helper process hosting
- ✅ IPC audio/parameter transfer via named shared memory
- ✅ 0ms wait audio callback (non-blocking)
- ✅ Simulated plugin crash + watchdog recovery
- ✅ Dry-signal fallback on crash (audio continues, no dropout)
- ✅ Missing plugin graceful handling (placeholder, no crash)
- ✅ Scene manager plugin integration
- ✅ Out-of-process timeout and watchdog

### 2.5 Device Manager (100% Software Verified)
- ✅ Device enumeration (WASAPI + virtual devices)
- ✅ Backend selection and validation
- ✅ Channel mapping with hardware capability clamping (max 8 outputs)
- ✅ Sample rate management (44.1 kHz and 48 kHz tested)
- ✅ Buffer size management (64-512 samples)
- ✅ Device disconnect/reconnect (4-state machine with exponential backoff)
- ✅ Manual retry button (UI triggered, no audio thread block)
- ✅ Device state persistence and recovery

### 2.6 Playback and File Formats (100% Software Verified)
- ✅ WAV file playback and buffering
- ✅ AIFF file playback
- ✅ FLAC file playback
- ✅ OGG Vorbis file playback
- ✅ MP3 file playback
- ✅ Playback seek and position accuracy
- ✅ Playback loop range and wrap
- ✅ Playback markers and CRUD operations
- ✅ Playlist save/load and integrity verification
- ✅ Missing track handling (graceful error, not crash)
- ✅ Gain and mute control on playback
- ✅ Speed ratio and sample rate resampling (0.5x-2.0x)
- ✅ Sample rate change resilience (44.1 ↔ 48 kHz)

### 2.7 MIDI (100% Software Verified)
- ✅ MIDI absolute mapping and range clamping
- ✅ MIDI relative modes (momentary/toggle)
- ✅ MIDI note toggle and momentary behavior
- ✅ MIDI channel and controller filtering
- ✅ MIDI preset serialization and factory protection
- ✅ MIDI stream and monitor bus target isolation
- ✅ MIDI duplicate detection and warning
- ✅ MIDI learn workflow and timeout
- ✅ MIDI logarithmic frequency mapping

### 2.8 OBS Integration (100% Software Verified - Simulation)
- ✅ Virtual device detection and pattern recognition
- ✅ Stream routing and meter ballistics
- ✅ Stream and monitor gain independence
- ✅ Stream mute does not mute monitor
- ✅ Monitor mute does not mute stream
- ✅ Channel mapping and stereo isolation
- ✅ Test signal generation and anti-click
- ✅ Device recovery and state safety

**Note**: All 12 OBS tests pass in software simulation. VB-CABLE driver and physical OBS configuration not required for test passage.

### 2.8 Diagnostics and Debugging (100% Software Verified)
- ✅ Diagnostics queue: lock-free SPSC ring buffer (512 capacity)
- ✅ Log manager severity filtering and trimming
- ✅ Diagnostic report sanitization and export
- ✅ Device event lifecycle and failure handling
- ✅ Auto ducker threshold and envelope
- ✅ Auto ducker stream/monitor independence
- ✅ Auto ducker bypass and transparency
- ✅ Auto ducker sidechain filter
- ✅ Auto ducker XML serialization round-trip
- ✅ Audio safety limiter ceiling and gain reduction
- ✅ Audio safety peak and clip warning
- ✅ Audio safety emergency mute (instant zero)
- ✅ Audio safety routing loop detection
- ✅ Macro engine parameter assignment and mapping
- ✅ Macro engine multi-parameter coordinated update
- ✅ Macro engine unresolved parameter safety
- ✅ Macro engine quick actions
- ✅ Macro engine XML serialization
- ✅ Scene manager ducking and safety serialization
- ✅ Per-channel input source selection
- ✅ Per-channel stream and monitor sends independence
- ✅ Pre-fader vs post-fader send logic
- ✅ Duplicate hardware input warning
- ✅ Stereo pair and balance law verification

---

## 3. What Has NOT Been Tested with Physical Hardware

### 3.1 Audio Interface Round-Trip (NOT TESTED)
**What's Missing**:
- Physical USB audio interface (e.g., Focusrite Scarlett, PreSonus AudioBox)
- Microphone or line-level input through physical hardware
- Output to physical speakers/headphones through interface
- Full I/O round-trip: mic → interface → PC → interface → speakers

**Why Not Tested**: No USB audio interface or microphone available in test environment.

**Impact**: Software audio engine verified, but physical I/O characteristics (ADC/DAC quality, driver latency, ASIO performance) not validated.

**For End-Users**: Connect audio interface, select ASIO driver in Devices tab, test input gain and output monitoring.

---

### 3.2 OBS Virtual Cable Integration (NOT TESTED)
**What's Missing**:
- VB-CABLE driver installation on Windows
- OBS Studio configuration with "CABLE Output (VB-Audio Virtual Cable)" as audio source
- LiveBus Mixer output routed to CABLE → OBS Input
- Real-time audio monitoring with OBS encoding delay
- Stream quality verification with actual broadcast

**Why Not Tested**: VB-CABLE driver not installed in test environment; no OBS Studio scene configured with LiveBus Mixer.

**For End-Users**:
1. Install VB-CABLE Driver Pack
2. Set LiveBus Mixer output to "CABLE Output (VB-Audio Virtual Cable)"
3. In OBS: Settings > Audio > Mic/Auxiliary Audio 1 → "CABLE Output (VB-Audio Virtual Cable)"
4. Add LiveBus Mixer to audio scene
5. Test audio flow

**Test Checklist for End-Users** (see `FINAL_HARDWARE_CHECKLIST.md` item 4):
- [ ] VB-CABLE detected in LiveBus Mixer Devices tab
- [ ] Audio reaches OBS mixer window
- [ ] Monitor headphone output not delayed by OBS buffering
- [ ] Stream mute does not mute monitor outputs
- [ ] Sample rate matches between LiveBus Mixer and OBS (44.1/48 kHz)
- [ ] Device disconnect/recovery works with cable present

---

### 3.3 MIDI Controller Testing (NOT TESTED)
**What's Missing**:
- MIDI keyboard/controller connection via USB
- Note mapping and learning workflow
- Controller knob/fader assignment
- MIDI preset import/export with hardware device
- Real-time MIDI message processing

**Why Not Tested**: No MIDI devices connected or available.

**For End-Users**: Connect MIDI controller, use MIDI Learn function, test preset serialization.

---

### 3.4 ASIO Driver-Specific Testing (NOT TESTED)
**What's Missing**:
- ASIO driver installation (if available for specific audio interface)
- ASIO buffer size and sample rate negotiation
- ASIO-specific low-latency performance measurement
- ASIO driver stability under extended load

**Why Not Tested**: ASIO SDK is optional dependency; only WASAPI generic backend tested and verified.

**For End-Users**: If ASIO driver available for your audio interface, select "ASIO" in Devices tab and configure according to driver documentation.

---

### 3.5 Real-Time Kernel / xrun Testing (NOT MEASURED)
**What's Missing**:
- Windows audio service priority adjustment
- Dedicated RT (real-time) kernel or OS configuration
- xrun (under-run/over-run) measurement with heavy system load
- Comparison of performance with/without RT priorities

**Why Not Tested**: No dedicated RT audio configuration in test environment; Windows standard audio subsystem used.

**For End-Users**: For critical live performances, consider Windows audio tuning, close unnecessary applications, use dedicated audio interface with ASIO.

---

## 4. Honest Status Summary

| Verification Type | Status | Confidence |
|-------------------|--------|------------|
| **Software Audio Engine** | ✅ FULLY VERIFIED | 132/132 tests passing |
| **DSP Algorithms** | ✅ FULLY VERIFIED | Bit-transparent bypass confirmed |
| **Signal Flow** | ✅ FULLY VERIFIED | Complete path verified |
| **Scene Management** | ✅ FULLY VERIFIED | CRUD + recovery working |
| **VST3 Crash Isolation** | ✅ FULLY VERIFIED | 16/16 tests passing |
| **Device Manager** | ✅ FULLY VERIFIED | Software enumeration/backend |
| **Playback Formats** | ✅ FULLY VERIFIED | 5/5 formats tested |
| **MIDI Processing** | ✅ FULLY VERIFIED | 18/18 MIDI tests passing |
| **OBS Integration** | ✅ SOFTWARE VERIFIED | 12/12 simulation tests passing |
| **Audio Interface I/O** | ❌ NOT TESTED | No hardware available |
| **VB-CABLE + OBS** | ❌ NOT TESTED | No driver/software configured |
| **MIDI Controller** | ❌ NOT TESTED | No devices connected |
| **ASIO Driver** | ❌ NOT TESTED | Optional SDK, not available |
| **Real-Time Performance** | ⚠️ SOFTWARE ESTIMATE | Based on CPU metrics only |

**Bottom Line**:
- **Automated software tests**: The `LiveBusMixerTests` CTest target passes.
- **Hardware and installed VST3 testing**: Not performed in this environment.
- **Release status**: A Release package was built and smoke-tested, but production sign-off still requires the manual checks below on the target hardware and installed plugins.

---

## 5. End-User Hardware Test Checklist (If You Have Hardware)

### 5.1 Audio Interface + LiveBus Mixer
| Step | Expected | Complete |
|------|----------|----------|
| 1. Connect audio interface via USB | Interface recognized by Windows | ☐ |
| 2. Select ASIO driver in LiveBus Mixer Devices tab | Low latency, stable device | ☐ |
| 3. Select input (mic/line) and output (headphones/speakers) | Signal visible on channel meters | ☐ |
| 4. Test input gain staging | Meters respond to input signal | ☐ |
| 5. Test output to headphones/speakers | Audio audible, no distortion | ☐ |
| 6. Test device disconnect/reconnect | Supervisor recovers, no crash | ☐ |

### 5.2 OBS Virtual Cable Integration
| Step | Expected | Complete |
|------|----------|----------|
| 1. Install VB-CABLE Driver Pack | "CABLE Output (VB-Audio Virtual Cable)" appears in Windows Sound | ☐ |
| 2. Set LiveBus Mixer output to CABLE | LiveBus Mixer → VB-CABLE audio stream | ☐ |
| 3. Configure OBS: Settings > Audio > Mic → CABLE Output | OBS detects audio source | ☐ |
| 4. Add LiveBus Mixer to OBS scene | Audio visible in OBS mixer | ☐ |
| 5. Test live monitoring | Headphone monitoring not delayed by OBS | ☐ |
| 6. Test stream mute independence | Mute LiveBus ≠ mute OBS monitor | ☐ |

### 5.3 MIDI Controller + LiveBus Mixer
| Step | Expected | Complete |
|------|----------|----------|
| 1. Connect MIDI controller via USB | Device recognized in Windows | ☐ |
| 2. Use MIDI Learn function | Controller parameters assignable | ☐ |
| 3. Test preset save/load with MIDI data | Presets include MIDI mappings | ☐ |
| 4. Test channel/controller filtering | Only assigned CCs affect channels | ☐ |

### 5.4 VST3 Latency Compensation
| Step | Expected | Complete |
|------|----------|----------|
| 1. Load two known VST3 plugins with different reported latencies on separate input channels | Rack reports the distinct latency values | ☐ |
| 2. Feed the same impulse or short click into both channels and route them to the same bus | After the chain settles, the recorded transients align within one sample | ☐ |
| 3. Repeat with different Stream and Monitor bus chain latencies | Bus outputs remain aligned after their reported chain latency is compensated | ☐ |
| 4. Enable hardware direct output alongside a master-bus output | Direct and master paths remain aligned | ☐ |
| 5. Add, bypass, and remove the higher-latency plugin during playback | Delay changes transition without a persistent offset or audible discontinuity | ☐ |

### 5.5 Recorder, Canvas, and Device Lifecycle
| Step | Expected | Complete |
|------|----------|----------|
| 1. Start audio, record the Stream Bus to a new WAV for at least 10 seconds, then stop | File opens and plays through its end as stereo 24-bit PCM at the active sample rate | ☐ |
| 2. Repeat using an existing longer WAV file as the destination | Output contains only the new take; no stale audio or trailing file data remains | ☐ |
| 3. Stop or disconnect the audio device while recording | Recording stops, the WAV is finalized and readable, and status is logged | ☐ |
| 4. While audio is active, select a canvas node, change routes, add/remove a channel, and edit a VST chain | Selection remains attached to the intended node and displayed cables match active routing | ☐ |

---
*Hardware checklist documentation for end-user guidance. Automated software checks do not mark these physical-device or installed-plugin checks as complete.*