#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>

#include <taglib/aifffile.h>
#include <taglib/commentsframe.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v1tag.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpegfile.h>
#include <taglib/oggfile.h>
#include <taglib/synchronizedlyricsframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/tstring.h>
#include <taglib/unsynchronizedlyricsframe.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/xiphcomment.h>

#include "tagmanager.h"
#include "utils.h"

#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION

#include "deps/dr_flac.h"
#include "deps/dr_mp3.h"
#include "deps/dr_wav.h"
#include "deps/md5.hpp"
#include "deps/stb_vorbis.h"

// calculateAudioStreamMD5 计算指定音频文件裸数据流的 MD5 散列值
std::string calculateAudioStreamMD5(const std::filesystem::path& filePath, const std::string& ext) {
    long long startOffset = 0;
    long long dataLength = -1;
    if (ext == ".flac") {
        drflac* pFlac = drflac_open_file(filePath.string().c_str(), nullptr);
        if (!pFlac) {
            return "";
        }
        websocketpp::md5::md5_state_t mdCtx;
        websocketpp::md5::md5_init(&mdCtx);
        const size_t sampleBufferSize = 4096;
        if (pFlac->bitsPerSample <= 16) {
            std::vector<drflac_int16> sampleBuffer(sampleBufferSize);
            while (true) {
                drflac_uint64 framesRead = drflac_read_pcm_frames_s16(pFlac, sampleBufferSize / pFlac->channels, sampleBuffer.data());
                if (framesRead == 0) {
                    break;
                }
                size_t bytesToDigest = framesRead * pFlac->channels * sizeof(drflac_int16);
                websocketpp::md5::md5_append(&mdCtx, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(sampleBuffer.data()), bytesToDigest);
            }
        } else {
            std::vector<drflac_int32> sampleBuffer(sampleBufferSize);
            while (true) {
                drflac_uint64 framesRead = drflac_read_pcm_frames_s32(pFlac, sampleBufferSize / pFlac->channels, sampleBuffer.data());
                if (framesRead == 0) {
                    break;
                }
                size_t bytesToDigest = framesRead * pFlac->channels * sizeof(drflac_int32);
                websocketpp::md5::md5_append(&mdCtx, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(sampleBuffer.data()), bytesToDigest);
            }
        }
        drflac_close(pFlac);
        websocketpp::md5::md5_byte_t digest[16];
        websocketpp::md5::md5_finish(&mdCtx, digest);
        std::ostringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return ss.str();
    }
    if (ext == ".ogg" || ext == ".oga" || ext == ".vorbis" || ext == ".opus" || ext == ".spx") {
        websocketpp::md5::md5_state_t mdctx;
        websocketpp::md5::md5_init(&mdctx);
        bool processed = false;
        if (ext == ".ogg" || ext == ".oga" || ext == ".vorbis") {
            int error = 0;
            stb_vorbis* v = stb_vorbis_open_filename(filePath.string().c_str(), &error, nullptr);
            if (v) {
                stb_vorbis_info info = stb_vorbis_get_info(v);
                if (info.channels > 0) {
                    const size_t frameBufferSize = 4096;
                    std::vector<short> pcmBuffer(frameBufferSize * info.channels);
                    bool readSuccess = true;
                    while (true) {
                        int samplesRead = stb_vorbis_get_samples_short_interleaved(v, info.channels, pcmBuffer.data(), static_cast<int>(pcmBuffer.size()));
                        if (samplesRead < 0) {
                            readSuccess = false;
                            break;
                        }
                        if (samplesRead == 0) {
                            break;
                        }
                        size_t bytesToDigest = samplesRead * info.channels * sizeof(short);
                        websocketpp::md5::md5_append(&mdctx, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(pcmBuffer.data()), bytesToDigest);
                    }
                    stb_vorbis_close(v);
                    if (readSuccess) {
                        processed = true;
                    } else {
                        websocketpp::md5::md5_init(&mdctx);
                    }
                } else {
                    stb_vorbis_close(v);
                }
            }
        }
        if (!processed && (ext == ".ogg" || ext == ".opus" || ext == ".spx" || ext == ".oga")) {
            std::ifstream fileStream(filePath, std::ios::binary);
            if (fileStream.is_open()) {
                char pageHeader[27];
                while (fileStream.read(pageHeader, 27)) {
                    if (std::memcmp(pageHeader, "OggS", 4) != 0) {
                        break;
                    }
                    size_t segmentCount = static_cast<unsigned char>(pageHeader[26]);
                    std::vector<char> segmentSizes(segmentCount);
                    if (!fileStream.read(segmentSizes.data(), segmentCount)) {
                        break;
                    }
                    std::vector<size_t> packetSizes;
                    size_t currentPacketSize = 0;
                    for (size_t i = 0; i < segmentCount; ++i) {
                        unsigned char lacingVal = static_cast<unsigned char>(segmentSizes[i]);
                        currentPacketSize += lacingVal;
                        if (lacingVal < 255) {
                            packetSizes.push_back(currentPacketSize);
                            currentPacketSize = 0;
                        }
                    }
                    if (currentPacketSize > 0) {
                        packetSizes.push_back(currentPacketSize);
                    }
                    bool skipPage = false;
                    for (size_t pSize : packetSizes) {
                        std::vector<char> packetData(pSize);
                        if (!fileStream.read(packetData.data(), pSize)) {
                            skipPage = true;
                            break;
                        }
                        bool isAudioPacket = false;
                        if (ext == ".opus") {
                            isAudioPacket = !(pSize >= 8 && (std::memcmp(packetData.data(), "OpusHead", 8) == 0 || std::memcmp(packetData.data(), "OpusTags", 8) == 0));
                        } else if (ext == ".spx") {
                            if (pSize >= 8 && std::memcmp(packetData.data(), "Speex   ", 8) == 0) {
                                isAudioPacket = false;
                            } else if (pSize >= 4 && std::memcmp(packetData.data(), "OggS", 4) == 0) {
                                isAudioPacket = false;
                            } else if (pSize > 8 && std::memcmp(packetData.data() + 1, "vorbis", 6) == 0) {
                                isAudioPacket = false;
                            } else if (pSize > 7 && std::memcmp(packetData.data(), "OpusHead", 8) == 0) {
                                isAudioPacket = false;
                            } else if (pSize > 7 && std::memcmp(packetData.data(), "OpusTags", 8) == 0) {
                                isAudioPacket = false;
                            } else {
                                isAudioPacket = !(pSize > 0 && static_cast<unsigned char>(packetData[0]) == 0xFF);
                            }
                        } else {
                            if (pSize > 0) {
                                unsigned char firstByte = static_cast<unsigned char>(packetData[0]);
                                if (firstByte == 0x01 || firstByte == 0x03 || firstByte == 0x05 || firstByte == 'O' || firstByte == 'K' || firstByte == 'f') {
                                    isAudioPacket = false;
                                } else {
                                    isAudioPacket = ((firstByte & 0x80) == 0);
                                }
                            }
                        }
                        if (isAudioPacket && pSize > 0) {
                            websocketpp::md5::md5_append(&mdctx, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(packetData.data()), pSize);
                        }
                    }
                    if (skipPage) {
                        break;
                    }
                }
                processed = true;
            }
        }
        if (!processed) {
            return "";
        }
        websocketpp::md5::md5_byte_t digest[16];
        websocketpp::md5::md5_finish(&mdctx, digest);
        std::ostringstream ss;
        for (int j = 0; j < 16; ++j) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[j]);
        }
        return ss.str();
    }
    if (ext == ".mp3") {
        TagLib::MPEG::File mpegFile(filePath.c_str());
        if (!mpegFile.isValid()) {
            return "";
        }
        long long audioStartOffset = 0;
        if (mpegFile.ID3v2Tag()) {
            audioStartOffset = static_cast<long long>(mpegFile.nextFrameOffset(0));
        }
        std::string pathStr = filePath.string();
        std::FILE* pFile = std::fopen(pathStr.c_str(), "rb");
        if (!pFile) {
            return "";
        }
        std::fseek(pFile, 0, SEEK_END);
        long long fileSize = std::ftell(pFile);
        long long audioDataSize = fileSize - audioStartOffset;
        if (audioDataSize <= 0) {
            std::fclose(pFile);
            return "";
        }
        std::fseek(pFile, audioStartOffset, SEEK_SET);
        std::vector<unsigned char> audioBuffer(audioDataSize);
        if (std::fread(audioBuffer.data(), 1, audioDataSize, pFile) != static_cast<size_t>(audioDataSize)) {
            std::fclose(pFile);
            return "";
        }
        std::fclose(pFile);
        drmp3 mp3;
        if (!drmp3_init_memory(&mp3, audioBuffer.data(), audioBuffer.size(), nullptr)) {
            return "";
        }
        websocketpp::md5::md5_state_t mdCtx;
        websocketpp::md5::md5_init(&mdCtx);
        const size_t pcmBufferSize = 4096;
        std::vector<drmp3_int16> pcmBuffer(pcmBufferSize);
        while (true) {
            unsigned long long framesRead = drmp3_read_pcm_frames_s16(&mp3, pcmBufferSize / mp3.channels, pcmBuffer.data());
            if (framesRead == 0) {
                break;
            }
            size_t bytesToDigest = framesRead * mp3.channels * sizeof(drmp3_int16);
            websocketpp::md5::md5_append(&mdCtx, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(pcmBuffer.data()), bytesToDigest);
        }
        drmp3_uninit(&mp3);
        websocketpp::md5::md5_byte_t digest[16];
        websocketpp::md5::md5_finish(&mdCtx, digest);
        std::ostringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return ss.str();
    }
    if (ext == ".mp4" || ext == ".m4a" || ext == ".mka" || ext == ".webm") {
        TagLib::MP4::File mp4File(filePath.c_str());
        long long mdatOffset = mp4File.find("mdat");
        startOffset = (mdatOffset >= 0) ? (mdatOffset + 8) : (mp4File.find("data") >= 0 ? mp4File.find("data") + 8 : startOffset);
    }
    if (ext == ".wav") {
        drwav wav;
        if (!drwav_init_file(&wav, filePath.string().c_str(), nullptr)) {
            return "";
        }
        websocketpp::md5::md5_state_t mdCtx;
        websocketpp::md5::md5_init(&mdCtx);
        const size_t pcmBufferSize = 4096;
        if (wav.bitsPerSample <= 16) {
            std::vector<drwav_int16> pcmBuffer(pcmBufferSize);
            while (true) {
                unsigned long long framesRead = drwav_read_pcm_frames_s16(&wav, pcmBufferSize / wav.channels, pcmBuffer.data());
                if (framesRead == 0) {
                    break;
                }
                size_t bytesToDigest = framesRead * wav.channels * sizeof(drwav_int16);
                websocketpp::md5::md5_append(&mdCtx, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(pcmBuffer.data()), bytesToDigest);
            }
        } else {
            std::vector<drwav_int32> pcmBuffer(pcmBufferSize);
            while (true) {
                unsigned long long framesRead = drwav_read_pcm_frames_s32(&wav, pcmBufferSize / wav.channels, pcmBuffer.data());
                if (framesRead == 0) {
                    break;
                }
                size_t bytesToDigest = framesRead * wav.channels * sizeof(drwav_int32);
                websocketpp::md5::md5_append(&mdCtx, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(pcmBuffer.data()), bytesToDigest);
            }
        }
        drwav_uninit(&wav);
        websocketpp::md5::md5_byte_t digest[16];
        websocketpp::md5::md5_finish(&mdCtx, digest);
        std::ostringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return ss.str();
    }
    if (ext == ".aif" || ext == ".aiff") {
        std::ifstream f(filePath, std::ios::binary);
        if (!f.is_open()) {
            return "";
        }
        char magic[4];
        f.read(magic, 4);
        if (std::strncmp(magic, "FORM", 4) != 0) {
            return "";
        }
        char searchBuf[4] = {0};
        bool foundSsnd = false;
        f.seekg(12, std::ios::beg);
        while (f.read(&searchBuf[0], 1)) {
            if (searchBuf[0] == 'S') {
                long long currentPos = f.tellg();
                char nextThree[3];
                bool isMatch = f.read(nextThree, 3) && std::strncmp(nextThree, "SND", 3) == 0;
                foundSsnd = isMatch;
                if (isMatch) {
                    break;
                }
                f.seekg(currentPos, std::ios::beg);
            }
        }
        if (!foundSsnd) {
            return "";
        }
        unsigned char sizeBytes[4];
        f.read(reinterpret_cast<char*>(sizeBytes), 4);
        uint32_t chunkSize = (sizeBytes[0] << 24) | (sizeBytes[1] << 16) | (sizeBytes[2] << 8) | sizeBytes[3];
        unsigned char offsetBytes[4];
        unsigned char blockSizeBytes[4];
        f.read(reinterpret_cast<char*>(offsetBytes), 4);
        f.read(reinterpret_cast<char*>(blockSizeBytes), 4);
        uint32_t ssndOffset = (offsetBytes[0] << 24) | (offsetBytes[1] << 16) | (offsetBytes[2] << 8) | offsetBytes[3];
        startOffset = static_cast<long long>(f.tellg()) + ssndOffset;
        dataLength = chunkSize - 8 - ssndOffset;
        if (startOffset <= 0 || dataLength <= 0) {
            return "";
        }
        f.clear();
        f.seekg(startOffset, std::ios::beg);
        websocketpp::md5::md5_state_t mdCtxAif;
        websocketpp::md5::md5_init(&mdCtxAif);
        const size_t SAMPLES_PER_LOOP = 1365;
        std::vector<unsigned char> rawBuffer(SAMPLES_PER_LOOP * 3);
        std::vector<int32_t> pcm32Buffer(SAMPLES_PER_LOOP);
        long long bytesProcessed = 0;
        while (bytesProcessed < dataLength) {
            long long bytesToRead = std::min(static_cast<long long>(rawBuffer.size()), dataLength - bytesProcessed);
            if (bytesToRead <= 0) {
                break;
            }
            f.read(reinterpret_cast<char*>(rawBuffer.data()), bytesToRead);
            std::streamsize actualBytes = f.gcount();
            if (actualBytes <= 0) {
                break;
            }
            size_t samplesRead = actualBytes / 3;
            for (size_t i = 0; i < samplesRead; ++i) {
                size_t idx = i * 3;
                int32_t sample32 = (rawBuffer[idx] << 24) |
                                   (rawBuffer[idx + 1] << 16) |
                                   (rawBuffer[idx + 2] << 8);
                pcm32Buffer[i] = sample32;
            }
            size_t bytesToDigest = samplesRead * sizeof(int32_t);
            if (bytesToDigest > 0) {
                websocketpp::md5::md5_append(&mdCtxAif, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(pcm32Buffer.data()), bytesToDigest);
            }
            bytesProcessed += actualBytes;
        }
        websocketpp::md5::md5_byte_t digest[16];
        websocketpp::md5::md5_finish(&mdCtxAif, digest);
        std::ostringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        }
        return ss.str();
    }
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    file.seekg(0, std::ios::end);
    long long fileSize = file.tellg();
    dataLength = (dataLength < 0) ? (fileSize - startOffset) : dataLength;
    if (ext == ".mp3" && fileSize > 128) {
        file.seekg(fileSize - 128);
        char tagHeader[3];
        file.read(tagHeader, 3);
        if (std::strncmp(tagHeader, "TAG", 3) == 0) {
            dataLength -= 128;
        }
    }
    if (startOffset < 0 || startOffset >= fileSize || dataLength <= 0) {
        return "";
    }
    file.seekg(startOffset, std::ios::beg);
    websocketpp::md5::md5_state_t mdCtxGlobal;
    websocketpp::md5::md5_init(&mdCtxGlobal);
    char buffer[4096];
    long long bytesReadTotal = 0;
    while (bytesReadTotal < dataLength) {
        long long bytesToRead = std::min(static_cast<long long>(sizeof(buffer)), dataLength - bytesReadTotal);
        if (!file.read(buffer, bytesToRead) && file.gcount() <= 0) {
            break;
        }
        websocketpp::md5::md5_append(&mdCtxGlobal, reinterpret_cast<const websocketpp::md5::md5_byte_t*>(buffer), file.gcount());
        bytesReadTotal += file.gcount();
    }
    websocketpp::md5::md5_byte_t finalDigest[16];
    websocketpp::md5::md5_finish(&mdCtxGlobal, finalDigest);
    std::ostringstream ss;
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(finalDigest[i]);
    }
    return ss.str();
}

