#include "OBSGuideComponent.h"

namespace LiveBus
{

OBSGuideComponent::OBSGuideComponent(OBSIntegrationService& service)
    : obsService(service)
{
    titleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00f5d4));
    addAndMakeVisible(titleLabel);

    closeBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222a3a));
    closeBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xfff0f6fc));
    closeBtn.onClick = [this]() {
        if (onClose) onClose();
    };
    addAndMakeVisible(closeBtn);

    guideText.setMultiLine(true);
    guideText.setReadOnly(true);
    guideText.setCaretVisible(false);
    guideText.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff12161f));
    guideText.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe6edf3));
    guideText.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff1e2636));
    guideText.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(guideText);

    updateGuideContent();
}

void OBSGuideComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff161b24));
    g.setColour(juce::Colour(0xff00f5d4).withAlpha(0.6f));
    g.drawRect(getLocalBounds(), 1);
}

void OBSGuideComponent::resized()
{
    auto b = getLocalBounds().reduced(16);
    auto topRow = b.removeFromTop(32);
    closeBtn.setBounds(topRow.removeFromRight(110));
    titleLabel.setBounds(topRow);

    b.removeFromTop(10);
    guideText.setBounds(b);
}

void OBSGuideComponent::updateGuideContent()
{
    auto virtualDevices = obsService.getAvailableVirtualCables();

    juce::String devStatus;
    if (virtualDevices.isEmpty())
    {
        devStatus = "(!) PERINGATAN: Tidak ada virtual audio device yang terdeteksi di Windows Anda saat ini.\n"
                    "    Silakan unduh dan instal salah satu driver berikut terlebih dahulu:\n"
                    "    1. VB-CABLE Driver Pack (gratis dari vb-audio.com)\n"
                    "    2. Virtual Audio Cable (VAC) oleh Eugene Muzychenko\n"
                    "    3. VoiceMeeter Virtual IO\n";
    }
    else
    {
        devStatus = "[+] Perangkat Virtual Terdeteksi di Sistem Anda:\n";
        for (const auto& d : virtualDevices)
        {
            devStatus += "    - " + d + " (" + OBSIntegrationService::identifyVirtualDriverType(d) + ")\n";
        }
    }

    juce::String content =
        "===============================================================================\n"
        " PANDUAN INTEGRASI AUDIO LIVEBUS MIXER KE OBS STUDIO\n"
        "===============================================================================\n\n"
        + devStatus + "\n"
        "-------------------------------------------------------------------------------\n"
        "LANGKAH 1: PILIH PERANGKAT VIRTUAL SEBAGAI STREAM BUS OUTPUT DI LIVEBUS MIXER\n"
        "-------------------------------------------------------------------------------\n"
        "1. Di tab 'OBS Studio' atau tab 'Devices', temukan bagian 'Stream Bus Output Device'.\n"
        "2. Pilih kabel virtual audio yang terdeteksi di atas (misalnya 'Virtual Audio Cable' atau 'CABLE Input').\n"
        "3. LiveBus Mixer akan otomatis mengalirkan sinyal audio Stream Bus murni ke perangkat tersebut.\n"
        "   Catatan: Monitor Bus TIDAK dialirkan ke kabel virtual ini sehingga pendengaran di headphone Anda\n"
        "   tetap terpisah dan tidak bocor ke penonton live streaming OBS.\n\n"
        "-------------------------------------------------------------------------------\n"
        "LANGKAH 2: BUKA OBS STUDIO DAN TAMBAHKAN AUDIO SOURCE\n"
        "-------------------------------------------------------------------------------\n"
        "1. Buka aplikasi OBS Studio di Windows Anda.\n"
        "2. Pada panel 'Sources', klik tombol '+' lalu pilih 'Audio Input Capture'.\n"
        "3. Beri nama sumber audio, misalnya: 'LiveBus Stream Bus'.\n"
        "4. Klik 'OK'.\n\n"
        "-------------------------------------------------------------------------------\n"
        "LANGKAH 3: PILIH SISI PENERIMA KABEL VIRTUAL DI OBS STUDIO\n"
        "-------------------------------------------------------------------------------\n"
        "Di dialog properties Audio Input Capture OBS:\n"
        "- Pada dropdown 'Device', pilih sisi penerima dari kabel virtual Anda:\n"
        "  * Jika menggunakan VB-CABLE: Pilih 'CABLE Output (VB-Audio Virtual Cable)'.\n"
        "  * Jika menggunakan Virtual Audio Cable (VAC): Pilih 'Line 1 (Virtual Audio Cable)'.\n"
        "- Klik 'OK'.\n\n"
        "-------------------------------------------------------------------------------\n"
        "LANGKAH 4: KIRIM NADA UJI (TEST TONE) DARI LIVEBUS MIXER UNTUK VERIFIKASI\n"
        "-------------------------------------------------------------------------------\n"
        "1. Di tab 'OBS Studio' LiveBus Mixer, klik tombol 'Send Test Tone to OBS'.\n"
        "2. LiveBus Mixer akan menyiarkan sinyal sinus 440 Hz pada level aman -18.0 dBFS.\n"
        "3. Perhatikan panel 'Audio Mixer' di OBS Studio: Meter indikator 'LiveBus Stream Bus' harus bergerak\n"
        "   dan berada di zona hijau-kuning (~ -18 dBFS).\n"
        "4. Klik tombol 'Stop Test Tone' setelah selesai mengonfirmasi gerak meter.\n\n"
        "-------------------------------------------------------------------------------\n"
        "LANGKAH 5: PENCEGAHAN FEEDBACK LOOP DAN DOUBLE MONITORING\n"
        "-------------------------------------------------------------------------------\n"
        "PENTING UNTUK MENJAGA KEJERNIHAN SUARA:\n"
        "1. Di OBS Studio, klik ikon roda gigi (Settings) pada Audio Mixer -> 'Advanced Audio Properties'.\n"
        "2. Pastikan kolom 'Audio Monitoring' untuk sumber 'LiveBus Stream Bus' diatur ke 'Monitor Off'.\n"
        "   (Alasan: Anda sudah mendengarkan diri Anda sendiri langsung melalui Monitor Bus LiveBus Mixer!\n"
        "   Jika disetel ke 'Monitor and Output' di OBS, Anda akan mendengar gema/delay latency OBS).\n"
        "3. Pastikan mikrofon fisik Anda di OBS Studio di-Mute agar suara mikrofon tidak masuk dua kali.\n"
        "   Semua mikrofon, gitar, dan musik sekarang dikendalikan terpusat melalui LiveBus Mixer!\n\n"
        "-------------------------------------------------------------------------------\n"
        "DUAL-DEVICE INDEPENDENT MONITORING (HEADPHONES + VIRTUAL CABLE)\n"
        "-------------------------------------------------------------------------------\n"
        "Jika Anda ingin mendengarkan Monitor Bus di headphone fisik sementara Stream Bus dikirim ke OBS:\n"
        "- Gunakan Mode 'Dedicated Virtual Device' di tab OBS Studio.\n"
        "- Monitor Bus akan diputar di perangkat primary Anda (Headphone / Realtek / USB Interface).\n"
        "- Stream Bus akan dialirkan secara mandiri ke Virtual Cable untuk ditangkap oleh OBS Studio.\n"
        "===============================================================================\n";

    guideText.setText(content);
}

} // namespace LiveBus
