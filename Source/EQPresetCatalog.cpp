#include "EQPresetCatalog.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace LiveBus
{
namespace
{

struct PresetNameGroup
{
    EQPresetCategory category;
    const char* names;
};

const std::array<PresetNameGroup, 6> presetNameGroups{{
    { EQPresetCategory::Vocal,
      "Clean Vocal|Warm Vocal|Bright Vocal|Vocal Presence|Deep Voice|Rich Baritone|"
      "Smooth Vocal|Clear Vocal|Airy Vocal|Intimate Vocal|Soft Vocal|Powerful Vocal|"
      "Female Vocal|Male Vocal|Vocal De-Ess|Radio Voice|Podcast Voice|Narration|"
      "Singing Vocal|Vocal Flat" },
    { EQPresetCategory::Streaming,
      "Live Streaming|OBS Streaming|Gaming Mic|Discord Voice|Voice Chat|Conference|"
      "Broadcast|Streamer Bright|Streamer Warm|Streamer Deep|Streamer Natural|"
      "Competitive Gaming|ASMR Voice|Telephone Voice|Megaphone" },
    { EQPresetCategory::Music,
      "Flat|Bass Boost|Deep Bass|Punchy Bass|Warm Music|Bright Music|Vocal Focus|"
      "Pop Music|Rock Music|Acoustic|Electronic|Hip-Hop|Classical|Jazz|Lo-Fi|EDM|"
      "Cinematic|Music Detail|Smooth Music|Night Listening" },
    { EQPresetCategory::Monitoring,
      "Monitor Flat|Monitor Warm|Monitor Bright|Monitor Vocal Presence|"
      "Headphone Correction|Studio Reference|Critical Listening|Bass Check|"
      "Midrange Check|Treble Check|Mono Check|Low Volume Listening" },
    { EQPresetCategory::TonalCorrection,
      "Muddy Voice Fix|Boxy Voice Fix|Nasal Voice Fix|Harshness Control|"
      "Sibilance Control|Boomy Bass Fix|Thin Voice Fix|Dull Audio Fix|Rumble Cut|"
      "High Cut|Low Cut|Midrange Clarity" },
    { EQPresetCategory::Character,
      "Vintage Radio|Old Telephone|Lo-Fi Voice|Dark Voice|Airy Voice|"
      "Warm Broadcast|Bright Broadcast|Studio Podcast|Gaming Broadcast|"
      "Cinematic Voice" }
}};

juce::String categoryName(EQPresetCategory category)
{
    switch (category)
    {
        case EQPresetCategory::Vocal: return "Vocal";
        case EQPresetCategory::Streaming: return "Streaming and communication";
        case EQPresetCategory::Music: return "Music";
        case EQPresetCategory::Monitoring: return "Headphone and monitoring";
        case EQPresetCategory::TonalCorrection: return "Tonal correction";
        case EQPresetCategory::Character: return "Sound character";
    }
    return "EQ";
}

juce::String categoryId(EQPresetCategory category)
{
    switch (category)
    {
        case EQPresetCategory::Vocal: return "vocal";
        case EQPresetCategory::Streaming: return "streaming";
        case EQPresetCategory::Music: return "music";
        case EQPresetCategory::Monitoring: return "monitoring";
        case EQPresetCategory::TonalCorrection: return "correction";
        case EQPresetCategory::Character: return "character";
    }
    return "eq";
}

juce::String makeId(EQPresetCategory category, const juce::String& name)
{
    return categoryId(category) + "." +
           name.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 ")
               .trim().replace(" ", "-");
}

bool hasAnyToken(const juce::String& text,
                 std::initializer_list<const char*> tokens)
{
    for (const auto* token : tokens)
        if (text.contains(token))
            return true;
    return false;
}

void makeProfile(EQPresetDefinition& preset)
{
    const auto name = preset.name.toLowerCase();
    auto& bands = preset.bands;
    const bool flat = name.contains("flat") ||
                      name.contains("critical listening") ||
                      name.contains("studio reference") ||
                      name.contains("headphone correction") ||
                      name.contains("mono check") ||
                      name.contains("monitor flat");
    const bool telephone = hasAnyToken(name, { "telephone", "megaphone",
                                                "vintage radio", "old telephone" });
    const bool dark = hasAnyToken(name, { "dark", "night listening",
                                          "high cut", "smooth" });
    const bool warm = hasAnyToken(name, { "warm", "bass", "deep voice",
                                          "baritone", "boomy", "low volume" });
    const bool bright = hasAnyToken(name, { "bright", "airy", "detail",
                                            "clarity", "treble" });
    const bool sibilance = hasAnyToken(name, { "de-ess", "sibilance", "harsh",
                                               "gaming mic", "competitive" });
    const bool muddy = hasAnyToken(name, { "muddy", "boxy", "nasal",
                                           "midrange clarity" });
    const bool voice = preset.category == EQPresetCategory::Vocal ||
                       preset.category == EQPresetCategory::Streaming ||
                       preset.category == EQPresetCategory::Character;

    const float hpf = telephone ? 320.0f
        : name.contains("female") ? 95.0f
        : hasAnyToken(name, { "deep", "baritone", "bass", "rumble" }) ? 55.0f
        : voice ? 75.0f : 30.0f;
    const bool enableHpf = !flat &&
        (telephone || voice || hasAnyToken(name, { "rumble", "low cut" }));
    const bool enableLpf = telephone || name.contains("high cut") ||
                           name.contains("lo-fi") ||
                           name.contains("old telephone") ||
                           name.contains("vintage radio");

    bands[0] = { EQPresetFilterType::HighPass, hpf, 0.0f, 0.707f, enableHpf };
    bands[1] = { EQPresetFilterType::LowShelf, 120.0f,
                 flat ? 0.0f : warm ? 2.0f : name.contains("thin") ? 2.5f
                     : name.contains("punchy") ? 1.5f
                     : name.contains("dark") ? -1.0f : 0.0f,
                 0.707f, true };
    bands[2] = { EQPresetFilterType::Bell, 250.0f,
                 flat ? 0.0f : muddy || name.contains("boomy") ||
                     name.contains("warm")
                     ? -1.5f : name.contains("deep") ? 1.0f : 0.0f,
                 1.0f, true };
    bands[3] = { EQPresetFilterType::Bell, 500.0f,
                 flat ? 0.0f : hasAnyToken(name, { "boxy", "muddy", "nasal" })
                     ? -2.5f : name.contains("intimate") ? 1.0f : 0.0f,
                 1.15f, true };
    bands[4] = { EQPresetFilterType::Bell, 1200.0f,
                 flat ? 0.0f : name.contains("nasal") ? -2.0f
                     : name.contains("telephone") || name.contains("megaphone")
                         ? 2.0f : 0.0f,
                 1.0f, true };
    bands[5] = { EQPresetFilterType::Bell, 2800.0f,
                 flat ? 0.0f : sibilance ? -1.5f
                     : bright || name.contains("presence") || name.contains("clear")
                         || name.contains("broadcast") || name.contains("gaming")
                         || name.contains("vocal focus") ? 2.0f : 0.5f,
                 1.0f, true };
    bands[6] = { EQPresetFilterType::Bell, 5600.0f,
                 flat ? 0.0f : sibilance ? -2.5f
                     : telephone ? -1.0f : bright ? 1.0f : 0.0f,
                 1.3f, true };
    bands[7] = enableLpf
        ? EQPresetBand{ EQPresetFilterType::LowPass,
                        telephone ? 3600.0f : 10000.0f, 0.0f, 0.707f, true }
        : EQPresetBand{ EQPresetFilterType::HighShelf, 10000.0f,
                        flat ? 0.0f : dark ? -1.5f
                            : bright || name.contains("presence") ||
                              name.contains("clear") || name.contains("detail")
                                ? 2.0f : 0.5f,
                        0.707f, true };

    if (name.contains("vocal flat") || name == "flat" ||
        name.contains("monitor flat"))
    {
        bands[0].enabled = false;
        bands[7].type = EQPresetFilterType::HighShelf;
        bands[7].gainDb = 0.0f;
        bands[7].enabled = true;
    }
}

double responseDb(const EQPresetDefinition& preset, float frequencyHz,
                 double sampleRate, bool bellFiltersOnly)
{
    const float safeSampleRate = (float)juce::jmax(8000.0, sampleRate);
    const float maxFrequency = (float)(safeSampleRate * 0.49);
    double magnitude = 1.0;
    for (const auto& band : preset.bands)
    {
        if (!band.enabled)
            continue;
        if (bellFiltersOnly && band.type != EQPresetFilterType::Bell)
            continue;

        const float frequency = juce::jlimit(10.0f, maxFrequency,
                                              band.frequencyHz);
        const float q = juce::jlimit(0.1f, 10.0f, band.q);
        const float gain = juce::Decibels::decibelsToGain(
            juce::jlimit(-18.0f, 18.0f, band.gainDb));
        juce::dsp::IIR::Coefficients<float>::Ptr coefficients;
        switch (band.type)
        {
            case EQPresetFilterType::HighPass:
                coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(
                    safeSampleRate, frequency, q);
                break;
            case EQPresetFilterType::LowShelf:
                coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                    safeSampleRate, frequency, q, gain);
                break;
            case EQPresetFilterType::HighShelf:
                coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                    safeSampleRate, frequency, q, gain);
                break;
            case EQPresetFilterType::LowPass:
                coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(
                    safeSampleRate, frequency, q);
                break;
            case EQPresetFilterType::Bell:
                coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                    safeSampleRate, frequency, q, gain);
                break;
        }
        magnitude *= coefficients->getMagnitudeForFrequency(frequencyHz,
                                                             safeSampleRate);
    }
    return juce::Decibels::gainToDecibels(magnitude, -120.0);
}