// cleanFramesFromApic 流式清洗匹配特定类型的图像帧
static void cleanFramesFromApic(TagLib::ID3v2::Tag* tag, const TagLib::ID3v2::AttachedPictureFrame::Type& targetType, const std::string& picTypeStr) {
    auto frames = tag->frameListMap()["APIC"];
    for (auto* f : frames) {
        auto* frame = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(f);
        if (frame->type() == targetType || (picTypeStr == "Front_Cover" && frame->type() == TagLib::ID3v2::AttachedPictureFrame::Other)) {
            tag->removeFrame(frame);
        }
    }
}

// fillCoverFromApic 从附属图片帧中提取图像基础元数据
static ExtractedCoverData fillCoverFromApic(const TagLib::ID3v2::AttachedPictureFrame* frame) {
    ExtractedCoverData outData;
    outData.mime = frame->mimeType().to8Bit(true);
    outData.rawBinary = std::string(frame->picture().data(), frame->picture().size());
    outData.dimensions = parseImageDimensions(outData.rawBinary.data(), outData.rawBinary.size());
    outData.sizeStr = formatByteSize(outData.rawBinary.size());
    outData.isValid = true;
    return outData;
}

// fillCoverFromFlacPic 从 FLAC 块中提取图像基础元数据
static ExtractedCoverData fillCoverFromFlacPic(const TagLib::FLAC::Picture* pic) {
    ExtractedCoverData outData;
    outData.mime = pic->mimeType().to8Bit(true);
    outData.rawBinary = std::string(pic->data().data(), pic->data().size());
    outData.dimensions = parseImageDimensions(outData.rawBinary.data(), outData.rawBinary.size());
    outData.sizeStr = formatByteSize(outData.rawBinary.size());
    outData.isValid = true;
    return outData;
}

