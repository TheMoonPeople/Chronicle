#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>

#include "../../../tools/dcdata/dcdata.hpp"

// The first start of an installed copy: the data directory is missing or holds no file, so the
// game asks for the disc in the system's file selector (the file-chooser portal inside Flatpak,
// which hands over the chosen file without any filesystem permission) and extracts it itself.

// True when root is not a directory or holds no regular file, as main's data check has it.
bool FirstRunDataMissing(const std::filesystem::path &root);

struct FirstRunOutcome {
    bool        ok = false;
    bool        cancelled = false;
    std::string error;
};

// Extracts source, a disc image or a directory holding DATA.DAT and DATA.HD2, into root on a worker
// thread and checks every file. It writes to "<root>.partial" and renames that over root only once
// all of it checks out, so a run cut short never leaves a directory the game would take for
// complete; a later run resumes from what is there. tick runs on the calling thread with the latest
// progress while the worker runs; returning false cancels.
FirstRunOutcome FirstRunExtract(const std::filesystem::path &source, const std::filesystem::path &root,
                                const std::function<bool(const dcdata::Progress &)> &tick);

enum class FirstRunSource {
    DiscImage,
    Folder,
};

// Stands in for the file selector: returns the chosen path, or nothing for a cancel, which then
// ends the first run. It also lets a headless run go through the flow.
using FirstRunChooser = std::function<std::optional<std::filesystem::path>(FirstRunSource)>;
void FirstRunSetChooser(FirstRunChooser chooser);

// Does nothing when the data directory has files, or for a headless run without a chooser.
// Otherwise opens a window, asks for the disc and extracts it into PathsDataRoot(), then closes
// the window again and returns, with the data in place or, when the person declined, without it
// (main's data check then exits with the no-data status and the manual dcdata command). A failed
// extraction shows the error and exits with status 1.
void FirstRunIfNoData(bool headless);
