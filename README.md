# LiveBus Mixer v1.0 — Professional Broadcast & Monitor Console for Windows

LiveBus Mixer is a high-performance, real-time desktop audio mixer, DSP routing matrix, and VST3 host engineered with JUCE (C++17) and CMake for Windows 10/11 x64. It is designed specifically for live streaming, live music broadcasting, and vocal processing with OBS Studio integration and a professional DAW broadcast console interface.

The Windows application embeds its branded icon from `Resources/LiveBusMixerIcon.png`.

---

## 1. Arsitektur dan Fitur Utama

### A. Professional Broadcast Console & 8-Tab Navigation
- **Header Bar**: Menampilkan `LiveBus Mixer v1.0`, status pill audio running/stopped, nama audio device aktif, sample rate, buffer size, dan CPU meter real-time (`%`).
- **9-Tab Navigation Bar**:
  1. `🎛 Mixer`: Dashboard utama 6-area Broadcast Console.
  2. `〰 DSP`: Studio DSP berpresisi tinggi dengan kurva EQ parametrik dan opsi Puzu 15-Band EQ.
  3. `🔌 Effects`: VST3 sandbox host dengan out-of-process isolation.
  4. `📁 Scenes`: Manajemen preset & XML serialization.
  5. `🎚 Devices`: Konfigurasi driver ASIO/WASAPI, channel mapping, dan test tone generator.
  6. `📹 OBS`: Panel kontrol integrasi OBS Studio dan virtual audio cables.
  7. `🎹 MIDI`: Professional MIDI Controller & MIDI Learn System (konsol gelap, preset mapping, CC absolute/relative, note toggle/momentary, soft takeover pickup).
  8. `▶ Playback`: Professional Streaming Audio File Player & Audio Test Signal Generator (Stereo waveform overview & detail, loop region handles, cue markers, playlist manager, speed resampling, honest pitch lock, Sine/Noise/Impulse generator).
  9. `⚙ Settings`: Tema visual dan preferensi sistem.

### B. 16-Channel Routing Matrix & Channel Strips
- **Expandable Live Inputs**: Hingga 16 input aktif dengan nama, sumber, meter, dan kontrol strip yang independen.
- **Input 1 (Microphone)**: Panning independen dengan equal-power pan law (-3 dB), gain fader (-60 dB s.d. +6 dB), mute, solo, peak meter.
- **Input 2 (Guitar / Inst)**: Panning independen, gain fader, mute, solo, peak meter.
- **Input 3 (Music Left)**: Channel stereo kiri musik/BGM dengan routing independen.
- **Input 4 (Music Right)**: Channel stereo kanan musik/BGM dengan routing independen.
- **Editable Channel Names**: Nama setiap channel dapat diubah langsung oleh pengguna di antarmuka.
- **Per-Channel Output Routing**: Halaman `Routing` dimulai dengan tampilan ringkas per channel: sumber, output (`Stream + Monitor`, `Stream only`, `Monitor only`, `Off`, atau salah satu dari empat Submix), level, pan, mute, dan solo. Tombol `Advanced Sends` menampilkan kembali kontrol send Stream/Monitor, AUX, dan direct hardware output; tabel dapat digeser horizontal untuk mencapai seluruh kolom hardware. Inspector mempertahankan pemetaan input, pre/post-fader, gain send, dan kontrol master bus. Pilihan output memakai state routing yang sama dengan strip mixer dan patchbay.
- **Interactive Routing Graph**: Tombol `Node Graph` pada halaman `Routing` menampilkan input aktif, Submix, AUX, Stream, dan Monitor berdasarkan state `RoutingMatrix` yang sama dengan audio engine. Tombol `+ Input` membuka pilihan sumber untuk menambah channel mixer aktif, yang kemudian muncul sebagai node; node DSP/plugin bebas dan bus di luar kapasitas engine tidak dapat ditambahkan. Sambungan dimodelkan dengan ID node dan port audio yang stabil serta pemetaan channel `EngineNative`; validator menolak node/port yang tidak dikenal, arah atau layout yang tidak didukung, dan jalur yang berpotensi feedback. Jika kabel yang sama dibuat lagi, engine melaporkannya sebagai sudah terhubung tanpa menggandakan state. Seret kabel dari output node ke input yang didukung untuk mengubah routing nyata; klik kanan kabel untuk memutusnya, seret node untuk mengatur tampilan, gunakan seret tengah untuk pan, dan `Ctrl`+scroll untuk zoom. Meter node Input, Submix, AUX, Stream, dan Monitor menampilkan level sinyal yang dibaca dari state engine atomik. Scene XML menyimpan routing engine sebagai sumber kebenaran, serta layout graph schema v2 dengan node, koneksi port/mapping, zoom, dan viewport; koneksi yang tersimpan divalidasi terhadap routing yang dipulihkan sebelum layout diterima. Ukuran node tetap karena node belum dapat di-resize. Layout schema v1 tetap didukung. Tabel routing tetap tersedia, sedangkan tombol `Signal Flow` di toolbar graph membuka diagram status dan meter yang lebih rinci. Graph tidak mengklaim mendukung penyisipan DSP/plugin bebas atau routing endpoint perangkat keras.
- **Real-Time Safe Summing**: Operasi summing dan routing menggunakan atomic state dan buffer pre-allocated tanpa alokasi memori pada audio thread.
- **Monitor ke output perangkat utama**: Pada mode `Dedicated Endpoint`, bus Monitor selalu dipetakan ke output stereo pertama perangkat utama (Output 1-2), termasuk jika antarmuka memiliki empat output atau lebih. Mode `Primary Multi-Channel` tetap memakai Output 1-2 untuk Stream dan Output 3-4 untuk Monitor.
- **Ducking pada Submix dan AUX**: Submix mempertahankan ducking Stream dan Monitor secara independen sampai tujuan masing-masing. Jika channel target ducking aktif mengirim sinyal ke suatu AUX, seluruh return AUX tersebut mengikuti multiplier ducking masing-masing tujuan; aturan ini berlaku juga untuk send AUX pre-fader (pre/post hanya mengatur pengaruh fader channel).

### Adaptasi workflow mixer Mixcraft untuk live mixing

Matriks ini membandingkan konsep mixer/effects yang relevan dari panduan Mixcraft dengan implementasi LiveBus. Ini adalah adaptasi workflow, bukan klaim kompatibilitas atau parity penuh dengan DAW Mixcraft.

