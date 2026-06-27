#ifndef TAGMANAGER_H
#define TAGMANAGER_H

#include <filesystem>
#include <string>

#include <taglib/attachedpictureframe.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4coverart.h>
#include <taglib/mpegfile.h>

namespace fs = std::filesystem;

struct ExtractedCoverData {
    std::string mime;
    std::string dimensions;
    std::string sizeStr;
    std::string rawBinary;
    bool isValid = false;
};

std::string calculateAudioStreamMD5(const std::filesystem::path& filePath, const std::string& ext);
std::string extractId3v2Frame(TagLib::MPEG::File& mf, const std::string& frameId, bool rawLyrics = false);
ExtractedCoverData getFilePictureStatus(const fs::path& targetPath, const std::string& ext, const std::string& picTypeStr);
TagLib::MP4::CoverArt::Format getMp4CoverArtFormat(const std::string& ext);
void injectId3v2Frame(TagLib::MPEG::File& mf, const std::string& frameId, const std::string& value);
TagLib::FLAC::Picture::Type parseFlacPictureType(const std::string& type);
TagLib::ID3v2::AttachedPictureFrame::Type parsePictureType(const std::string& type);
void removeCoverArt(const fs::path& targetPath, const std::string& ext, const std::string& picTypeStr, bool removeAll);
void removeId3v1Field(TagLib::MPEG::File& mf, const std::string& canonicalKey);
void removeId3v2Frame(TagLib::MPEG::File& mf, const std::string& frameId);
void writeCoverArtDirect(const fs::path& targetPath, const std::string& ext, const std::string& picTypeStr, const std::string& imgPathStr);

#endif