// findApicMatchFrame 在关联列表中过滤查找特定类别的图像帧指针
static ExtractedCoverData findApicMatchFrame(const TagLib::ID3v2::FrameList& list, const TagLib::ID3v2::AttachedPictureFrame::Type& targetType, const std::string& picTypeStr) {
    ExtractedCoverData outData;
    for (auto* f : list) {
        auto* frame = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(f);
        if (frame->type() == targetType) {
            return fillCoverFromApic(frame);
        }
    }
    if (picTypeStr == "Front_Cover") {
        for (auto* f : list) {
            auto* frame = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(f);
            if (frame->type() == TagLib::ID3v2::AttachedPictureFrame::Other) {
                return fillCoverFromApic(frame);
            }
        }
    }
    return outData;
}

// parseDimsToFlacPic 转换分辨率文本并填充至图像信息块
static void parseDimsToFlacPic(TagLib::FLAC::Picture* pic, const std::string& dims, const std::vector<char>& imgData) {
    size_t xPos = dims.find('x');
    if (xPos != std::string::npos) {
        try {
            pic->setWidth(std::stoi(dims.substr(0, xPos)));
            pic->setHeight(std::stoi(dims.substr(xPos + 1)));
        } catch (...) {
            pic->setWidth(0);
            pic->setHeight(0);
        }
    }
    pic->setColorDepth(24);
}