| Area | Adaptasi LiveBus Mixer | Status dan batasan |
|---|---|---|
| Channel strip | Strip input menyediakan nama/sumber, meter, fader, gain, pan, mute/solo, routing Stream/Monitor, akses FX, dan warna aksen yang dapat dipilih dari badge nomor channel. | Warna membantu membedakan input live dan ikut tersimpan dalam scene; bukan track timeline DAW. |
| Effects list dan manajemen plugin | Tab `Effects` dan floating FX Rack menyediakan pengelolaan chain/plugin; tombol `FX` pada strip membuka rack untuk target yang dipilih. | Chain input dan bus independen; pengelolaan tetap berorientasi pada target mixer. |
| Chain dan urutan efek | Insert dapat dikelola dan diurutkan secara independen per input atau bus. | Tidak mengubah atau menggabungkan chain antar-target. |
| Output bus, submix, dan signal flow | Tampilan ringkas Routing menyediakan pemilih output, level, pan, mute, dan solo per channel; kontrol send AUX/hardware dibuka melalui `Advanced Sends`. `Node Graph` dapat menghubungkan routing input/Submix/AUX ke Stream atau Monitor secara langsung, dan `Signal Flow` menampilkan status serta meter rinci. | Kabel hanya tersedia untuk koneksi yang didukung routing matrix; DSP/plugin bebas dan endpoint hardware bukan node graph. Visualisasi memakai state routing yang sama dengan audio engine, bukan jalur pemrosesan terpisah. |
| Parameter dan bypass plugin | Plugin editor tetap digunakan untuk parameter; quick menu strip menyediakan bypass/enable semua insert atau toggle per insert. | Menu quick-bypass telah diuji langsung pada aplikasi: toggle individual dan bypass/enable seluruh chain mengubah status yang tampil; plugin Stream dikembalikan ke status aktif setelah pengujian. |
| Kustomisasi layout mixer | Input dapat digeser per channel; panel mixer atas/bawah dapat diubah ukurannya. Posisi channel pertama dan proporsi panel disimpan saat scene ditangkap/dipulihkan. | Menyesuaikan keterbacaan konsol live tanpa memperkenalkan track/timeline atau mode arranger. |

### B. Master Output Buses
- **Stream Bus (Outputs 1 & 2)**: Bus utama yang ditujukan ke virtual audio cable untuk dihubungkan ke OBS Studio. Memiliki master fader, pan, mute, dual stereo VU meters, dan indikator clipping LED.
- **Monitor Bus (Outputs 3 & 4)**: Bus monitoring independen untuk headphone atau monitor speaker musisi/broadcaster. Memiliki fader, pan, mute, dan metering independen.
- **Endpoint Output Master**: Halaman `OBS` menyediakan satu strip master endpoint tambahan. Pilih output device dari backend audio aktif (termasuk headphone Bluetooth atau output jack yang terdeteksi Windows), pilih `Stream Bus`, `Monitor Bus`, atau gabungan, lalu atur mute dan level endpoint secara independen. Pada perangkat utama stereo, Monitor Bus tetap keluar ke perangkat utama meski endpoint sekunder belum dipilih; Stream memerlukan endpoint sekunder, atau pengguna dapat memilih mode audition gabungan. Pada perangkat utama dengan empat output atau lebih, Stream memakai output 1-2 dan Monitor output 3-4. Hanya satu endpoint sekunder yang dapat dibuka pada satu waktu.

### C. Professional Vocal DSP Rack (Waves eMo D5 Hardware Style)
Tab `DSP` menyediakan konsol hardware dinamika vokal studio lengkap dengan arsitektur 7-stage chain:
1. **Input Trim & High-Pass Filter (HPF)**:
   - Input Gain (-18 dB s.d. +18 dB)
   - HPF Cutoff (20 Hz s.d. 500 Hz) dengan seleksi kemiringan slope: 12 dB/Oct, 24 dB/Oct, 36 dB/Oct, 48 dB/Oct (Butterworth cascade).
2. **Noise Gate / Expander**:
   - Open Threshold (-80 dB s.d. 0 dB)
   - Close Threshold Hysteresis (-80 dB s.d. 0 dB) untuk mencegah chatter di sekitar threshold.
   - Attack (0.1 ms s.d. 100 ms), Hold (0 ms s.d. 500 ms), Release (10 ms s.d. 1000 ms).
   - Floor attenuation (-80 dB s.d. 0 dB).
   - Mode Gate vs Expander (1:2 downward ratio).
   - Key Source (Internal vs External Sidechain) dengan Sidechain Key Filters (HPF 20..2000 Hz, LPF 200..20000 Hz).
3. **8-Band Parametric EQ**:
   - Terintegrasi langsung dengan 8-band EQ engine (Bell, Low Shelf, High Shelf, HPF, LPF) dengan visualisasi kurva aktual dan live FFT spectrum analyzer.
4. **De-Esser**:
   - Threshold (-40 dB s.d. 0 dB)
   - Frequency (2000 Hz s.d. 12000 Hz)
   - Range / Maximum Attenuation (-24 dB s.d. 0 dB)
   - Filter Type (Bell / Peak band-pass vs High Shelf detector)
   - Gain reduction metering presisi tinggi dan envelope smoothing untuk eliminasi sibilance vokal natural tanpa lisping.
5. **Studio Compressor**:
   - Threshold (-60 dB s.d. 0 dB)
   - Ratio (1.0:1 s.d. 20.0:1)
   - Knee options: **SOFT** (quadratic transition ~12 dB), **NORMAL** (~6 dB), **HARD** (sharp transition 0 dB).
   - Attack (0.1 ms s.d. 100 ms), Release (10 ms s.d. 1000 ms).
   - Release Mode: **MANUAL** vs **AUTO** (program-adaptive release timing berdasarkan transient/sustain).
   - Sidechain Key Source (INT, DeS band-pass, EXT) dengan Sidechain Key Filters (HPF 20..2000 Hz, LPF 200..20000 Hz).
   - Parallel Compression Mix (0% s.d. 100% wet).
6. **Brickwall Limiter**:
   - Ceiling / Threshold (-20 dBFS s.d. 0 dBFS).
   - Release (5 ms s.d. 500 ms).
   - Fast attack ceiling enforcement untuk perlindungan inter-sample clipping streaming.
7. **Level & Output Section**:
   - Makeup Gain (-18 dB s.d. +18 dB).
   - Output Gain (-18 dB s.d. +18 dB).
   - Output Level Metering stereo aktual dan Clip LED indicator.
   - 4 Vertical Segmented LED Ladders: Gate GR (dB), Gate In (dBFS), Comp In (dBFS), Comp GR (dB).
- **Target Independence**: 6 target independen (Input 1-4, Stream Bus Master, Monitor Bus Master) dengan DSP state terisolasi penuh.
- **Effects Chain Overview**: Header rack menampilkan tombol master `DSP ON/OFF` dan urutan insert DSP HPF → Gate → EQ → De-Esser → Compressor → Limiter dengan status ON/BYP dan bypass cepat per modul. Urutan mencerminkan jalur pemrosesan tetap di engine; pengaturan detail tetap berada pada kontrol masing-masing modul.
- **7 Studio Presets**: *Vocal Clean*, *Vocal Speech*, *Vocal Streaming*, *Guitar Clean*, *Music Bus*, *Broadcast Limiter*, dan *Reset Default*.
- **Real-Time Safety**: Parameter atomics, smoothed gain curves, zero allocations, no locks/blocking di audio callback.

