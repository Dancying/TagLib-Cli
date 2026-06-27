#include <filesystem>
#include <iostream>
#include <unordered_map>
#include <vector>

#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>

#include "actionhandler.h"
#include "common.h"
#include "tagmanager.h"
#include "utils.h"
#include "version.h"

namespace fs = std::filesystem;

// printHelp 打印程序使用帮助信息
void printHelp() {
    std::cout << "Usage: taglib-cli [OPTIONS] <audio_file>\n\n"
              << "Options:\n"
              << "  -r, --read <field>                              Read and display the value of a tag (e.g., Title, Artist).\n"
              << "  -w, --write <field>=<val>                       Set or update a specific tag with text or file content.\n"
              << "  -d, --delete <field>                            Remove a specific tag completely from the audio file.\n"
              << "  -e, --extract [type=]path                       Save artwork to path. (Default type: Front_Cover)\n"
              << "  -i, --inject [type=]path                        Insert a JPG/PNG image into audio. (Default type: Front_Cover)\n"
              << "  -v, --version                                   Show app version, build info, and TagLib version.\n"
              << "  -h, --help                                      Show this help message and exit.\n\n"
              << "Operational Examples:\n"
              << "  taglib-cli -r Title -r Artist track.mp3         # Multiple read operations\n"
              << "  taglib-cli -w Front_Cover=cover.jpg track.mp3   # Write artwork via -w option\n"
              << "  taglib-cli -w Title=lyrics.txt track.mp3        # Write text content from file\n";
}

// printVersion 打印程序版本与构建配置信息
void printVersion() {
    std::cout << "taglib-cli version " << APPLICATION_VERSION << " (GNU/Unix Precision Audio Tag Tool)\n"
              << "Build Configuration: C++17 Standard, TagLib " << TAGLIB_MAJOR_VERSION << "." << TAGLIB_MINOR_VERSION << "\n";
}