float clampedMappedGain(const EQPresetDefinition& preset, float frequency,
                        double sampleRate, bool bellFiltersOnly,
                        float maximumGain)
{
    const double response = responseDb(preset, frequency, sampleRate,
                                       bellFiltersOnly);
    return std::isfinite(response)
        ? juce::jlimit(-maximumGain, maximumGain, (float)response)
        : 0.0f;
}

} // namespace

const std::vector<EQPresetDefinition>& EQPresetCatalog::getFactoryPresets()
{
    static const auto presets = []
    {
        std::vector<EQPresetDefinition> result;
        for (const auto& group : presetNameGroups)
        {
            juce::String names(group.names);
            juce::StringArray entries;
            entries.addTokens(names, "|", {});
            for (const auto& name : entries)
            {
                EQPresetDefinition preset;
                preset.id = makeId(group.category, name);
                preset.name = name;
                preset.category = group.category;
                preset.description = categoryName(group.category) +
                    " starting curve. Adjust to the microphone, source, room, and listener.";
                makeProfile(preset);
                result.push_back(std::move(preset));
            }
        }
        return result;
    }();
    return presets;
}

const EQPresetDefinition* EQPresetCatalog::findFactoryPreset(
    const juce::String& id)
{
    const auto& presets = getFactoryPresets();
    const auto found = std::find_if(presets.begin(), presets.end(),
        [&id](const auto& preset) { return preset.id == id; });
    return found == presets.end() ? nullptr : &*found;
}