### D. Independent Built-in DSP Strips (6 Functional Modules)
Setiap channel dan bus (Stream Bus, Monitor Bus, Input 1-4) memiliki modul DSP terpisah yang dapat dikonfigurasi secara independen pada Tab `DSP`:
1. **High-Pass Filter (HPF)**:
   - Cutoff: 20 Hz – 1000 Hz.
   - Filter Slope: 6 dB/oct, 12 dB/oct, 18 dB/oct, 24 dB/oct (Cascaded Butterworth IIR).
   - Filter Type: Butterworth, Linkwitz-Riley, Bessel.
   - Kurva visualisasi logaritmik real-time dengan status cutoff frequency live.
2. **Noise Gate / Expander**:
   - Threshold: -80 dB s.d. 0 dB (dengan hysteresis 3 dB pencegah chatter).
   - Attack: 0.1 ms s.d. 100 ms, Hold: 0 ms s.d. 500 ms, Release: 10 ms s.d. 1000 ms.
   - Floor / Range: -80 dB s.d. 0 dB.
   - Real-time Gain Reduction Metering dan Status Pill aktif (Closed / Hold / Open).
3. **Studio Dynamics Compressor**:
   - Threshold: -60 dB s.d. 0 dB, Ratio: 1.0:1 s.d. 30.0:1.
   - Attack: 0.1 ms s.d. 100 ms, Release: 10 ms s.d. 1000 ms.
   - Knee options: Soft (quadratic ~12 dB), Normal (~6 dB), Hard (sharp 0 dB).
   - Independent Makeup Gain: -18 dB s.d. +18 dB.
   - Parallel Mix: 0% s.d. 100% (dry/wet).
   - Dynamic transfer curve visualizer dengan moving operating point dan real-time Gain Reduction meter.
4. **Broadcast Peak Limiter**:
   - Ceiling / Threshold: -24 dBFS s.d. 0 dBFS.
   - Release: 1 ms s.d. 500 ms.
   - Lookahead-free zero-latency exponential envelope follower untuk perlindungan clipping seketika.
   - Output Peak Metering dan Gain Reduction Metering aktual.
   - Isolasi penuh antara Stream Bus (OBS) dan Monitor Bus (Headphones).
5. **Studio Acoustic Reverb**:
   - Algoritma stereo Schroeder/Freeverb berbasis `juce::dsp::Reverb`.
   - Room Size: 0% s.d. 100%, Damping: 0% s.d. 100%.
   - Wet Level: 0% s.d. 100%, Dry Level: 0% s.d. 100%, Stereo Width: 0% s.d. 100%.
   - Natural decay tail persistensi (reverb tail tetap meluruh alami setelah audio input berhenti).
   - Zero-pop bit-exact bypass.
6. **Stereo Echo & Delay**:
   - Delay Buffer sirkular aman untuk audio thread dengan sub-sample linear interpolation.
   - Delay Time: 1 ms s.d. 2000 ms.
   - Feedback: 0% s.d. 90% (dibatasi <= 0.90 untuk mencegah signal runaway atau feedback explosion).
   - Mode Stereo Ping-Pong (L/R crossfeed bounce) & Normal stereo.
   - Wet Level (0% s.d. 100%) dan Dry Level (0% s.d. 100%).
   - Anti-click parameter smoothing saat delay time digeser.
- **Real-Time Safety**: Parameter atomics (`std::atomic<float>`, `std::atomic<bool>`), smoothed gain curves, zero-allocation di audio callback, dan bit-exact bypass transparency.

### D. Professional VST3 Plugin Host & Library System
Tab `Effects` menyediakan konsol hosting VST3 insert rack dan modul **Plug-in Library** lengkap:
1. **Plug-in Library Interface (Reference Accurate)**:
   - Header konsol gelap profesional dengan icon menu glyph, judul `PLUG-IN LIBRARY`, tombol close `[X]`, dan sub-instruksi *"Use the Def Column to set your default plug-in. Click the Star icon to add a plug-in to your favourites."*.
   - Filter bar terintegrasi: Search query box interaktif, Category filter dropdown, Manufacturer filter dropdown, checkbox `Favorites Only`, dan sorting berdasarkan Nama atau Kategori.
   - Tabel plugin lengkap: Kolom `Def` (radio bullet cyan untuk memilih default plugin), `★` (gold filled star untuk favorite, grey outline untuk non-favorite), `Name`, `Format` (bright cyan badge `VST3`), `Category` (`Fx|Dynamics|EQ`, `Fx|Modulation`, dll.), `Manufacturer` (`Harrison Audio`, `Slate Digital`, `Waves`, dll.), `Profile` (`User` vs `Factory`), dan row highlight bar gelap solid (`#374151`).
   - Bottom Toolbar: `LOAD PLUG-IN`, `SCAN PLUG-INS` (cyan accent button), `MANAGE`, dan tombol `BACK`.
2. **VST3 Scanning & Path Management (`PluginManageComponent` & `VST3HostEngine`)**:
   - Lokasi scanning standar Windows (`%CommonProgramFiles%\VST3` dan `C:\Program Files\Vstplugins`).
   - Tambah/hapus custom search path yang dipilih pengguna melalui dialog modal `MANAGE`.
   - Pemindaian background thread non-blocking (`ScannerThread`) tanpa membebani audio callback atau antarmuka pengguna.
   - Live progress indicator dengan persentase dan nama file plugin aktif yang sedang diverifikasi, serta tombol `Cancel`.
   - Cache XML persistence (`plugin_library.xml`, `plugins_cache.xml`) di folder `%APPDATA%/LiveBusMixer/` untuk waktu startup instan.
3. **Multi-Target Plugin Processing Chain (`EffectsConsoleComponent`)**:
   - 18 target chain independen: 16 input, Stream Bus, dan Monitor Bus.
   - Rack Slot Controls: Bypass toggle, tombol `Open GUI`, tombol reorder `[^]` (move up) dan `[v]` (move down), serta tombol remove `[X]`.
   - Tombol `Bypass Target Chain` untuk bypass instan seluruh plugin dalam chain dan tombol `Clear Chain`.
   - Latency Reporting: Menampilkan latency samples per plugin slot dan total latency akumulatif per target bus.
   - Plugin Delay Compensation (PDC): menyelaraskan channel dengan kompensasi terhadap chain channel terlatensi tertinggi, menyelaraskan Stream/Monitor, dan menunda hardware direct send agar tetap sejajar dengan bus master. Delay line dialokasikan sebelum audio berjalan dan perubahan delay memakai transisi 128 sampel; batas maksimum kompensasi 65.536 sampel.
   - Tombol `FX` pada setiap input dan bus membuka floating rack yang langsung memilih target strip tersebut; klik kanan membuka menu bypass cepat per plugin atau untuk seluruh chain. Label, warna, dan tooltip tombol menunjukkan jumlah insert, status bypass, dan nama plugin di chain; tab `Effects` tetap tersedia sebagai halaman penuh.
