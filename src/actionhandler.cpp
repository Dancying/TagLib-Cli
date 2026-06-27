#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include <taglib/mpegfile.h>

#include "actionhandler.h"
#include "common.h"
#include "tagmanager.h"
#include "utils.h"

// executeWritePair 处理单个键值对的写入调度
int executeWritePair(const std::string& pairStr, const fs::path& targetPath, const std::string& filename, const std::string& ext, bool isNativeMp3, TagLib::PropertyMap& properties, TagLib::FileRef& f) {
    auto [field, val] = parseKeyValue(pairStr);
    if (field.empty()) {
        std::cerr << "taglib-cli: invalid write format '" << pairStr << "'\n";
        return 1;
    }
    fs::path valPath(val);
    const size_t valLen = val.length();
    const bool isAbs = valPath.is_absolute();
    // 检查参数值是否包含绝对路径或显式的相对路径前缀
    const bool hasPathIndicator = isAbs ? true : ((valLen >= 2 && val[0] == '.' && val[1] == '/') || (valLen >= 3 && val[0] == '.' && val[1] == '.' && val[2] == '/'));
    bool isFile = false;
    if (hasPathIndicator) {
        if (fs::exists(valPath) && fs::is_regular_file(valPath)) {
            isFile = true;
        } else {
            std::cerr << "taglib-cli: specified file path '" << val << "' does not exist\n";
            return 1;
        }
    }
    // 判断目标字段是否属于多媒体封面图像对象
    if (isCoverField(field)) {
        if (isFile) {
            std::string injectArg = field + "=" + val;
            return handleInjectAction(injectArg, targetPath, filename, ext);
        }
        std::cerr << "taglib-cli: cover field '" << field << "' requires a valid image file path (using absolute or relative prefix)\n";
        return 1;
    }
    std::string finalValue = isFile ? readFileToString(val) : val;
    std::vector<std::string> tempPairs = {field + "=" + finalValue};
    return handleWriteAction(tempPairs, targetPath, filename, ext, isNativeMp3, properties, f);
}

// handleDeleteAction 执行音频标签抹除操作
int handleDeleteAction(const std::vector<std::string>& deleteFields, const fs::path& targetPath, const std::string& filename, const std::string& ext, bool isNativeMp3, TagLib::PropertyMap& properties, TagLib::FileRef& f) {
    bool propertiesChanged = false;
    bool coverArtRemoved = false;
    for (const auto& key : deleteFields) {
        bool match = false;
        for (const auto& meta : METADATA_REGISTRY) {
            if (meta.canonicalKey != key) {
                continue;
            }
            match = true;
            if (key == "Front_Cover" || key == "Back_Cover" || key == "File_Icon" || key == "During_Performance") {
                removeCoverArt(targetPath, ext, key, false);
                coverArtRemoved = true;
            } else if (isNativeMp3) {
                TagLib::MPEG::File mf(targetPath.c_str());
                removeId3v2Frame(mf, meta.id3v2FrameId);
                removeId3v1Field(mf, meta.canonicalKey);
                mf.save();
            } else {
                properties.erase(meta.tagLibPropKey);
                propertiesChanged = true;
            }
            std::cout << filename << ": field stripped successfully: [" << key << "]\n";
            break;
        }
        if (!match) {
            std::cerr << "taglib-cli: " << key << ": Unknown canonical metadata property field\n";
            return 1;
        }
    }
    if (coverArtRemoved && !isNativeMp3) {
        f = TagLib::FileRef(targetPath.c_str());
    }
    if (propertiesChanged && !isNativeMp3) {
        f.file()->setProperties(properties);
        f.file()->save();
    }
    return 0;
}

// handleExtractAction 执行音频内嵌入封面导出操作
int handleExtractAction(const std::string& coverRawExtract, const fs::path& targetPath, const std::string& filename, const std::string& ext) {
    std::string targetType = "Front_Cover";
    std::string savePathStr = coverRawExtract;
    size_t eqPos = coverRawExtract.find('=');
    if (eqPos != std::string::npos) {
        targetType = coverRawExtract.substr(0, eqPos);
        savePathStr = coverRawExtract.substr(eqPos + 1);
    }
    if (targetType != "Front_Cover" && targetType != "Back_Cover" && targetType != "File_Icon" && targetType != "During_Performance") {
        std::cerr << "taglib-cli: " << targetType << ": Invalid artwork metadata type specified\n";
        return 1;
    }
    auto cover = getFilePictureStatus(targetPath, ext, targetType);
    if (!cover.isValid) {
        std::cerr << "taglib-cli: " << filename << ": No " << targetType << " resolved\n";
        return 1;
    }
    std::ofstream out(savePathStr, std::ios::binary);
    out.write(cover.rawBinary.data(), cover.rawBinary.size());
    std::cout << filename << ": extracted " << targetType << " stream successfully\n";
    return 0;
}

// handleInjectAction 执行封面图片注入音频操作
int handleInjectAction(const std::string& coverRawInject, const fs::path& targetPath, const std::string& filename, const std::string& ext) {
    std::string targetType = "Front_Cover";
    std::string imgPathStr = coverRawInject;
    size_t eqPos = coverRawInject.find('=');
    if (eqPos != std::string::npos) {
        targetType = coverRawInject.substr(0, eqPos);
        imgPathStr = coverRawInject.substr(eqPos + 1);
    }
    if (targetType != "Front_Cover" && targetType != "Back_Cover" && targetType != "File_Icon" && targetType != "During_Performance") {
        std::cerr << "taglib-cli: " << targetType << ": Invalid artwork metadata type specified\n";
        return 1;
    }
    writeCoverArtDirect(targetPath, ext, targetType, imgPathStr);
    std::cout << filename << ": " << targetType << " synchronized successfully\n";
    return 0;
}

// handleWriteAction 执行音频标签修改与写入操作
int handleWriteAction(const std::vector<std::string>& writePairs, const fs::path& targetPath, const std::string& filename, const std::string& ext, bool isNativeMp3, TagLib::PropertyMap& properties, TagLib::FileRef& f) {
    for (const auto& pair : writePairs) {
        size_t delimiter = pair.find('=');
        if (delimiter == std::string::npos) {
            std::cerr << "taglib-cli: " << pair << ": Invalid format. Expected field=value\n";
            return 1;
        }
        std::string key = pair.substr(0, delimiter);
        std::string val = pair.substr(delimiter + 1);
        bool match = false;
        for (const auto& meta : METADATA_REGISTRY) {
            if (meta.canonicalKey != key) {
                continue;
            }
            match = true;
            if (isNativeMp3) {
                TagLib::MPEG::File mf(targetPath.c_str());
                injectId3v2Frame(mf, meta.id3v2FrameId, val);
                mf.save();
            } else {
                properties[meta.tagLibPropKey] = TagLib::String(val, TagLib::String::UTF8);
                f.file()->setProperties(properties);
                f.file()->save();
            }
            std::cout << filename << ": modification committed: [" << key << "] -> '" << val << "'\n";
            break;
        }
        if (!match) {
            std::cerr << "taglib-cli: " << key << ": Unknown canonical metadata property field\n";
            return 1;
        }
    }
    return 0;
}