// main 应用程序主入口函数
int main(int argc, char* argv[]) {
    // 检查参数基数并提前返回
    if (argc < 2) {
        printHelp();
        return 1;
    }
    std::string filename = "";
    std::vector<std::string> readFields;
    std::vector<std::string> writePairs;
    std::vector<std::string> deleteFields;
    std::vector<std::string> coverExtracts;
    std::vector<std::string> coverInjects;
    // 循环解析多组命令行输入参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        }
        if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        }
        if (arg == "-r" || arg == "--read") {
            if (i + 1 < argc) readFields.push_back(argv[++i]);
            continue;
        }
        if (arg == "-w" || arg == "--write") {
            if (i + 1 < argc) writePairs.push_back(argv[++i]);
            continue;
        }
        if (arg == "-d" || arg == "--delete") {
            if (i + 1 < argc) deleteFields.push_back(argv[++i]);
            continue;
        }
        if (arg == "-e" || arg == "--extract") {
            if (i + 1 < argc) coverExtracts.push_back(argv[++i]);
            continue;
        }
        if (arg == "-i" || arg == "--inject") {
            if (i + 1 < argc) coverInjects.push_back(argv[++i]);
            continue;
        }
        if (arg[0] == '-') {
            std::cerr << "taglib-cli: unrecognized option '" << arg << "'\nTry 'taglib-cli --help' for more information.\n";
            return 1;
        }
        filename = arg;
    }
    // 校验文件目标操作数
    if (filename.empty()) {
        std::cerr << "taglib-cli: missing file operand\nTry 'taglib-cli --help' for more information.\n";
        return 1;
    }
    fs::path targetPath(filename);
    if (!fs::exists(targetPath) || !fs::is_regular_file(targetPath)) {
        std::cerr << "taglib-cli: " << filename << ": No such file or directory\n";
        return 1;
    }
    // 规范化文件扩展名
    std::string ext = targetPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    // 实例化内核文件引用
    TagLib::FileRef f(targetPath.c_str());
    if (f.isNull()) {
        std::cerr << "taglib-cli: " << filename << ": Standard engine mapping block instantiation failed\n";
        return 1;
    }
    TagLib::PropertyMap properties = f.file()->properties();
    bool isNativeMp3 = (ext == ".mp3");
    // 按次序循环处理所有的写入操作
    if (!writePairs.empty()) {
        for (const auto& pairStr : writePairs) {
            int res = executeWritePair(pairStr, targetPath, filename, ext, isNativeMp3, properties, f);
            if (res != 0) {
                return res;
            }
        }
        return 0;
    }
    // 按次序循环处理所有的删除操作
    if (!deleteFields.empty()) {
        int res = handleDeleteAction(deleteFields, targetPath, filename, ext, isNativeMp3, properties, f);
        if (res != 0) {
            return res;
        }
        return 0;
    }
    // 按次序循环处理所有的封面注入操作
    if (!coverInjects.empty()) {
        for (const auto& injectStr : coverInjects) {
            int res = handleInjectAction(injectStr, targetPath, filename, ext);
            if (res != 0) {
                return res;
            }
        }
        return 0;
    }
    // 按次序循环处理所有的封面提取操作
    if (!coverExtracts.empty()) {
        for (const auto& extractStr : coverExtracts) {
            int res = handleExtractAction(extractStr, targetPath, filename, ext);
            if (res != 0) {
                return res;
            }
        }
        return 0;
    }
    // 提取流媒体硬件技术参数
    long autoDurationMs = 0;
    int autoBitrate = 0;
    int autoSampleRate = 0;
    int autoChannels = 0;
    int autoBitDepth = 0;
    if (f.audioProperties()) {
        autoDurationMs = f.audioProperties()->lengthInMilliseconds();
        autoBitrate = f.audioProperties()->bitrate();
        autoSampleRate = f.audioProperties()->sampleRate();
        autoChannels = f.audioProperties()->channels();
    }
    if (ext == ".flac") {
        TagLib::FLAC::File ff(targetPath.c_str());
        if (ff.audioProperties()) {
            autoBitDepth = ff.audioProperties()->bitsPerSample();
        }
    }
    std::unordered_map<std::string, std::string> resolvedMetadata;
    std::unordered_map<std::string, ExtractedCoverData> resolvedCovers;
    // 加载全部插图槽位状态
    resolvedCovers["Front_Cover"] = getFilePictureStatus(targetPath, ext, "Front_Cover");
    resolvedCovers["Back_Cover"] = getFilePictureStatus(targetPath, ext, "Back_Cover");
    resolvedCovers["File_Icon"] = getFilePictureStatus(targetPath, ext, "File_Icon");
    resolvedCovers["During_Performance"] = getFilePictureStatus(targetPath, ext, "During_Performance");
    for (const auto& [key, cover] : resolvedCovers) {
        resolvedMetadata[key] = cover.isValid ? ("[" + cover.mime + " | " + cover.dimensions + " | " + cover.sizeStr + "]") : "";
    }
    // 复合提取容器底层全部标签映射表
    if (isNativeMp3) {
        TagLib::MPEG::File mf(targetPath.c_str());
        for (const auto& meta : METADATA_REGISTRY) {
            if (resolvedMetadata.find(meta.canonicalKey) != resolvedMetadata.end()) {
                continue;
            }
            bool isTargetLyrics = std::find(readFields.begin(), readFields.end(), "Lyrics") != readFields.end();
            resolvedMetadata[meta.canonicalKey] = extractId3v2Frame(mf, meta.id3v2FrameId, isTargetLyrics);
        }
    } else {
        for (const auto& meta : METADATA_REGISTRY) {
            if (resolvedMetadata.find(meta.canonicalKey) != resolvedMetadata.end()) {
                continue;
            }
            resolvedMetadata[meta.canonicalKey] = properties[meta.tagLibPropKey].toString().to8Bit(true);
            bool isExplicitLyricsRead = std::find(readFields.begin(), readFields.end(), "Lyrics") != readFields.end();
            if (meta.canonicalKey == "Lyrics" && !resolvedMetadata["Lyrics"].empty() && !isExplicitLyricsRead) {
                resolvedMetadata["Lyrics"] = "Unsynced Text (" + std::to_string(countLines(resolvedMetadata["Lyrics"])) + " lines)";
            }
        }
    }
    std::uintmax_t bytes = 0;
    try {
        bytes = fs::file_size(targetPath);
    } catch (...) {}
    completeTechnicalMetadata(resolvedMetadata, ext, autoDurationMs, autoBitrate, autoSampleRate, autoChannels, autoBitDepth, bytes);
    if (resolvedMetadata["Audio_MD5"].empty()) {
        resolvedMetadata["Audio_MD5"] = calculateAudioStreamMD5(targetPath, ext);
    }
    // 循环处理并批量展示特定字段读取请求
    if (!readFields.empty()) {
        for (const auto& targetField : readFields) {
            if (resolvedMetadata.find(targetField) == resolvedMetadata.end()) {
                std::cerr << "taglib-cli: " << targetField << ": Attribute key missing\n";
                return 1;
            }
            std::cout << resolvedMetadata[targetField] << "\n";
        }
        return 0;
    }
    // 默认全局打印行为
    for (const auto& meta : METADATA_REGISTRY) {
        std::cout << std::left << std::setw(30) << (meta.canonicalKey + ":") << resolvedMetadata[meta.canonicalKey] << "\n";
    }
    return 0;
}