4. **Plugin GUI Editor Hosting (`PluginEditorWindow`)**:
   - Floating window `juce::DocumentWindow` aman pada message thread untuk menampilkan editor GUI native plugin.
   - Fallback otomatis ke `juce::GenericAudioProcessorEditor` jika plugin tidak memiliki custom GUI.
   - Penutupan jendela editor secara aman saat instance plugin dihapus dari rack slot guna mencegah orphan windows.
5. **Real-Time Safety & Zero Allocation**:
   - Audio callback menggunakan `chainMutex.try_lock()` dengan instant dry fallback jika chain sedang dimodifikasi oleh UI thread.
   - Buffer temporary pre-allocated tanpa alokasi memori heap (`malloc`/`new`) pada audio thread.
6. **Plugin State Persistence & Scene Restore**:
   - Integrasi penuh dengan Scene Manager (`SceneData.plugins`).
   - Serialisasi parameter dan binary state blob plugin via base64 encoding.
   - Restorasi otomatis plugin chains saat Scene dimuat, dengan penanganan graceful placeholder jika plugin tidak terpasang di sistem tanpa menyebabkan crash atau merusak data scene.
7. **Dual Hosting Modes**:
   - **Mode A — In-Process**: Hosting langsung di dalam proses mixer utama untuk performa maksimal dan 0 IPC overhead.
   - **Mode B — Out-of-Process Isolation**: Hosting terisolasi menggunakan helper process `LiveBusPluginHost.exe` via Windows Named Shared Memory dan Named Events IPC dengan sequence number, zero-blocking audio callback (0ms wait), dan watchdog crash recovery otomatis.

### E. Professional Scenes & Presets Manager (`ScenesConsoleComponent`)
Tab **Scenes** menyediakan antarmuka konsol siaran gelap profesional dengan 4 sub-navigasi (`Scene Manager`, `Recall Filter`, `Preset Manager`, `Copy & Tools`):

1. **Pemisahan Konsep Scenes vs Presets**:
   - **Scenes**: Snapshot menyeluruh dari seluruh mixer (Input Channels 1-4, Stream Bus, Monitor Bus, 8-Band EQ semua channel, Vocal DSP Rack semua channel, Routing Matrix, Master VU Ballistics, Preferensi Audio Device, MIDI mapping, dan Schema Version 3.0 metadata).
   - **Presets**: Snapshot terisolasi khusus komponen tertentu (Vocal DSP Rack, 8-Band Parametric EQ, Channel Strip, Master Bus Processing). Preset memiliki proteksi cakupan (scope isolation): memuat preset vokal tidak akan pernah mengubah routing, device settings, bus routing, atau channel lain.
2. **Schema Version 3.0 & Safe Recovery Engine**:
   - Serialization XML tervalidasi dengan timestamp presisi tinggi dan field deskripsi scene opsional.
   - **Atomic File Writing**: Ditulis ke file `.tmp` terlebih dahulu, lalu di-rename secara atomik.
   - **Pre-Write Backup Mechanism**: Otomatis membuat salinan cadangan `.bak.xml` sebelum menimpa file scene yang ada.
   - **Auto-Recovery on Corruption**: Jika file XML korup atau terpotong, loader otomatis beralih ke `.bak.xml` yang valid atau safe defaults tanpa crashing.
3. **Broadcast Console Recall Filter Matrix** (Allen & Heath dLive / DiGiCo Style):
   - **Matrix Selektif Parameter**: Memilih secara presisi bagian state yang akan diterapkan saat recall scene.
   - **Indikator Visual 3D LED**: Diska LED radial bercahaya (glowing crimson untuk diizinkan / aktif, slate charcoal untuk diblokir / tidak didukung).
   - **Master ALL Toggle**: Diska toggle diagonal untuk mengizinkan/memblokir seluruh matriks dalam satu klik.
   - **Column & Row Header Toggles**: Tombol `+` pada setiap kolom parameter (*Processing*, *Mute*, *Level*, *Main On*, *Pan*, *Routing*, *Direct Out*, *IEM/Wedge*, *DCA & Grp*, *Name & Colour*) dan baris (*Inputs 1-4*, *Stream Bus*, *Monitor Bus*, *FX Units*, *MIDI Mapping*, *Device Preferences*).
   - **Left Category Selector**: Menampilkan status live LED pada kategori (*Inputs*, *Mixes*, *FX Units*, *FX Sends*, *FX Returns*, *DCAs*, *Mute Groups*, *SoftKeys*, *MIDI*, *Controllers*, *Dyn8 / Vocal*, *Other*).
   - **Safe Workflow & Rollback**: Tombol *Block All*, *Allow All*, *Apply* (emerald green), dan *Cancel* (burgundy red). Tombol Cancel menjamin tidak ada perubahan state aktif yang diterapkan.
4. **Preset Manager Terisolasi**:
   - Browser preset multi-kategori (Vocal DSP Rack, 8-Band Parametric EQ, Channel Strip, Master Bus Processing).
   - **Factory Presets Protection**: Preset bawaan studio (*Broadcast Lead Vocal*, *Voiceover Warm & Intimate*, *Aggressive Rock Vocal*, *Acoustic Guitar Dynamics*, *Podcast Dual Host*, *Stream Bus Master Leveler*) berstatus read-only dan dilindungi dari penimpaan/penghapusan tidak sengaja.
   - **User Presets CRUD**: Simpan, muat, Save As, Duplicate, Rename, Delete, dan Import/Export file preset (`.xml`) pada direktori standar pengguna Windows (`%USERPROFILE%/Documents/LiveBusMixer/Presets/`).
5. **Copy & Update Tools**:
   - **Copy 8-Band Parametric EQ To**: Salin konfigurasi EQ dari channel/bus sumber ke tujuan tertentu saja.
   - **Copy Vocal DSP Rack To**: Salin konfigurasi Vocal Rack lengkap (Gate, EQ, De-Esser, Comp, Limiter) tanpa mengubah routing.
   - **Copy Channel Strip Settings**: Salin level gain, pan, mute, solo (dengan opsi checklist *Include Send Levels*).
   - **Reset Target Processor Defaults**: Mengembalikan parameter prosesor target ke kondisi flat/bawaan dengan konfirmasi.
   - **Update Active Scene Now**: Memperbarui scene aktif langsung dengan konfigurasi konsol saat ini.

