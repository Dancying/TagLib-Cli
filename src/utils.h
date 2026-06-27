#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <unordered_map>
#include <utility>

void completeTechnicalMetadata(std::unordered_map<std::string, std::string>& resolvedMetadata, const std::string& ext, long long autoDurationMs, int autoBitrate, int autoSampleRate, int autoChannels, int autoBitDepth, std::uintmax_t bytes);
size_t countLines(const std::string& str);
std::string formatByteSize(size_t bytes);
std::string formatDuration(long long milliseconds);
std::string getChannelLayout(int channels);
std::string getMimeTypeFromExt(const std::string& ext);
bool isCoverField(const std::string& field);
std::string parseImageDimensions(const char* data, size_t length);
std::pair<std::string, std::string> parseKeyValue(const std::string& arg);
std::string readFileToString(const std::string& filePath);

#endif
