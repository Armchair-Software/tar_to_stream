# tar_to_stream.h
A tiny C++ single-header header-only library for writing TAR archives to arbitrary streams, packaging data from memory.

## Features

- Compact, single header library - just include in your project and go.
- Modern C++.
- Identical output to [GNU TAR](https://www.gnu.org/software/tar/manual/html_node/Standard.html).
- Write tarball to any iostream - for example:
  - In memory [`std::ostringstream`](https://en.cppreference.com/w/cpp/io/basic_ostringstream).
  - Direct to file using [`std::ofstream`](https://en.cppreference.com/w/cpp/io/basic_ofstream).
  - Over the network using [Boost ASIO streams](https://www.boost.org/doc/libs/1_81_0/doc/html/boost_asio/overview/networking/iostreams.html).
  - To a logging framework, for example [LogStorm](https://github.com/VoxelStorm-Ltd/logstorm).
  - Compressed streams using [`bxz::ostream`](https://github.com/tmaklin/bxzstr) or any other streaming compression library.
  - Anything with a `<<` operator.
- Set any filename.
- Optional advanced settings:
  - File mode, modification time, user and group ID and name.
  - Reasonable defaults provided if you don't care.
- Size and checksum computed automatically.
- Optional writing of the tail section.

## Motivation

Imagine you've created several files in memory - let's say, a set of PNG images, using libPNG.  You now have a set of buffers containing your image data in memory, and you want to produce a compressed TAR archive and send it over the network.  You do not have files on disk, and you do not want to waste time writing those files out before tarballing and compressing them in order to send.  You want to use a streaming compression with something like [bxzstr](https://github.com/tmaklin/bxzstr), so you want to write your TAR data to a standard iostream without any intermediate copies.  And you want to do it all in clean modern C++.  This is the tool to do that.

This library was written out of a desire to create TAR archives in memory, containing "files" that also only existed in memory, without touching the disk, and avoiding unnecessary copies.

At the time of writing, there is no other header-only C++ TAR library, and no library capable of easily creating TAR archives from data in memory rather than files on disk, and writing to streams rather than files on disk.  The majority of libraries capable of handling TAR archives are written in C and somewhat bloated, and there is currently no other implementation that can write in TAR format to a stream without incurring multiple copies.  Additionally, most of the C implementations that can write to memory require a fixed size buffer allocation up front.  This tiny, simple library addresses all of these gaps.

## Use case

This was written for a web application using Emscripten, to use in concert with [emscripten-browser-file](https://github.com/Armchair-Software/emscripten-browser-file) to offer the user "download" of multiple files created in memory by the application.  `emscripten-browser-file` can create downloads from files in memory - `tar-to-stream` is intended to allow multiple such "files" to be packaged together for joint download.  These files never exist on disk, and in a wasm environment there may not even be a filesystem available, so an implementation that avoids touching the disk altogether is ideal.

## Usage

### Getting started

The following example creates a simple "file" in memory (a text file containing "hello world"), and creates a tar archive with three copies of it.

```cpp
#include <tar_to_stream.h>
#include <fstream>

auto main()->int {
  std::string const my_buffer{"Hello world!\n"};
  std::ofstream stream("my_tarball.tar", std::ios::binary);
  
  tar_to_stream(stream, { // add one file to the archive
    .filename{"my_file_1.txt"},
    .data{std::as_bytes(std::span{my_buffer})},
  });
  tar_to_stream(stream, {
    .filename{"my_file_2.txt"},
    .data{std::as_bytes(std::span{my_buffer})},
  });
  tar_to_stream(stream, {
    .filename{"my_file_3.txt",
    .data{std::as_bytes(std::span{my_buffer})},
  });
  tar_to_stream_tail(stream); // finalise the archive by adding a tail of zeros

  return EXIT_SUCCESS;
}
```

This produces a TAR archive on disk, containing your files, with default attributes:

```bash
> tar -xvvf my_tarball.tar
-rw-r--r-- root/root        13 1970-01-01 01:00 my_file_1.txt
-rw-r--r-- root/root        13 1970-01-01 01:00 my_file_2.txt
-rw-r--r-- root/root        13 1970-01-01 01:00 my_file_3.txt

> cat my_file_1.txt
Hello world!

```

### Advanced usage

```cpp
  tar_to_stream(stream, {                        // the stream to write to
    .filename{"my_dir/my_filename.txt"},         // filename (string) including directory
    .data{std::as_bytes(std::span{my_buffer})},  // pointer to buffer data (file contents)
    .mtime{static_cast<uint64_t>(std::time(0))}, // file modification time (mtime): this sets it to "now"
    .filemode{"755"},                            // file mode
    .uid{1000},                                  // file owner user ID
    .gid{1000},                                  // file owner group ID
    .uname{"my_username"},                       // file owner username
    .gname{"my_group"}                           // file owner group name
  });
```
(Note that all members of the `tar_to_stream_properties` struct are optional except `filename` and `data`; they will default to standard defaults if left unspecified.)

```bash
> tar --list -vvf my_tarball.tar 
-rwxr-xr-x my_username/my_group 13 2023-01-30 12:19 my_dir/my_filename.txt

> tar --list --numeric-owner -vvf my_tarball.tar 
-rwxr-xr-x 1000/1000        13 2023-01-30 12:19 my_dir/my_filename.txt
```

- Filename includes directory structure
- File modification time is in seconds since epoch, i.e. 1 Jan 1970.
  - If you just want "now", you can use `std::time(0)`.
- File mode is standard unix [numeric notation](https://en.wikipedia.org/wiki/File-system_permissions#Numeric_notation), i.e. `644`, `755`.
  - The TAR format actually uses up to 7 digits for the mode (our default is `0000644`), but what happens with non-standard modes is up to the unpacking implementation.
- User ID and group ID can be set to anything.
  - Some unpacking implementations attempt to match them to existing system users when extracting, others may just ignore them.
  - They don't have to match anything that actually exists, either on the packing or the unpacking system.
  - They don't have to match each other, usernames don't need to match real user IDs etc.
  - Default is set to '0' ('root') - this is by convention - although it is not specified in any standard, it is generally taken to mean "no user information is stored".
- Using `tar_to_stream_tail(stream)` after adding all your files is optional, but recommended.
  - It is in the [official standard](https://www.gnu.org/software/tar/manual/html_node/Standard.html), but most TAR extraction tools should function just fine without it.
  - Many implementations will issue a truncation warning if it is not found.

## Limitations

- This library only creates TAR archives, it does not provide functionality to unpack existing archives.
  - Contributions for unpack functionality in the same style as the existing library are welcome - simple unpacking of a TAR from a stream, to dynamically allocated buffers in memory.
- No support for links
  - Only normal files can be represented in the archive, links are not supported.
- No support for file versioning
  - TAR archives support multiple files with the same name and different versions.  You can insert multiple files with the same name, but versioning is not handled.