// extractId3v2Frame 从 MPEG 文件的 ID3v2 标签中提取特定标识帧的文本内容
std::string extractId3v2Frame(TagLib::MPEG::File& mf, const std::string& frameId, bool rawLyrics) {
    TagLib::ID3v2::Tag* tag = mf.ID3v2Tag();
    if (!tag) {
        return "";
    }
    if (frameId == "USLT") {
        if (tag->frameListMap().contains("SYLT") && !tag->frameListMap()["SYLT"].isEmpty()) {
            auto* sylt = static_cast<TagLib::ID3v2::SynchronizedLyricsFrame*>(tag->frameListMap()["SYLT"].front());
            size_t lines = 0;
            std::string outText = "";
            TagLib::ID3v2::SynchronizedLyricsFrame::SynchedTextList list = sylt->synchedText();
            for (auto it = list.begin(); it != list.end(); ++it) {
                outText += it->text.to8Bit(true) + "\n";
                lines++;
            }
            return rawLyrics ? outText : "Synced LRC (" + std::to_string(lines) + " lines)";
        }
        if (tag->frameListMap().contains("USLT") && !tag->frameListMap()["USLT"].isEmpty()) {
            auto* uslt = static_cast<TagLib::ID3v2::UnsynchronizedLyricsFrame*>(tag->frameListMap()["USLT"].front());
            std::string text = uslt->text().to8Bit(true);
            return rawLyrics ? text : "Unsynced Text (" + std::to_string(countLines(text)) + " lines)";
        }
        return "";
    }
    if (frameId.rfind("TXXXX:", 0) == 0) {
        std::string description = frameId.substr(6);
        TagLib::ByteVector id("TXXX");
        if (tag->frameListMap().contains(id)) {
            for (auto* f : tag->frameListMap()[id]) {
                auto* txxx = static_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(f);
                if (txxx && txxx->description().to8Bit(true) == description) {
                    return txxx->fieldList().back().to8Bit(true);
                }
            }
        }
        return "";
    }
    TagLib::ByteVector id(frameId.c_str(), frameId.size());
    if (tag->frameListMap().contains(id) && !tag->frameListMap()[id].isEmpty()) {
        if (frameId == "COMM") {
            return static_cast<TagLib::ID3v2::CommentsFrame*>(tag->frameListMap()[id].front())->text().to8Bit(true);
        }
        return tag->frameListMap()[id].front()->toString().to8Bit(true);
    }
    return "";
}