### F. Professional Audio Device Manager & Stability Engine (`AudioDeviceService`)
Pengelolaan perangkat audio perangkat keras terpusat dengan antarmuka studio gelap profesional (`AudioSettingsComponent`) pada Tab **Devices** dan **Settings**:
### G. Professional Performance & Diagnostics Console (`PerformanceDiagnosticsComponent`)
Tab **Settings** menyediakan konsol terintegrasi **Performance & Diagnostics** dengan 3-card layout konsol audio gelap profesional yang terkalibrasi presisi dengan referensi desain perangkat keras:
1. **Audio Settings Panel (Reference Accurate)**:
   - Panel gelap bergradasi (`#181a20`) dengan aksen border halus (`#2b303c`).
   - Dropdown `Interface:` (Perangkat audio aktif).
   - Dropdown `Sample Rate:` (44.1 kHz, 48 kHz, 88.2 kHz, 96 kHz, 192 kHz).
   - **Interactive Notched Buffer Size Slider (`NotchedBufferSizeSlider`)**:
     - Notched ticks akurat untuk ukuran buffer standar (32, 64, 128, 256, 512, 1024, 2048 samples).
     - Label tick terkalibrasi, track aktif teal cyan (`#00f5d4`), dan thumb slider kapsul putih (`#ffffff`).
     - Tampilan ganda: label kanan atas (`128 Samples`) dan label dinamis kanan bawah dengan durasi buffer aktual:
       $$\text{Buffer Duration (ms)} = \frac{\text{Buffer Samples}}{\text{Sample Rate}} \times 1000$$
   - **Honest Latency Readouts**:
     - `Input Latency:` dan `Output Latency:` dalam milidetik (cyan `#00f5d4`).
     - Pelabelan transparan: membedakan latensi yang dilaporkan driver (*Driver reported values*) dengan durasi 1 buffer. Menampilkan `N/A` jika driver backend tidak melaporkan latensi.
   - Dropdown `Interface Driver:` (ASIO, Windows Audio, DirectSound).
   - **Audio CPU Load Bar**:
     - Recessed dark channel dengan pengukur visual bergradasi cyan (`#00f5d4` $\to$ `#4cc9f0`).
     - Persentase real-time akurat diukur langsung dari audio processing callback.
2. **Real-Time Telemetry & Audio Stability**:
   - Status badge mesin audio: `ENGINE RUNNING` (hijau neon `#00f5d4`), `STOPPED`, atau `ERROR`.
   - 6 Metric Tiles:
     - *Buffer Processing Time*: Rata-rata dan maksimum waktu pemrosesan blok audio (`Avg: 0.12 ms | Max: 0.45 ms / Budget: 10.00 ms`).
     - *Peak CPU Load*: Beban puncak audio dengan mekanisme peak hold dan decay perlahan.
     - *Deadline Misses (Overruns)*: Counter kejadian di mana processing callback melebihi jatah waktu blok audio (`elapsedMs > budgetMs`).
     - *Driver Dropouts*: Counter underrun/dropout yang dilaporkan oleh backend atau driver hardware.
     - *Device Errors*: Counter kegagalan atau diskoneksi perangkat keras.
     - *Active Sample Rate & Ch*: Sample rate aktif dan konfigurasi kanal I/O fisik.
   - Tombol `Clear Statistics`, `Test Tone (440Hz)`, dan `Open ASIO Panel`.
   - Dynamic Warning Banner: Peringatan otomatis jika beban CPU melebihi 85%, terdeteksi deadline miss, atau perangkat terputus.
3. **Diagnostics Event Log & Reporting System (`DiagnosticsLogManager`)**:
   - **Zero-Allocation Audio Thread Logging**: Menggunakan lock-free bounded SPSC circular ring buffer (`RealtimeDiagnosticQueue`, kapasitas 512) tanpa heap allocation (`malloc`/`new`) atau locking pada audio callback.
   - Event categorization: `Device`, `AudioEngine`, `Buffer`, `DSP`, `VST3`, `Scene`, `Routing`.
   - Severity badges: `INFO` (cyan/teal), `WARNING` (amber yellow), `ERROR` (bright red).
   - Filter bar: `All`, `Info Only`, `Warnings & Errors`, `Errors Only`.
   - Tombol `Clear Log`, `Export Log` (.txt), `Preview Report`, dan `Export Report` (.txt).
4. **Sanitized Diagnostic Report Generator (`DiagnosticReportPreviewDialog`)**:
   - Jendela modal gelap untuk meninjau laporan sistem lengkap sebelum diekspor.
   - Informasi terstruktur: App version, JUCE version, OS info, audio backend, device names, sample rates, buffer sizes, latencies, CPU metrics, counter status, VST3 host state, routing summary, dan event history terbaru.
   - **Kerahasiaan & Keamanan**: 100% tersanitasi — tidak menyertakan kredensial, token, path sensitif, maupun binary data.
   - Tombol `Copy to Clipboard` dan `Save to File...`.

---

## 2. Panduan Setup OBS Studio

JUCE Audio Device Manager pada Windows mengirim audio ke perangkat output fisik atau virtual. Windows tidak mengizinkan aplikasi non-driver membuat audio input virtual tanpa driver WDM/KS. Untuk menghubungkan LiveBus Mixer ke OBS Studio:

### Langkah Integrasi Virtual Audio Cable:
1. **Instal Virtual Audio Device**:
   - Pasang driver virtual audio seperti **VB-Audio Cable** (VB-CABLE Driver Pack) atau **VAC (Virtual Audio Cable)**.
2. **Konfigurasi LiveBus Mixer**:
   - Buka LiveBus Mixer > Tab **Settings** (atau klik ikon gear di header).
   - Pada **Interface**, pilih perangkat audio interface Anda (jika mendukung multi-channel 4-out: Ch 1-2 Stream, Ch 3-4 Monitor) ATAU pilih virtual cable (misal: *CABLE Input (VB-Audio Virtual Cable)*).
3. **Konfigurasi OBS Studio**:
   - Buka OBS Studio > **Settings** > **Audio**.
   - Pada **Mic/Auxiliary Audio 1**, pilih **CABLE Output (VB-Audio Virtual Cable)**.
   - Di audio mixer OBS, tambahkan filter limiter/monitoring jika diperlukan.
4. **Alur Audio Live**:
   - Mikrofon & Gitar masuk ke LiveBus Mixer > Diproses melalui HPF, EQ, Gate, Comp, Limiter, Reverb, VST3 > Dikirim ke **Stream Bus** (CABLE Input) > Diterima oleh OBS Studio (CABLE Output).
   - Broadcaster mendengarkan **Monitor Bus** secara langsung melalui headphone tanpa delay OBS.

### Status LiveBus Virtual Audio Device (driver bawaan)

LiveBus saat ini **belum memasang atau menyediakan driver virtual audio sendiri**. Aplikasi hanya mengenumerasi dan menggunakan perangkat virtual pihak ketiga yang memang sudah terpasang. Karena itu tidak ada endpoint Windows bernama LiveBus yang boleh ditampilkan sebagai tersedia.

