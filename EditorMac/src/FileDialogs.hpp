#pragma once

#include <cstddef>

namespace hurrican::editor {

// Uses macOS native Open/Save panels. Returns false when cancelled or when the selected
// path does not fit in the caller-provided buffer.
bool ChooseMapToOpen(char* destination, std::size_t capacity);
bool ChooseMapToSave(char* destination, std::size_t capacity);

} // namespace hurrican::editor