// getFilePictureStatus 读取并提取音频容器中封装的图像元数据块
ExtractedCoverData getFilePictureStatus(const fs::path& targetPath, const std::string& ext, const std::string& picTypeStr) {
    ExtractedCoverData outData;
    if (ext == ".flac") {
        TagLib::FLAC::File ff(targetPath.c_str());
        auto targetType = parseFlacPictureType(picTypeStr);
        if (!ff.pictureList().isEmpty()) {
            for (auto* pic : ff.pictureList()) {
                if (pic && pic->type() == targetType) {
                    return fillCoverFromFlacPic(pic);
                }
            }
            if (picTypeStr == "Front_Cover") {
                for (auto* pic : ff.pictureList()) {
                    if (pic && pic->type() == TagLib::FLAC::Picture::Other) {
                        return fillCoverFromFlacPic(pic);
                    }
                }
            }
        }
        auto* id3v2Tag = ff.ID3v2Tag();
        if (id3v2Tag && !id3v2Tag->frameListMap()["APIC"].isEmpty()) {
            return findApicMatchFrame(id3v2Tag->frameListMap()["APIC"], parsePictureType(picTypeStr), picTypeStr);
        }
        return outData;
    }
    if (ext == ".ogg" || ext == ".opus" || ext == ".spx" || ext == ".oga") {
        TagLib::Ogg::Vorbis::File ovf(targetPath.c_str());
        if (ovf.isValid() && ovf.tag()) {
            auto targetType = parseFlacPictureType(picTypeStr);
            const auto& pictures = ovf.tag()->pictureList();
            for (auto* pic : pictures) {
                if (pic && pic->type() == targetType) {
                    return fillCoverFromFlacPic(pic);
                }
            }
            if (picTypeStr == "Front_Cover") {
                for (auto* pic : pictures) {
                    if (pic && pic->type() == TagLib::FLAC::Picture::Other) {
                        return fillCoverFromFlacPic(pic);
                    }
                }
            }
        }
        return outData;
    }
    if (ext == ".mp3") {
        TagLib::MPEG::File mf(targetPath.c_str());
        TagLib::ID3v2::Tag* tag = mf.ID3v2Tag();
        if (!tag || tag->frameListMap()["APIC"].isEmpty()) {
            return outData;
        }
        return findApicMatchFrame(tag->frameListMap()["APIC"], parsePictureType(picTypeStr), picTypeStr);
    }
    if (ext == ".mp4" || ext == ".m4a" || ext == ".mka" || ext == ".webm") {
        if (picTypeStr != "Front_Cover") {
            return outData;
        }
        TagLib::MP4::File mp4f(targetPath.c_str());
        TagLib::MP4::Tag* tag = mp4f.tag();
        if (tag && tag->itemMap().contains("covr")) {
            auto artList = tag->itemMap()["covr"].toCoverArtList();
            if (!artList.isEmpty()) {
                auto art = artList.front();
                outData.mime = (art.format() == TagLib::MP4::CoverArt::PNG) ? "image/png" : "image/jpeg";
                outData.rawBinary = std::string(art.data().data(), art.data().size());
                outData.dimensions = parseImageDimensions(outData.rawBinary.data(), outData.rawBinary.size());
                outData.sizeStr = formatByteSize(outData.rawBinary.size());
                outData.isValid = true;
                return outData;
            }
        }
        return outData;
    }
    if (ext == ".wav") {
        TagLib::RIFF::WAV::File wf(targetPath.c_str());
        if (wf.isValid() && wf.ID3v2Tag() && !wf.ID3v2Tag()->frameListMap()["APIC"].isEmpty()) {
            return findApicMatchFrame(wf.ID3v2Tag()->frameListMap()["APIC"], parsePictureType(picTypeStr), picTypeStr);
        }
        return outData;
    }
    if (ext == ".aif" || ext == ".aiff") {
        TagLib::RIFF::AIFF::File af(targetPath.c_str());
        if (af.isValid() && af.tag() && !af.tag()->frameListMap()["APIC"].isEmpty()) {
            return findApicMatchFrame(af.tag()->frameListMap()["APIC"], parsePictureType(picTypeStr), picTypeStr);
        }
        return outData;
    }
    return outData;
}

