#include "PresetZipManager.h"

#include <utility>
#include <vector>

namespace
{
constexpr int maxPresetArchiveEntries = 128;
constexpr juce::int64 maxPresetFileBytes = 4 * 1024 * 1024;
constexpr juce::int64 maxPresetArchiveBytes = 32 * 1024 * 1024;
constexpr juce::int64 maxPresetZipBytes = maxPresetArchiveBytes + 1024 * 1024;

bool isSafePresetFilename(const juce::String& filename)
{
    if (filename.isEmpty() || filename == "." || filename == ".." ||
        filename.endsWithChar('.') || filename.endsWithChar(' ') ||
        !filename.endsWithIgnoreCase(".xml"))
        return false;

    for (int i = 0; i < filename.length(); ++i)
    {
        const auto character = filename[i];
        if (character < 0x20 || character == '<' || character == '>' || character == ':' ||
            character == '"' || character == '/' || character == '\\' || character == '|' ||
            character == '?' || character == '*')
            return false;
    }

    const auto deviceName = filename.upToFirstOccurrenceOf(".", false, false).toUpperCase();
    if (deviceName == "CON" || deviceName == "PRN" || deviceName == "AUX" || deviceName == "NUL")
        return false;
    if (deviceName.length() == 4 &&
        (deviceName.startsWith("COM") || deviceName.startsWith("LPT")) &&
        deviceName[3] >= '1' && deviceName[3] <= '9')
        return false;

    return true;
}

struct StagingDirectory
{
    explicit StagingDirectory(juce::File pathToRemove) : path(std::move(pathToRemove)) {}
    ~StagingDirectory()
    {
        if (removeOnDestruction && path.exists())
            path.deleteRecursively();
    }

    juce::File path;
    bool removeOnDestruction = true;
};

struct ImportFile
{
    juce::File stagedFile;
    juce::File targetFile;
    juce::File backupFile;
    bool hadOriginal = false;
};
}

bool PresetZipManager::exportPresetsToZip(const juce::File& destZipFile, const juce::File& scenesDirectory)
{
    if (!scenesDirectory.isDirectory())
        return false;

    juce::ZipFile::Builder builder;
    const auto files = scenesDirectory.findChildFiles(juce::File::findFiles, false, "*.xml");
    if (files.isEmpty())
        return false;

    for (const auto& file : files)
        builder.addFile(file, 9, file.getFileName());

    const auto temporaryZip = destZipFile.getSiblingFile(
        destZipFile.getFileName() + ".tmp-" +
        juce::String(juce::Random::getSystemRandom().nextInt()));
    bool writeSucceeded = false;
    {
        juce::FileOutputStream stream(temporaryZip);
        if (!stream.openedOk())
            return false;
        writeSucceeded = builder.writeToStream(stream, nullptr);
        stream.flush();
        writeSucceeded = writeSucceeded && stream.getStatus().wasOk();
    }

    if (!writeSucceeded)
    {
        temporaryZip.deleteFile();
        return false;
    }

    const bool committed = destZipFile.existsAsFile()
        ? temporaryZip.replaceFileIn(destZipFile)
        : temporaryZip.moveFileTo(destZipFile);
    if (!committed)
        temporaryZip.deleteFile();
    return committed;
}

bool PresetZipManager::importPresetsFromZip(const juce::File& sourceZipFile, const juce::File& scenesDirectory)
{
    if (!sourceZipFile.existsAsFile() || sourceZipFile.getSize() > maxPresetZipBytes ||
        (!scenesDirectory.isDirectory() && !scenesDirectory.createDirectory()))
        return false;

    juce::ZipFile zip(sourceZipFile);
    const int entryCount = zip.getNumEntries();
    if (entryCount <= 0 || entryCount > maxPresetArchiveEntries)
        return false;

    juce::StringArray entryNames;
    juce::int64 totalUncompressedBytes = 0;
    for (int i = 0; i < entryCount; ++i)
    {
        const auto* entry = zip.getEntry(i);
        if (entry == nullptr || entry->isSymbolicLink || !isSafePresetFilename(entry->filename) ||
            entryNames.contains(entry->filename, true) || entry->uncompressedSize < 0 ||
            entry->uncompressedSize > maxPresetFileBytes)
            return false;

        totalUncompressedBytes += entry->uncompressedSize;
        if (totalUncompressedBytes > maxPresetArchiveBytes)
            return false;
        entryNames.add(entry->filename);
    }

    const auto stagingPath = scenesDirectory.getNonexistentChildFile(".LiveBusImport", "", false);
    if (!stagingPath.createDirectory())
        return false;
    StagingDirectory staging(stagingPath);
    const auto extractedDirectory = stagingPath.getChildFile("files");
    const auto backupDirectory = stagingPath.getChildFile("backups");
    if (!extractedDirectory.createDirectory() || !backupDirectory.createDirectory())
        return false;

    std::vector<ImportFile> importFiles;
    importFiles.reserve((std::size_t)entryCount);
    for (int i = 0; i < entryCount; ++i)
    {
        if (zip.uncompressEntry(i, extractedDirectory, false).failed())
            return false;

        const auto* entry = zip.getEntry(i);
        const auto stagedFile = extractedDirectory.getChildFile(entry->filename);
        if (!stagedFile.existsAsFile() || stagedFile.getSize() != entry->uncompressedSize ||
            !stagedFile.isAChildOf(extractedDirectory) || juce::XmlDocument::parse(stagedFile) == nullptr)
            return false;

        const auto targetFile = scenesDirectory.getChildFile(entry->filename);
        if (!targetFile.isAChildOf(scenesDirectory) || targetFile.isSymbolicLink() ||
            (targetFile.exists() && !targetFile.existsAsFile()))
            return false;

        ImportFile importFile{ stagedFile, targetFile, backupDirectory.getChildFile(entry->filename),
                               targetFile.existsAsFile() };
        if (importFile.hadOriginal && !targetFile.copyFileTo(importFile.backupFile))
            return false;
        importFiles.push_back(std::move(importFile));
    }

    std::size_t attemptedCount = 0;
    for (auto& importFile : importFiles)
    {
        ++attemptedCount;
        const bool moved = importFile.targetFile.existsAsFile()
            ? importFile.stagedFile.replaceFileIn(importFile.targetFile)
            : importFile.stagedFile.moveFileTo(importFile.targetFile);
        if (moved)
            continue;

        bool rollbackSucceeded = true;
        for (std::size_t i = 0; i < attemptedCount; ++i)
        {
            auto& attemptedFile = importFiles[i];
            const bool restored = attemptedFile.hadOriginal
                ? attemptedFile.backupFile.copyFileTo(attemptedFile.targetFile)
                : (!attemptedFile.targetFile.exists() || attemptedFile.targetFile.deleteFile());
            rollbackSucceeded = restored && rollbackSucceeded;
        }

        if (!rollbackSucceeded)
        {
            staging.removeOnDestruction = false;
            juce::Logger::writeToLog("Preset ZIP import rollback failed; recovery files retained at " +
                                     stagingPath.getFullPathName());
        }
        return false;
    }

    return true;
}
