#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>

struct TargetTagMetadata {
    std::string canonicalKey;
    std::string tagLibPropKey;
    std::string id3v2FrameId;
};

extern const std::vector<TargetTagMetadata> METADATA_REGISTRY;

#endif