// getMp4CoverArtFormat 根据图像文件后缀名映射 MP4 容器对应的专有格式枚举
TagLib::MP4::CoverArt::Format getMp4CoverArtFormat(const std::string& ext) {
    return (ext == ".png") ? TagLib::MP4::CoverArt::PNG : TagLib::MP4::CoverArt::JPEG;
}

// injectId3v2Frame 向 MPEG 文件的 ID3v2 标签中注入或覆盖特定标识帧的文本内容
void injectId3v2Frame(TagLib::MPEG::File& mf, const std::string& frameId, const std::string& value) {
    TagLib::ID3v2::Tag* tag = mf.ID3v2Tag(true);
    if (!tag) {
        return;
    }
    if (frameId.rfind("TXXXX:", 0) == 0) {
        std::string description = frameId.substr(6);
        TagLib::ByteVector id("TXXX");
        if (tag->frameListMap().contains(id)) {
            auto frames = tag->frameListMap()[id];
            for (auto* f : frames) {
                auto* txxx = static_cast<TagLib::ID3v2::UserTextIdentificationFrame*>(f);
                if (txxx && txxx->description().to8Bit(true) == description) {
                    tag->removeFrame(txxx);
                }
            }
        }
        if (!value.empty()) {
            auto* frame = new TagLib::ID3v2::UserTextIdentificationFrame(TagLib::String::UTF8);
            frame->setDescription(TagLib::String(description, TagLib::String::UTF8));
            frame->setText(TagLib::String(value, TagLib::String::UTF8));
            tag->addFrame(frame);
        }
        return;
    }
    TagLib::ByteVector id(frameId.c_str(), frameId.size());
    tag->removeFrames(id);
    if (value.empty()) {
        return;
    }
    if (frameId == "COMM") {
        auto* frame = new TagLib::ID3v2::CommentsFrame(TagLib::String::UTF8);
        frame->setText(TagLib::String(value, TagLib::String::UTF8));
        tag->addFrame(frame);
    } else if (frameId == "USLT") {
        auto* frame = new TagLib::ID3v2::UnsynchronizedLyricsFrame(TagLib::String::UTF8);
        frame->setText(TagLib::String(value, TagLib::String::UTF8));
        tag->addFrame(frame);
    } else {
        auto* frame = new TagLib::ID3v2::TextIdentificationFrame(id, TagLib::String::UTF8);
        frame->setText(TagLib::String(value, TagLib::String::UTF8));
        tag->addFrame(frame);
    }
}

// parseFlacPictureType 将字符串映射为 TagLib 原生支持的 FLAC 图片类别枚举
TagLib::FLAC::Picture::Type parseFlacPictureType(const std::string& type) {
    if (type == "Back_Cover") {
        return TagLib::FLAC::Picture::BackCover;
    }
    if (type == "File_Icon") {
        return TagLib::FLAC::Picture::FileIcon;
    }
    if (type == "During_Performance") {
        return TagLib::FLAC::Picture::DuringPerformance;
    }
    return TagLib::FLAC::Picture::FrontCover;
}

// parsePictureType 将字符串映射为 TagLib 原生支持 of ID3v2 图像类别枚举
TagLib::ID3v2::AttachedPictureFrame::Type parsePictureType(const std::string& type) {
    if (type == "Back_Cover") {
        return TagLib::ID3v2::AttachedPictureFrame::BackCover;
    }
    if (type == "File_Icon") {
        return TagLib::ID3v2::AttachedPictureFrame::FileIcon;
    }
    if (type == "During_Performance") {
        return TagLib::ID3v2::AttachedPictureFrame::DuringPerformance;
    }
    return TagLib::ID3v2::AttachedPictureFrame::FrontCover;
}

