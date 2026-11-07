#import <AppKit/AppKit.h>

#include "FileDialogs.hpp"

#include <cstring>

namespace hurrican::editor {

bool ChooseMapToOpen(char* destination, std::size_t capacity) {
    if (destination == nullptr || capacity == 0)
        return false;

    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.canChooseFiles = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.allowedFileTypes = @[ @"map" ];
        panel.title = @"Open Hurrican map";

        if ([panel runModal] != NSModalResponseOK || panel.URL == nil)
            return false;

        const char* path = panel.URL.fileSystemRepresentation;
        const std::size_t length = std::strlen(path);
        if (length + 1 > capacity)
            return false;
        std::memcpy(destination, path, length + 1);
        return true;
    }
}

bool ChooseMapToSave(char* destination, std::size_t capacity) {
    if (destination == nullptr || capacity == 0)
        return false;

    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        panel.allowedFileTypes = @[ @"map" ];
        panel.nameFieldStringValue = @"NewLevel.map";
        panel.title = @"Save Hurrican map";

        if ([panel runModal] != NSModalResponseOK || panel.URL == nil)
            return false;

        const char* path = panel.URL.fileSystemRepresentation;
        const std::size_t length = std::strlen(path);
        if (length + 1 > capacity)
            return false;
        std::memcpy(destination, path, length + 1);
        return true;
    }
}

} // namespace hurrican::editor