bool EQPresetCatalog::validate(const EQPresetDefinition& preset,
                               juce::String& error)
{
    if (preset.id.trim().isEmpty() || preset.name.trim().isEmpty())
    {
        error = "Preset ID and name must not be empty.";
        return false;
    }
    if (preset.formatVersion != 1)
    {
        error = "Unsupported EQ preset format version.";
        return false;
    }
    switch (preset.category)
    {
        case EQPresetCategory::Vocal:
        case EQPresetCategory::Streaming:
        case EQPresetCategory::Music:
        case EQPresetCategory::Monitoring:
        case EQPresetCategory::TonalCorrection:
        case EQPresetCategory::Character:
            break;
        default:
            error = "Invalid EQ preset category.";
            return false;
    }
    for (size_t index = 0; index < preset.bands.size(); ++index)
    {
        const auto& band = preset.bands[index];
        switch (band.type)
        {
            case EQPresetFilterType::HighPass:
            case EQPresetFilterType::LowShelf:
            case EQPresetFilterType::Bell:
            case EQPresetFilterType::HighShelf:
            case EQPresetFilterType::LowPass:
                break;
            default:
                error = "Invalid EQ filter type at band " +
                        juce::String((int)index + 1) + ".";
                return false;
        }
        if (!std::isfinite(band.frequencyHz) || band.frequencyHz < 20.0f ||
            band.frequencyHz > 20000.0f ||
            !std::isfinite(band.gainDb) || band.gainDb < -18.0f ||
            band.gainDb > 18.0f || !std::isfinite(band.q) ||
            band.q < 0.1f || band.q > 10.0f)
        {
            error = "Invalid EQ band parameters at band " +
                    juce::String((int)index + 1) + ".";
            return false;
        }
    }
    error.clear();
    return true;
}