// removeCoverArt 从指定的音频容器文件中安全清除单张或者全部附带的图片流
void removeCoverArt(const fs::path& targetPath, const std::string& ext, const std::string& picTypeStr, bool removeAll) {
    if (ext == ".flac") {
        TagLib::FLAC::File ff(targetPath.c_str());
        if (removeAll) {
            ff.removePictures();
            auto* id3v2Tag = ff.ID3v2Tag();
            if (id3v2Tag) {
                id3v2Tag->removeFrames("APIC");
            }
        } else {
            auto targetType = parseFlacPictureType(picTypeStr);
            const auto& pictures = ff.pictureList();
            std::vector<TagLib::FLAC::Picture*> toRemove;
            for (auto* pic : pictures) {
                if (pic && (pic->type() == targetType || (picTypeStr == "Front_Cover" && pic->type() == TagLib::FLAC::Picture::Other))) {
                    toRemove.push_back(pic);
                }
            }
            for (auto* pic : toRemove) {
                ff.removePicture(pic);
            }
            auto* id3v2Tag = ff.ID3v2Tag();
            if (id3v2Tag) {
                cleanFramesFromApic(id3v2Tag, parsePictureType(picTypeStr), picTypeStr);
            }
        }
        ff.save();
        return;
    }
    if (ext == ".ogg" || ext == ".opus" || ext == ".spx" || ext == ".oga") {
        TagLib::Ogg::Vorbis::File ovf(targetPath.c_str());
        if (!ovf.isValid() || !ovf.tag()) {
            return;
        }
        if (removeAll) {
            ovf.tag()->removeAllPictures();
            ovf.tag()->removeFields("METADATA_BLOCK_PICTURE");
            ovf.tag()->removeFields("COVERART");
        } else {
            auto targetType = parseFlacPictureType(picTypeStr);
            const auto& pictures = ovf.tag()->pictureList();
            std::vector<TagLib::FLAC::Picture*> remainingPics;
            for (auto* p : pictures) {
                if (!p) {
                    continue;
                }
                if (p->type() == targetType || (picTypeStr == "Front_Cover" && p->type() == TagLib::FLAC::Picture::Other)) {
                    continue;
                }
                remainingPics.push_back(p);
            }
            ovf.tag()->removeAllPictures();
            for (auto* p : remainingPics) {
                auto* copyPic = new TagLib::FLAC::Picture();
                copyPic->setMimeType(p->mimeType());
                copyPic->setType(p->type());
                copyPic->setDescription(p->description());
                copyPic->setData(p->data());
                ovf.tag()->addPicture(copyPic);
            }
            if (picTypeStr == "Front_Cover") {
                ovf.tag()->removeFields("METADATA_BLOCK_PICTURE");
                ovf.tag()->removeFields("COVERART");
            }
        }
        ovf.save();
        return;
    }
    if (ext == ".mp3") {
        TagLib::MPEG::File mf(targetPath.c_str());
        TagLib::ID3v2::Tag* tag = mf.ID3v2Tag();
        if (!tag) {
            return;
        }
        if (removeAll) {
            tag->removeFrames("APIC");
        } else {
            cleanFramesFromApic(tag, parsePictureType(picTypeStr), picTypeStr);
        }
        mf.save();
        return;
    }
    if (ext == ".mp4" || ext == ".m4a" || ext == ".mka" || ext == ".webm") {
        if (picTypeStr != "Front_Cover") {
            return;
        }
        TagLib::MP4::File mp4f(targetPath.c_str());
        TagLib::MP4::Tag* tag = mp4f.tag();
        if (tag && tag->itemMap().contains("covr")) {
            tag->removeItem("covr");
            mp4f.save();
        }
        return;
    }
    if (ext == ".wav") {
        TagLib::RIFF::WAV::File wf(targetPath.c_str());
        if (wf.isValid() && wf.ID3v2Tag()) {
            if (removeAll) {
                wf.ID3v2Tag()->removeFrames("APIC");
            } else {
                cleanFramesFromApic(wf.ID3v2Tag(), parsePictureType(picTypeStr), picTypeStr);
            }
            wf.save();
        }
        return;
    }
    if (ext == ".aif" || ext == ".aiff") {
        TagLib::RIFF::AIFF::File af(targetPath.c_str());
        if (af.isValid() && af.tag()) {
            if (removeAll) {
                af.tag()->removeFrames("APIC");
            } else {
                cleanFramesFromApic(af.tag(), parsePictureType(picTypeStr), picTypeStr);
            }
            af.save();
        }
        return;
    }
}

// removeId3v1Field 从 MPEG 文件的 ID3v1 标签中完全擦除特定字段的数据
void removeId3v1Field(TagLib::MPEG::File& mf, const std::string& canonicalKey) {
    TagLib::ID3v1::Tag* v1Tag = mf.ID3v1Tag();
    if (!v1Tag) {
        return;
    }
    typedef void (TagLib::ID3v1::Tag::*V1StringSetter)(const TagLib::String&);
    static const std::unordered_map<std::string, V1StringSetter> stringFields = {
        {"Album", &TagLib::ID3v1::Tag::setAlbum},
        {"Artist", &TagLib::ID3v1::Tag::setArtist},
        {"Comment", &TagLib::ID3v1::Tag::setComment},
        {"Title", &TagLib::ID3v1::Tag::setTitle}
    };
    auto it = stringFields.find(canonicalKey);
    if (it != stringFields.end()) {
        (v1Tag->*(it->second))(TagLib::String());
        return;
    }
    if (canonicalKey == "Track_Number") {
        v1Tag->setTrack(0);
    } else if (canonicalKey == "Date") {
        v1Tag->setYear(0);
    }
}

// removeId3v2Frame 从 MPEG 文件的 ID3v2 标签中完全移除特定标识对应的文本数据帧
void removeId3v2Frame(TagLib::MPEG::File& mf, const std::string& frameId) {
    TagLib::ID3v2::Tag* tag = mf.ID3v2Tag();
    if (!tag) {
        return;
    }
    TagLib::ByteVector id(frameId.c_str(), frameId.size());
    tag->removeFrames(id);
}

