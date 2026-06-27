# Audio Test Files Archive

## 1. Source Master Info
* **File**: `music_FLAC_24BIT_48kHz_stereo.flac`
* **Details**: FLAC audio bitstream data, 24 bit, stereo, 48 kHz, 7950423 samples

## 2. FFmpeg Conversion Commands
```bash
# WAV (24bit PCM)
ffmpeg -i music_FLAC_24BIT_48kHz_stereo.flac -c:a pcm_s24le -vn music_PCM_24BIT_48kHz_stereo.wav

# AIFF (24bit PCM)
ffmpeg -i music_FLAC_24BIT_48kHz_stereo.flac -c:a pcm_s24be -vn music_PCM_24BIT_48kHz_stereo.aif

# MP3 (320kbps)
ffmpeg -i music_FLAC_24BIT_48kHz_stereo.flac -c:a libmp3lame -b:a 320k -vn music_MP3_320kbps_48kHz_stereo.mp3

# OGG (Vorbis)
ffmpeg -i music_FLAC_24BIT_48kHz_stereo.flac -c:a libvorbis -q:a 10 -vn music_Vorbis_500kbps_48kHz_stereo.ogg

# OPUS
ffmpeg -i music_FLAC_24BIT_48kHz_stereo.flac -c:a libopus -b:a 160k -vn music_Opus_160kbps_48kHz_stereo.opus

# SPEEX
ffmpeg -i music_FLAC_24BIT_48kHz_stereo.flac -c:a libspeex -q:a 8 -vn music_Speex_32kHz_stereo.spx
```

## 3. System 'file' Command Output
```plain
music_FLAC_24BIT_48kHz_stereo.flac:     FLAC audio bitstream data, 24 bit, stereo, 48 kHz, 7950423 samples
music_MP3_320kbps_48kHz_stereo.mp3:     Audio file with ID3 version 2.4.0, contains: MPEG ADTS, layer III, v1, 320 kbps, 48 kHz, Stereo
music_Opus_160kbps_48kHz_stereo.opus:   Ogg data, Opus audio, version 0.1, stereo, 48000 Hz (Input Sample Rate)
music_PCM_24BIT_48kHz_stereo.aif:       IFF data, AIFF audio
music_PCM_24BIT_48kHz_stereo.wav:       RIFF (little-endian) data, WAVE audio, stereo 48000 Hz
music_Speex_32kHz_stereo.spx:           Ogg data, Speex audio
music_Vorbis_500kbps_48kHz_stereo.ogg:  Ogg data, Vorbis audio, stereo, 48000 Hz, ~499821 bps
```