double EQPresetCatalog::getMagnitudeDb(const EQPresetDefinition& preset,
                                       float frequencyHz, double sampleRate)
{
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0f ||
        !std::isfinite(sampleRate) || sampleRate < 8000.0)
        return 0.0;
    juce::String error;
    if (!validate(preset, error))
        return 0.0;
    return responseDb(preset, frequencyHz, sampleRate, false);
}

bool EQPresetCatalog::mapToEightBand(
    const EQPresetDefinition& preset,
    SceneData::EightBandEQSceneData& destination,
    juce::String& error)
{
    if (!validate(preset, error))
        return false;
    destination.masterEnabled = true;
    for (size_t index = 0; index < preset.bands.size(); ++index)
    {
        const auto& source = preset.bands[index];
        auto& target = destination.bands[index];
        target.type = (int)source.type;
        target.freq = source.frequencyHz;
        target.gainDb = source.gainDb;
        target.q = source.q;
        target.enabled = source.enabled;
    }
    return true;
}

bool EQPresetCatalog::mapToPuzu15Band(
    const EQPresetDefinition& preset, PuzuEQState& destination,
    juce::String& error, double sampleRate)
{
    if (!validate(preset, error))
        return false;
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0)
    {
        error = "Invalid sample rate for Puzu EQ mapping.";
        return false;
    }
    destination.enabled = true;
    for (size_t index = 0; index < destination.bands.size(); ++index)
    {
        auto& target = destination.bands[index];
        target.freq = defaultPuzuFreqs[index];
        target.q = defaultPuzuQs[index];
        target.enabled = true;
        target.type = PuzuFilterType::Bell;
        target.gainDb = clampedMappedGain(
            preset, target.freq, sampleRate, true, 18.0f);
    }
    if (preset.bands.front().enabled &&
        preset.bands.front().type == EQPresetFilterType::HighPass)
    {
        destination.bands.front().type = PuzuFilterType::HighPass;
        destination.bands.front().freq = preset.bands.front().frequencyHz;
        destination.bands.front().q = preset.bands.front().q;
        destination.bands.front().gainDb = 0.0f;
    }
    for (const auto& source : preset.bands)
    {
        if (!source.enabled ||
            (source.type != EQPresetFilterType::LowShelf &&
             source.type != EQPresetFilterType::HighShelf))
            continue;
        size_t closest = 0;
        float distance = std::numeric_limits<float>::max();
        for (size_t index = 0; index < numPuzuBands; ++index)
        {
            const float candidate = std::abs(std::log(
                defaultPuzuFreqs[index] / source.frequencyHz));
            if (candidate < distance)
            {
                closest = index;
                distance = candidate;
            }
        }
        auto& target = destination.bands[closest];
        target.type = source.type == EQPresetFilterType::LowShelf
            ? PuzuFilterType::LowShelf : PuzuFilterType::HighShelf;
        target.freq = source.frequencyHz;
        target.q = source.q;
        target.gainDb = source.gainDb;
    }
    if (preset.bands.back().enabled &&
        preset.bands.back().type == EQPresetFilterType::LowPass)
    {
        destination.bands.back().type = PuzuFilterType::LowPass;
        destination.bands.back().freq = preset.bands.back().frequencyHz;
        destination.bands.back().q = preset.bands.back().q;
        destination.bands.back().gainDb = 0.0f;
    }
    return true;
}

