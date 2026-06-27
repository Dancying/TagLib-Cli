#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include <taglib/aifffile.h>
#include <taglib/flacfile.h>
#include <taglib/mpegfile.h>
#include <taglib/mp4file.h>
#include <taglib/oggfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>

#include "utils.h"

// completeTechnicalMetadata 补全和推导音频物理层的底层流技术指标参数
void completeTechnicalMetadata(std::unordered_map<std::string, std::string>& resolvedMetadata, const std::string& ext, long long autoDurationMs, int autoBitrate, int autoSampleRate, int autoChannels, int autoBitDepth, std::uintmax_t bytes) {
    // 填充音频基本时长信息
    if (resolvedMetadata["Duration"].empty() && autoDurationMs > 0) {
        resolvedMetadata["Duration"] = formatDuration(autoDurationMs);
    }
    // 填充音频流实时传输比特率
    if (resolvedMetadata["Bit_Rate"].empty() && autoBitrate > 0) {
        resolvedMetadata["Bit_Rate"] = std::to_string(autoBitrate) + " kbps";
    }
    // 填充音频采样率技术指标
    if (resolvedMetadata["Sample_Rate"].empty() && autoSampleRate > 0) {
        resolvedMetadata["Sample_Rate"] = std::to_string(autoSampleRate) + " Hz";
    }
    // 填充底层物理音频声道总数量
    if (resolvedMetadata["Channels"].empty() && autoChannels > 0) {
        resolvedMetadata["Channels"] = std::to_string(autoChannels);
    }
    // 填充声音信号量化位深指标
    if (resolvedMetadata["Bit_Depth"].empty() && autoBitDepth > 0) {
        resolvedMetadata["Bit_Depth"] = std::to_string(autoBitDepth) + "-bit";
    }
    // 映射并填充标准的声道布局别名
    if (resolvedMetadata["Channel_Layout"].empty() && autoChannels > 0) {
        resolvedMetadata["Channel_Layout"] = getChannelLayout(autoChannels);
    }
    // 基于已知封装及物理文件尺寸指标推理码率控制状态
    if (resolvedMetadata["Bit_Rate_Mode"].empty() && autoBitrate > 0 && autoDurationMs > 0) {
        if (ext == ".flac" || ext == ".alac" || ext == ".ape") {
            resolvedMetadata["Bit_Rate_Mode"] = "VBR";
        } else {
            double durationSec = static_cast<double>(autoDurationMs) / 1000.0;
            double estimatedFileBitrate = (static_cast<double>(bytes) * 8.0 / 1000.0) / durationSec;
            resolvedMetadata["Bit_Rate_Mode"] = (std::abs(estimatedFileBitrate - autoBitrate) / autoBitrate > 0.15) ? "VBR" : "CBR";
        }
    }
    // 剥离点符号转换填充音频编码格式
    if (resolvedMetadata["Audio_Codec"].empty() && !ext.empty()) {
        std::string codec = ext.substr(1);
        std::transform(codec.begin(), codec.end(), codec.begin(), ::toupper);
        resolvedMetadata["Audio_Codec"] = codec;
    }
    // 转换填充上层音视频容器封装格式
    if (resolvedMetadata["Container_Format"].empty() && !ext.empty()) {
        std::string container = ext.substr(1);
        std::transform(container.begin(), container.end(), container.begin(), ::toupper);
        resolvedMetadata["Container_Format"] = container;
    }
    // 换算并规整文件体积信息使其符合标准双重显示格式
    if (resolvedMetadata["File_Size"].empty() && bytes > 0) {
        double fileMb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        std::ostringstream ss;
        ss << bytes << " Bytes (" << std::fixed << std::setprecision(2) << fileMb << " MB)";
        resolvedMetadata["File_Size"] = ss.str();
    }
    // 推导计算原始 PCM 数据流的实时整体压缩比率
    if (resolvedMetadata["Compression_Ratio"].empty() && autoBitrate > 0 && autoSampleRate > 0 && autoChannels > 0) {
        int depth = (autoBitDepth > 0) ? autoBitDepth : 16;
        double uncompressedBitrate = static_cast<double>(autoSampleRate) * depth * autoChannels / 1000.0;
        if (uncompressedBitrate > 0) {
            double ratio = (static_cast<double>(autoBitrate) / uncompressedBitrate) * 100.0;
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2) << ratio << "%";
            resolvedMetadata["Compression_Ratio"] = ss.str();
        }
    }
    // 估算物理层纯音频轨数据的体积字节开销比例
    if (resolvedMetadata["Stream_Size"].empty() && autoBitrate > 0 && autoDurationMs > 0) {
        std::uintmax_t streamBytes = static_cast<std::uintmax_t>((static_cast<double>(autoDurationMs) / 1000.0) * (autoBitrate * 1000.0 / 8.0));
        double streamMb = static_cast<double>(streamBytes) / (1024.0 * 1024.0);
        std::ostringstream ss;
        ss << streamBytes << " Bytes (" << std::fixed << std::setprecision(2) << streamMb << " MB)";
        resolvedMetadata["Stream_Size"] = ss.str();
    }
}

