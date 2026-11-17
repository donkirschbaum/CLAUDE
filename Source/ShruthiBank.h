// Factory bank access and 92-byte Shruthi-1 patch decoding. GPL v3.0.
#pragma once
#include "ShruthiEngine.h"
#include <vector>
#include <cstdint>

namespace shruthi {
int numFactoryPrograms();
const char* factoryName(int i);
const char* factoryCategory(int i);
// Decodes a firmware patch (92 bytes) and an optional sequence (32 bytes).
// Returns true if the patch carries sequencer settings (version '%').
bool decodePatch(const std::vector<uint8_t>& patch, const std::vector<uint8_t>* seq, Patch& out, const Patch* keepSeqFrom);
bool loadFactory(int i, Patch& out, const Patch& current);
}
