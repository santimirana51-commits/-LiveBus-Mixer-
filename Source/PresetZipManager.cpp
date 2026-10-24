#include "PresetZipManager.h"

bool PresetZipManager::exportPresetsToZip(const juce::File& destZipFile, const juce::File& scenesDirectory)
{
    if (!scenesDirectory.isDirectory())
        return false;

    if (destZipFile.exists())
        destZipFile.deleteFile();

    juce::ZipFile::Builder builder;
    auto files = scenesDirectory.findChildFiles(juce::File::findFiles, false, "*.xml");
    if (files.isEmpty())
        return false;

    for (const auto& f : files)
    {
        builder.addFile(f, 9, f.getFileName());
    }

    juce::FileOutputStream stream(destZipFile);
    if (!stream.openedOk())
        return false;

    return builder.writeToStream(stream, nullptr);
}

bool PresetZipManager::importPresetsFromZip(const juce::File& sourceZipFile, const juce::File& scenesDirectory)
{
    if (!sourceZipFile.existsAsFile())
        return false;

    scenesDirectory.createDirectory();

    juce::ZipFile zip(sourceZipFile);
    if (zip.getNumEntries() == 0)
        return false;

    auto result = zip.uncompressTo(scenesDirectory);
    return result.wasOk();
}