Rute implementasi yang perlu diteliti dan dibangun terpisah adalah pasangan endpoint render/capture berbasis driver audio Windows (PortCls/WaveRT atau model driver virtual yang masih didukung), dengan format awal yang eksplisit seperti PCM float 32-bit, stereo, 48 kHz. Sampel resmi [Microsoft SysVAD](https://github.com/microsoft/Windows-driver-samples/tree/main/audio/sysvad) adalah contoh WDM audio driver/endpoint dan WaveRT, **bukan driver siap pakai untuk LiveBus**; integrasi tetap memerlukan implementasi topology/pin/format/clock, lifecycle stream/device, power management, INF/package, dan pengujian kompatibilitas Windows. Lihat juga [Port Class Audio Driver Reference](https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/port-class-audio-driver-reference).

Bridge yang disarankan untuk tahap desain: driver dan proses mixer bertukar blok melalui shared-memory ring buffer SPSC dengan header protokol/version/format, sequence counter, ukuran maksimum, timestamp/clock domain, serta counter underrun/overrun. Producer/consumer memakai indeks atomik dan slot preallocated; event/IOCTL hanya untuk control/lifecycle di luar callback—bukan menunggu event atau mengambil mutex di callback. Start/stop/restart harus memvalidasi ulang format dan generation ID; endpoint hilang atau helper mati harus menghasilkan silence/dry fallback yang tercatat. Perbedaan clock/sample rate membutuhkan resampler asinkron dan strategi drift; alur dua arah memerlukan ring buffer terpisah per arah. Shared-memory VST3 yang ada bukan bridge driver yang siap digunakan.

Driver belum dibuat, ditandatangani, dipasang, atau diuji. Sebelum rilis diperlukan driver package dengan penandatanganan yang berlaku untuk distribusi Windows, installer/uninstaller bertingkat privilege yang benar, pengujian restart/sleep/device removal, soak test buffer, serta validasi pada OBS/Discord. Jangan menonaktifkan perlindungan Windows atau menganggap SysVAD langsung memenuhi kebutuhan tersebut.

---

## 3. Menjalankan di PC Lain

Untuk membagikan aplikasi tanpa source code atau Visual Studio:
1. Unduh dan ekstrak seluruh isi `LiveBusMixer-Release-*.zip` ke folder lokal. Jangan pisahkan atau memindahkan file di dalam folder `bin`.
2. Jalankan `Jalankan_LiveBusMixer.bat` dari folder hasil ekstrak. Python tidak diperlukan.
3. Jika Windows melaporkan runtime Visual C++ tidak tersedia, instal **Microsoft Visual C++ Redistributable 2015–2022 x64** dari [situs resmi Microsoft](https://aka.ms/vs/17/release/vc_redist.x64.exe), lalu coba lagi.

Paket ditujukan untuk Windows 10/11 x64 dan berisi aplikasi, helper VST3, serta audio demo. Driver ASIO dan plugin VST3 pihak ketiga tidak disertakan; instal driver/plugin yang diperlukan pada PC tujuan. Peluncur `run_app.py` tersedia sebagai alternatif jika Python sudah terpasang.

## 4. Kompilasi dan Build

### Prasyarat:
- Windows 10/11 x64
- Visual Studio 2022 (atau Visual Studio Build Tools 2026/2022) dengan workload *Desktop development with C++*
- CMake 3.22+
- JUCE 8.0.4 (checkout ke `./JUCE`; versi ini dipin pada workflow CI)

### Perintah Build (PowerShell / Command Prompt):
```powershell
# 1. Konfigurasi CMake
cmake -B build -A x64

# 2. Build semua target (Release)
cmake --build build --config Release
```

### Hasil Build:
Executable yang dihasilkan berada di:
- **Main Mixer**: `build\LiveBusMixer_artefacts\Release\LiveBusMixer.exe` (atau `bin\LiveBusMixer.exe`)
- **Isolated VST3 Host**: `build\LiveBusPluginHost_artefacts\Release\LiveBusPluginHost.exe` (atau `bin\LiveBusPluginHost.exe`)
- **Automated Tests**: `build\LiveBusMixerTests_artefacts\Release\LiveBusMixerTests.exe` (atau `bin\LiveBusMixerTests.exe`)

---

## 5. Pengujian Otomatis (Automated Tests)

Jalankan test suite menggunakan CTest:
```powershell
ctest --test-dir build -C Release --output-on-failure
```

Test runner saat ini menjalankan **150 unit dan integration test**; daftar di bawah merangkum cakupan yang terdokumentasi.
1. `testRoutingMatrixAndPanLaw`: Pengujian equal power pan law (-3dB) dan routing matriks 4-in ke 2-bus.
2. `testIndependentBusSummingAndIsolation`: Pengujian isolasi bus Stream vs Monitor tanpa crosstalk/kebocoran audio.
3. `testGainAndSendLevels`: Pengujian akurasi matematis multiplier gain fader dan send level.
4. `testMuteAndSoloLogic`: Pengujian isolasi solo multi-channel dan pembungkaman mute.
5. `testDSPProcessorNumericalPerformance`: Pengujian numerik DSP limiter ceiling (-1.0 dB) dan deteksi clipping real-time.
6. `testSceneSerializationAndDeserialization`: Pengujian simpan dan pulihkan XML scene beserta state biner.
7. `testSceneFileCorruptionRecovery`: Pengujian ketahanan terhadap file scene yang rusak atau terpotong.
8. `testIPCProtocolAndConstants`: Pengujian validitas struktur shared memory, versi protokol (v3), dan magic number.
9. `testPluginDryFallbackOnCrash`: Pengujian dry signal fallback saat helper crash/tidak responsif (zero audio dropout).
10. `testSimulatedPluginCrashAndWatchdogRecovery`: Pengujian integrasi IPC Command::SimulateCrash, deteksi exit code helper, dan restart otomatis oleh watchdog.
11. `testDeviceSupervisorStateMachine`: Pengujian transisi state supervisor dan exponential backoff.
12. `testDynamicChannelAdditionAndRemoval`: Pengujian penambahan dan penghapusan input channel dinamis.
13. `testFourBandStudioEqualizer`: Pengujian 4-band studio equalizer filter responses.
14. `testFifteenBandPuzuEqualizerAndSlopeFilters`: Pengujian 15-band PUZU tuning console dan high-order Butterworth filters.
15. `testMidiInputBufferThreadSafety`: Pengujian thread-safety ring buffer pesan MIDI.
16. `testAudioFilePlaybackAndBuffering`: Pengujian streaming audio buffer pemutar file musik.
17. `testZipPresetPackaging`: Pengujian ekspor/impor ZIP terkompresi preset manager.
18. `testSessionPersistenceAndFullState`: Pengujian penyimpanan otomatis dan pemulihan sesi `_last_session`.
19. `testEightBandEQParameterValidationAndClamping`: Pengujian validasi & sanitasi parameter 8-band EQ.
20. `testEightBandEQFilterTypesAndAcousticResponse`: Pengujian respon akustik filter Bell, Shelves, HPF, LPF.
21. `testEightBandEQBypassAndBitTransparency`: Pengujian bit-transparency saat EQ di-bypass (0 error).
22. `testEightBandEQStereoConsistencyAndNumericalStability`: Pengujian konsistensi stereo dan stabilitas numerik NaN/Inf.
23. `testEightBandEQTargetIsolation`: Pengujian independensi 6 target EQ tanpa kebocoran state.
24. `testEightBandEQSceneSerializationRoundTripAndLegacy`: Pengujian round-trip XML scene untuk 8-band EQ.
25. `testLevelMeterSilenceAndKnownSineWavePeakRms`: Pengujian kalibrasi meter dBFS dan RMS terhadap input sinus.
26. `testLevelMeterPeakHoldAndManualReset`: Pengujian peak hold dan fungsi reset manual.
27. `testLevelMeterClipDetectionAndLatch`: Pengujian deteksi clipping dan indikator latched clip LED.
28. `testLevelMeterVUBallisticsAndCalibration`: Pengujian balistik fisik jarum analog VU IEC 60268-17 (300ms integration).
29. `testLevelMeterBusIsolationAndStereoConsistency`: Pengujian isolasi multi-titik metering antar channel dan bus.
30. `testVocalDSPGateHysteresisAndHold`: Pengujian threshold hysteresis open vs close, hold time, dan floor attenuation Noise Gate.
31. `testVocalDSPCompressorKneeAndGainReduction`: Pengujian soft/normal/hard knee transitions, ratio compression, dan kalkulasi GR dB.
32. `testVocalDSPDeEsserTargetFrequencyAttenuation`: Pengujian selective band-pass sibilance detection dan dynamic attenuation De-Esser.
33. `testVocalDSPLimiterCeilingEnforcementAndOutputProtection`: Pengujian ceiling threshold limiter (-1.0 dBFS) dan pencegahan inter-sample overshoot.
34. `testVocalDSPBypassAndBitTransparency`: Pengujian bit-transparency Vocal DSP Rack saat di-bypass (exact sample match).
35. `testVocalDSPStreamAndMonitorBusIndependence`: Pengujian independensi mutlak DSP state antara Stream Bus dan Monitor Bus.
36. `testVocalDSPSceneSerializationRoundTrip`: Pengujian serialisasi dan deserialisasi XML Scene untuk seluruh 6 target Vocal DSP.
37. `testAudioDeviceServiceEnumerationAndBackendValidation`: Pengujian enumerasi backend driver audio dan penanganan backend tidak valid.
38. `testAudioDeviceServiceBufferDurationCalculation`: Pengujian kalkulasi durasi buffer ms aktual berdasarkan sample rate.
39. `testAudioDeviceServiceTestToneSafetyAndLevel`: Pengujian nada uji 440 Hz level aman -18 dBFS dan transisi gain anti-click.
40. `testAudioDeviceServiceInputMeterBallistics`: Pengujian balistik meter input real-time terhadap silence dan sinyal sinus referensi.
41. `testAudioDeviceServiceChannelBitmaskValidation`: Pengujian manipulasi bitmask channel input/output hardware.
42. `testAudioDeviceServiceSupervisorReconnectionAndBackoff`: Pengujian transisi state machine supervisor, exponential backoff, dan manual retry.
43. `testSceneSaveLoadAndMetadata`: Pengujian simpan dan muat Scene dengan format Schema Version 3.0, metadata deskripsi, dan timestamp presisi tinggi.
44. `testSceneSaveAsDuplicateRenameDelete`: Pengujian operasi CRUD lengkap Scene (Save As, Duplicate, Rename, Delete) dan integritas list scene.
45. `testScenePreWriteBackupAndCorruptionRecovery`: Pengujian mekanisme pembuatan cadangan atomik `.bak.xml` dan auto-recovery saat XML utama korup.
46. `testRecallFilterBlockAllAndAllowAll`: Pengujian konfigurasi global Recall Filter (Block All, Allow All) dan status per-kategori.
47. `testRecallFilterSelectiveApplication`: Pengujian recall parsial selektif (hanya memulihkan channel yang diizinkan tanpa menyentuh parameter yang diblokir).
48. `testRecallFilterCancelSafety`: Pengujian tombol Cancel Recall Filter yang menjamin integritas state aktif tidak berubah sama sekali (safe rollback).
49. `testPresetManagerCRUDAndFactoryProtection`: Pengujian Preset Manager (Save, Load, Duplicate, Delete) dan proteksi read-only pada Factory Presets.
50. `testPresetIsolationScopeAndCopyTools`: Pengujian isolasi cakupan Preset (tidak pernah merusak routing/devices) serta operasi granular Copy Tools.
51. `testOBSVirtualDeviceDetectionAndPatternRecognition`: Pengujian deteksi dinamis driver virtual audio (VB-CABLE, VAC, VoiceMeeter, Elgato).
52. `testOBSStreamRoutingAndMeterBallistics`: Pengujian perutean audio bus stream ke virtual cable dan balistik meter OBS.
53. `testOBSStreamAndMonitorGainIndependence`: Pengujian independensi master fader stream vs monitor (zero monitor bleed).
54. `testOBSStreamMuteDoesNotMuteMonitor`: Pengujian isolasi mute stream (stream mute tidak mematikan monitor headphone).
55. `testOBSMonitorMuteDoesNotMuteStream`: Pengujian isolasi mute monitor (headphone mute tidak mematikan siaran OBS).
56. `testOBSChannelMappingAndStereoIsolation`: Pengujian pemetaan kanal virtual dan integritas isolasi stereo L/R.
57. `testOBSTestSignalGenerationAndAntiClick`: Pengujian generator sinyal uji aman 440 Hz -18 dBFS dengan transisi anti-click.
58. `testOBSDeviceRecoveryAndStateSafety`: Pengujian resiliensi pemulihan perangkat virtual terhadap disconnect dan sanitasi NaN/Inf.
59. `testMIDICCAbsoluteMappingAndRangeClamping`: Pengujian pemetaan CC absolute, sanitasi range, dan clamping nilai target.
60. `testMIDIRelativeModes`: Pengujian 3 format relative encoder (Relative Signed, Two's Complement, Binary Offset).
61. `testMIDILogarithmicFrequencyMapping`: Pengujian pemetaan frekuensi audio eksponensial/logaritmik (20Hz - 20kHz).
62. `testMIDINoteToggleAndMomentaryBehavior`: Pengujian semantik tombol Note (Toggle mute/solo, Momentary, penyaringan Note On velocity 0).
63. `testMIDIChannelAndControllerFiltering`: Pengujian filter MIDI Channel (1..16 / All) dan nomor CC controller.
64. `testMIDILearnWorkflowAndTimeout`: Pengujian alur kerja interaktif MIDI Learn, pembatalan (Cancel), dan proteksi timeout.
65. `testMIDIDuplicateDetectionAndWarning`: Pengujian deteksi duplikasi mapping (sumber MIDI sama dengan target berbeda).
66. `testMIDIPresetSerializationAndFactoryProtection`: Pengujian serialisasi XML preset mapping MIDI dan proteksi preset pabrik.
67. `testMIDIStreamAndMonitorBusTargetIsolation`: Pengujian isolasi target parameter MIDI antara Stream Bus dan Monitor Bus.
68. `testPlaybackOpenFileValid`: Pengujian pembacaan file audio aktual multi-format dan ekstraksi metadata track.
69. `testPlaybackRejectInvalidFile`: Pengujian penolakan aman terhadap file korup atau ekstensi tidak valid tanpa crash.
70. `testPlaybackSeekAndPosition`: Pengujian akurasi scrubbing dan seek timeline milidetik.
71. `testPlaybackLoopRangeAndWrap`: Pengujian batasan loop start/end (start < end) dan seamless wrap-around looping.
72. `testPlaybackMarkerCRUD`: Pengujian penambahan, penamaan, dan penghapusan cue marker waktu.
73. `testPlaybackPlaylistSaveLoadAndIntegrity`: Pengujian integritas round-trip XML playlist.
74. `testPlaybackMissingTrackHandling`: Pengujian deteksi track yang hilang di disk dan penandaan status unavailable.
75. `testPlaybackGainAndMute`: Pengujian penskalaan linear output gain fader dan isolasi mute audio.
76. `testPlaybackSpeedRatioAndResampling`: Pengujian rasio resampling playback speed (0.5x s.d. 2.0x).
77. `testTestSignalSineFrequencyAndPhaseDelta`: Pengujian kalkulasi phase delta oscillator sinus 440 Hz terkalibrasi.
78. `testTestSignalLevelClampingAndDbConversion`: Pengujian konversi level dBFS ke gain linear dan boundary clamping (-90 s.d. 0 dBFS).
79. `testTestSignalWhiteAndPinkNoiseSpectralDifferences`: Pengujian perbedaan kurva spektral nyata (filter 1/f Paul Kellet) antara White Noise dan Pink Noise.
80. `testTestSignalNyquistLimitEnforcement`: Pengujian pemotongan batas atas frekuensi osilator berdasarkan Nyquist limit (sampleRate / 2).
81. `testTestSignalRoutingToBusesAndChannels`: Pengujian perutean fleksibel generator ke Input Channels 1-4, Stream Bus, dan Monitor Bus dalam mode Stereo/L/R.
82. `testTestSignalNoNaNOrInfinitySanitization`: Pengujian sanitasi floating-point terhadap nilai tidak valid (NaN/Inf) pada buffer generator.
83. `testTestSignalSilenceWhenStopped`: Pengujian kepastian nol mutlak (true silence) saat generator dinonaktifkan.
84. `testPlaybackSampleRateChangeResilience`: Pengujian ketahanan player terhadap pergantian sample rate hardware runtime (44.1k ke 48k/96k).
85. `testVST3PluginLibraryModelSerializationAndDefaults`: Pengujian serialisasi database plugin, status favorites, dan persistensi plugin default.
86. `testVST3PluginScanSettingsAndPaths`: Pengujian manajemen direktori scanning VST3 dan proteksi path sistem Windows.
87. `testVST3CatalogSearchAndFiltering`: Pengujian query pencarian, filter kategori, filter pabrikan, dan filter favorit.
88. `testVST3PluginChainSlotManagementAndLifecycle`: Pengujian penambahan, bypass, reordering, dan penghapusan slot plugin dalam rack chain.
89. `testVST3PluginChainXMLSerializationAndStateRestore`: Pengujian serialisasi XML rack chain dan restorasi state biner.
90. `testVST3MissingPluginPlaceholderGracefulHandling`: Pengujian placeholder aman saat plugin tidak ditemukan tanpa menyebabkan crash.
91. `testVST3AudioProcessingFormatAndDryFallbackSafety`: Pengujian dry signal fallback saat pemrosesan plugin dinonaktifkan atau gagal.
92. `testVST3OutOfProcessWorkerTimeoutAndWatchdog`: Pengujian timeout IPC dan mekanisme pemulihan otomatis watchdog proses worker.
93. `testVST3SceneManagerPluginIntegration`: Pengujian integrasi serialisasi rantai plugin dengan Scene Manager utama.
94. `testBufferDurationAndSampleRateConversion`: Pengujian akurasi rumus durasi 1 buffer `(samples / SR * 1000)` dan konversi sample rate.
95. `testLatencyReportingAndUnavailableHandling`: Pengujian pelabelan latensi driver vs durasi buffer, konversi sampel ke ms, dan fallback transparan `N/A`.
96. `testCPULoadCalculationAndBoundaryClamping`: Pengujian kalkulasi beban audio callback `(elapsed / budget * 100)`, peredaman smoothing EMA, dan clamping 0..100%.
97. `testDeadlineMissAndDropoutCounters`: Pengujian deteksi callback overrun (`elapsed > budget`), increment atomic counter, dan thread-safe rate-limited logging.
98. `testDiagnosticsQueueLockFreeRingBufferAndOverflowHandling`: Pengujian lock-free bounded SPSC circular ring buffer (kapasitas 512), zero-allocation, dan penanganan graceful overflow saat antrean penuh.
99. `testDiagnosticsLogManagerSeverityFilteringAndTrimming`: Pengujian filtering severity log (All, Info, Warning, Error), deduplikasi event, dan auto-trim riwayat 1.000 entri.
100. `testDiagnosticReportSanitizationAndExport`: Pengujian generator laporan diagnostik lengkap, pembersihan data sensitif (sanitasi kredensial/token), dan ekspor ke file teks.
101. `testDeviceEventLifecycleAndFailureHandling`: Pengujian transisi siklus hidup perangkat, logging event device open/close/error/buffer change, dan ketahanan shutdown aman.

**Hasil validasi build ini: 150 PASSED, 0 FAILED.**

---

## 6. Batasan dan Catatan Operasional
- **Sinkronisasi Dua Clock Berbeda**: Jika output Stream dikirim ke virtual cable (clock Windows) dan Monitor dikirim ke physical USB interface (clock interface), clock drift dapat terjadi pada sesi streaming berjam-jam tanpa resampler asinkron antar driver. Penggunaan interface dengan direct multi-channel output (Ch 1/2 Stream, Ch 3/4 Monitor) sangat disarankan untuk clocking yang 100% sempurna.
- **Hardware Testing**: Tes software DSP, Audio Engine, VST3 Host, dan Diagnostic Telemetry dijalankan tanpa mengklaim validasi perangkat fisik. Pengujian perangkat USB audio interface dan driver ASIO spesifik bergantung pada perangkat keras yang terpasang di sistem pengguna.
- **Deteksi Audio Dropout Backend**: Nilai dropout/underrun yang ditampilkan adalah yang berhasil dilaporkan oleh driver backend aktif (ASIO buffer switch flags / WASAPI discontinuity notifications) serta deadline miss yang terdeteksi secara internal oleh mixer. JUCE dan Windows tidak selalu menjamin pelaporan underrun dari setiap jenis driver pihak ketiga.
- **Pengukuran Latensi Round-Trip**: Nilai latensi input dan output yang ditampilkan adalah nilai resmi yang dilaporkan oleh driver audio interface. Nilai ini tidak mencakup latensi konverter analog-ke-digital (ADC/DAC) eksternal atau buffer tambahan OS yang hanya bisa diukur melalui loopback physical cable test.