// countLines 计算目标文本串中所包含的换行符号行数
size_t countLines(const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    size_t lines = std::count(str.begin(), str.end(), '\n');
    if (str.back() != '\n') {
        lines++;
    }
    return lines;
}

// formatByteSize 将字节数单位规整换算至适合阅读的 KB 或 MB
std::string formatByteSize(size_t bytes) {
    // 采用提前返回策略处理无需复杂转换的极小基础字节流
    if (bytes < 1024) {
        return std::to_string(bytes) + " Bytes";
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (bytes < 1024 * 1024) {
        ss << (bytes / 1024.0) << " KB";
    } else {
        ss << (bytes / (1024.0 * 1024.0)) << " MB";
    }
    return ss.str();
}

// formatDuration 将毫秒级时间转换格式化为时分秒毫秒的文本串
std::string formatDuration(long long milliseconds) {
    if (milliseconds <= 0) {
        return "00:00:00.000";
    }
    long long ms = milliseconds % 1000;
    long long seconds = (milliseconds / 1000) % 60;
    long long minutes = (milliseconds / (1000 * 60)) % 60;
    long long hours = (milliseconds / (1000 * 60 * 60));
    std::ostringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes << ":"
       << std::setfill('0') << std::setw(2) << seconds << "."
       << std::setfill('0') << std::setw(3) << ms;
    return ss.str();
}

// getChannelLayout 根据音频声道数映射标准布局别名
std::string getChannelLayout(int channels) {
    if (channels == 1) {
        return "Mono";
    }
    if (channels == 2) {
        return "Stereo";
    }
    if (channels == 6) {
        return "5.1 Surround";
    }
    if (channels == 8) {
        return "7.1 Surround";
    }
    return "Unknown (" + std::to_string(channels) + " CH)";
}

// getMimeTypeFromExt 根据标准扩展名查询匹配的图像 Mime 类型
std::string getMimeTypeFromExt(const std::string& s) {
    if (s == ".jpg" || s == ".jpeg") {
        return "image/jpeg";
    }
    if (s == ".png") {
        return "image/png";
    }
    if (s == ".gif") {
        return "image/gif";
    }
    if (s == ".bmp") {
        return "image/bmp";
    }
    if (s == ".webp") {
        return "image/webp";
    }
    return "";
}

// isCoverField 检查目标字段是否属于图像元数据类型
bool isCoverField(const std::string& field) {
    static const std::unordered_map<std::string, bool> coverMap = {
        {"Front_Cover", true},
        {"Back_Cover", true},
        {"File_Icon", true},
        {"During_Performance", true}
    };
    auto it = coverMap.find(field);
    return (it != coverMap.end()) ? it->second : false;
}

// parseImageDimensions 解析二进制图片流的头部标志符提取宽高分辨率
std::string parseImageDimensions(const char* data, size_t length) {
    // 检查并识别 PNG 格式的图像魔术头部字节
    if (length > 8 && (unsigned char)data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
        if (length >= 24) {
            uint32_t width = ((unsigned char)data[16] << 24) | ((unsigned char)data[17] << 16) | ((unsigned char)data[18] << 8) | (unsigned char)data[19];
            uint32_t height = ((unsigned char)data[20] << 24) | ((unsigned char)data[21] << 16) | ((unsigned char)data[22] << 8) | (unsigned char)data[23];
            return std::to_string(width) + "x" + std::to_string(height);
        }
    }
    // 检查并识别 JPEG 格式的图像魔术头部标志
    if (length > 4 && (unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xD8) {
        size_t i = 2;
        while (i + 8 < length) {
            if ((unsigned char)data[i] == 0xFF) {
                unsigned char marker = data[i + 1];
                // 定位并读取 JPEG 核心标志帧提取数据
                if (marker >= 0xC0 && marker <= 0xC3) {
                    uint16_t height = ((unsigned char)data[i + 5] << 8) | (unsigned char)data[i + 6];
                    uint16_t width = ((unsigned char)data[i + 7] << 8) | (unsigned char)data[i + 8];
                    return std::to_string(width) + "x" + std::to_string(height);
                } else {
                    uint16_t chunkLength = ((unsigned char)data[i + 2] << 8) | (unsigned char)data[i + 3];
                    i += 2 + chunkLength;
                }
            } else {
                i++;
            }
        }
    }
    return "Unknown x Unknown";
}

// parseKeyValue 解析带有等号的键值对参数
std::pair<std::string, std::string> parseKeyValue(const std::string& arg) {
    size_t pos = arg.find('=');
    if (pos == std::string::npos) {
        return {"", ""};
    }
    return {arg.substr(0, pos), arg.substr(pos + 1)};
}

// readFileToString 读取指定路径文件的文本内容并转换为字符串
std::string readFileToString(const std::string& filePath) {
    std::ifstream fileStream(filePath, std::ios::in | std::ios::binary);
    if (!fileStream.is_open()) {
        return "";
    }
    std::ostringstream outputBuffer;
    outputBuffer << fileStream.rdbuf();
    return outputBuffer.str();
}
