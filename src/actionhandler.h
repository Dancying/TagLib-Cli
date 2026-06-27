#ifndef ACTIONHANDLER_H
#define ACTIONHANDLER_H

#include <filesystem>
#include <string>
#include <vector>

#include <taglib/fileref.h>
#include <taglib/tpropertymap.h>

namespace fs = std::filesystem;

int executeWritePair(const std::string& pairStr, const fs::path& targetPath, const std::string& filename, const std::string& ext, bool isNativeMp3, TagLib::PropertyMap& properties, TagLib::FileRef& f);
int handleDeleteAction(const std::vector<std::string>& deleteFields, const fs::path& targetPath, const std::string& filename, const std::string& ext, bool isNativeMp3, TagLib::PropertyMap& properties, TagLib::FileRef& f);
int handleExtractAction(const std::string& coverRawExtract, const fs::path& targetPath, const std::string& filename, const std::string& ext);
int handleInjectAction(const std::string& coverRawInject, const fs::path& targetPath, const std::string& filename, const std::string& ext);
int handleWriteAction(const std::vector<std::string>& writePairs, const fs::path& targetPath, const std::string& filename, const std::string& ext, bool isNativeMp3, TagLib::PropertyMap& properties, TagLib::FileRef& f);

#endif
