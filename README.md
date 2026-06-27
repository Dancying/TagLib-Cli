# TagLib-Cli

A simple and efficient C++ command-line tool based on TagLib for reading, writing, and managing audio file metadata and artwork.

## Building

### Prerequisites
You need a C++ compiler supporting C++17, CMake 3.10+, and the required development tools and libraries installed on your system. Run the following command to install them:

```bash
sudo dnf install cmake gcc-c++ make taglib-devel
```

### Build Steps
```bash
mkdir build && cd build
cmake ..
make
```
The executable will be generated in the `bin/` directory.

## Usage

```text
Usage: taglib-cli [OPTIONS] <audio_file>

Options:
  -r, --read <field>                Read and display the value of a tag (e.g., Title, Artist).
  -w, --write <field>=<val>         Set or update a specific tag with text or file content.
  -d, --delete <field>              Remove a specific tag completely from the audio file.
  -e, --extract [type=]path         Save artwork to path. (Default type: Front_Cover)
  -i, --inject [type=]path          Insert a JPG/PNG image into audio. (Default type: Front_Cover)
  -v, --version                     Show app version, build info, and TagLib version.
  -h, --help                        Show this help message and exit.

Operational Examples:
  taglib-cli -r Title -r Artist track.mp3         # Multiple read operations
  taglib-cli -w Front_Cover=cover.jpg track.mp3   # Write artwork via -w option
  taglib-cli -w Title=lyrics.txt track.mp3        # Write text content from file
```