bool EQPresetCatalog::mapToMonitor31Band(
    const EQPresetDefinition& preset, MonitorGraphicEQ31State& destination,
    juce::String& error, double sampleRate)
{
    if (!validate(preset, error))
        return false;
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0)
    {
        error = "Invalid sample rate for Monitor Graphic EQ mapping.";
        return false;
    }
    destination.enabled = true;
    for (size_t index = 0; index < destination.gainDb.size(); ++index)
        destination.gainDb[index] = clampedMappedGain(
            preset, monitorGraphicEQFrequencies[index], sampleRate, false, 12.0f);
    return true;
}

bool EQPresetCatalog::mapToMainOutput24Band(
    const EQPresetDefinition& preset, MainOutputEQ24State& destination,
    juce::String& error, double sampleRate)
{
    if (!validate(preset, error))
        return false;
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0)
    {
        error = "Invalid sample rate for Main Output EQ mapping.";
        return false;
    }
    destination.enabled = true;
    for (size_t index = 0; index < destination.bands.size(); ++index)
    {
        auto& target = destination.bands[index];
        target.freq = mainOutputEQFrequencies[index];
        target.gainDb = clampedMappedGain(
            preset, target.freq, sampleRate, true, 18.0f);
        target.q = 0.707f;
        target.enabled = true;
        target.type = MainOutputEQFilterType::Bell;
    }
    if (preset.bands.front().enabled &&
        preset.bands.front().type == EQPresetFilterType::HighPass)
    {
        destination.bands.front().type = MainOutputEQFilterType::HighPass;
        destination.bands.front().freq = preset.bands.front().frequencyHz;
        destination.bands.front().gainDb = 0.0f;
    }
    for (const auto& source : preset.bands)
    {
        if (!source.enabled ||
            (source.type != EQPresetFilterType::LowShelf &&
             source.type != EQPresetFilterType::HighShelf))
            continue;
        size_t closest = 0;
        float distance = std::numeric_limits<float>::max();
        for (size_t index = 0; index < mainOutputEQFrequencies.size(); ++index)
        {
            const float candidate = std::abs(std::log(
                mainOutputEQFrequencies[index] / source.frequencyHz));
            if (candidate < distance)
            {
                closest = index;
                distance = candidate;
            }
        }
        auto& target = destination.bands[closest];
        target.type = source.type == EQPresetFilterType::LowShelf
            ? MainOutputEQFilterType::LowShelf
            : MainOutputEQFilterType::HighShelf;
        target.freq = source.frequencyHz;
        target.q = source.q;
        target.gainDb = source.gainDb;
    }
    const auto& lastBand = preset.bands.back();
    if (lastBand.enabled && lastBand.type == EQPresetFilterType::LowPass)
    {
        destination.bands.back().type = MainOutputEQFilterType::LowPass;
        destination.bands.back().freq = lastBand.frequencyHz;
        destination.bands.back().gainDb = 0.0f;
        destination.bands.back().q = lastBand.q;
    }
    return true;
}

} // namespace LiveBus