// writeCoverArtDirect 读取外部图像资源并将其以二进制格式直接写入对应的音频标签块
void writeCoverArtDirect(const fs::path& targetPath, const std::string& ext, const std::string& picTypeStr, const std::string& imgPathStr) {
    fs::path imgPath(imgPathStr);
    if (!fs::exists(imgPath) || !fs::is_regular_file(imgPath)) {
        return;
    }
    std::string imgExt = imgPath.extension().string();
    std::transform(imgExt.begin(), imgExt.end(), imgExt.begin(), ::tolower);
    std::string mime = getMimeTypeFromExt(imgExt);
    if (mime.empty()) {
        return;
    }
    std::ifstream imgFile(imgPathStr, std::ios::binary);
    std::vector<char> imgData((std::istreambuf_iterator<char>(imgFile)), std::istreambuf_iterator<char>());
    TagLib::ByteVector bv(imgData.data(), imgData.size());
    if (ext == ".flac") {
        TagLib::FLAC::File ff(targetPath.c_str());
        if (!ff.isValid()) {
            return;
        }
        removeCoverArt(targetPath, ext, picTypeStr, false);
        auto targetType = parseFlacPictureType(picTypeStr);
        auto* pic = new TagLib::FLAC::Picture();
        pic->setMimeType(mime);
        pic->setType(targetType);
        pic->setDescription(picTypeStr);
        pic->setData(bv);
        parseDimsToFlacPic(pic, parseImageDimensions(imgData.data(), imgData.size()), imgData);
        ff.addPicture(pic);
        ff.save();
        return;
    }
    if (ext == ".ogg" || ext == ".opus" || ext == ".spx" || ext == ".oga") {
        TagLib::Ogg::Vorbis::File ovf(targetPath.c_str());
        if (ovf.isValid() && ovf.tag()) {
            auto targetType = parseFlacPictureType(picTypeStr);
            const auto& pictures = ovf.tag()->pictureList();
            std::vector<TagLib::FLAC::Picture*> toRemove;
            for (auto* p : pictures) {
                if (p && (p->type() == targetType || (picTypeStr == "Front_Cover" && p->type() == TagLib::FLAC::Picture::Other))) {
                    toRemove.push_back(p);
                }
            }
            for (auto* p : toRemove) {
                ovf.tag()->removePicture(p);
            }
            auto* pic = new TagLib::FLAC::Picture();
            pic->setMimeType(mime);
            pic->setType(targetType);
            pic->setDescription(picTypeStr);
            pic->setData(bv);
            parseDimsToFlacPic(pic, parseImageDimensions(imgData.data(), imgData.size()), imgData);
            ovf.tag()->addPicture(pic);
            ovf.save();
        }
        return;
    }
    if (ext == ".mp3") {
        TagLib::MPEG::File mf(targetPath.c_str());
        TagLib::ID3v2::Tag* tag = mf.ID3v2Tag(true);
        if (tag) {
            auto targetType = parsePictureType(picTypeStr);
            cleanFramesFromApic(tag, targetType, picTypeStr);
            auto* frame = new TagLib::ID3v2::AttachedPictureFrame();
            frame->setMimeType(mime);
            frame->setType(targetType);
            frame->setPicture(bv);
            tag->addFrame(frame);
            mf.save();
        }
        return;
    }
    if (ext == ".mp4" || ext == ".m4a" || ext == ".mka" || ext == ".webm") {
        if (picTypeStr != "Front_Cover") {
            return;
        }
        TagLib::MP4::File mp4f(targetPath.c_str());
        TagLib::MP4::Tag* tag = mp4f.tag();
        if (tag) {
            TagLib::MP4::CoverArt art(getMp4CoverArtFormat(imgExt), bv);
            TagLib::MP4::CoverArtList artList;
            artList.append(art);
            tag->setItem("covr", TagLib::MP4::Item(artList));
            mp4f.save();
        }
        return;
    }
    if (ext == ".wav") {
        TagLib::RIFF::WAV::File wf(targetPath.c_str());
        if (wf.isValid()) {
            TagLib::ID3v2::Tag* tag = wf.ID3v2Tag();
            if (tag) {
                auto targetType = parsePictureType(picTypeStr);
                cleanFramesFromApic(tag, targetType, picTypeStr);
                auto* frame = new TagLib::ID3v2::AttachedPictureFrame();
                frame->setMimeType(mime);
                frame->setType(targetType);
                frame->setPicture(bv);
                tag->addFrame(frame);
                wf.save();
            }
        }
        return;
    }
    if (ext == ".aif" || ext == ".aiff") {
        TagLib::RIFF::AIFF::File af(targetPath.c_str());
        if (af.isValid()) {
            TagLib::ID3v2::Tag* tag = af.tag();
            if (tag) {
                auto targetType = parsePictureType(picTypeStr);
                cleanFramesFromApic(tag, targetType, picTypeStr);
                auto* frame = new TagLib::ID3v2::AttachedPictureFrame();
                frame->setMimeType(mime);
                frame->setType(targetType);
                frame->setPicture(bv);
                tag->addFrame(frame);
                af.save();
            }
        }
        return;
    }
}
