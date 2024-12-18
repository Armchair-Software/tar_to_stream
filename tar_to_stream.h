#pragma once

#include <span>
#include <string>
#include <string_view>
#include <cstring>

struct tar_to_stream_properties {
  /// Properties of the file to enter into the stream
  std::string const &filename;                                                  /// name of the file to write
  std::span<std::byte const> data;                                              /// the location of the file's contents in memory
  uint64_t mtime{0u};                                                           /// file modification time, in seconds since epoch
  std::string filemode{"644"};                                                  /// file mode
  unsigned int uid{0u};                                                         /// file owner user ID
  unsigned int gid{0u};                                                         /// file owner group ID
  std::string const &uname{"root"};                                             /// file owner username
  std::string const &gname{"root"};                                             /// file owner group name
};

template<typename T>
void tar_to_stream(T &stream,                                                   /// stream to write to, e.g. ostream or ofstream
                   tar_to_stream_properties &&file) {                           /// properties of the file to enter into the stream
  /// Read a "file" in memory, and write it as a TAR archive to the stream
  struct {                                                                      // offset
    char name[100]{};                                                           //   0    filename
    char mode[8]{};                                                             // 100    file mode: 0000644 etc
    char uid[8]{};                                                              // 108    user id, ascii representation of octal value: "0001750" (for UID 1000)
    char gid[8]{};                                                              // 116    group id, ascii representation of octal value: "0001750" (for GID 1000)
    char size[12]{};                                                            // 124    file size, ascii representation of octal value
    char mtime[12]{"00000000000"};                                              // 136    modification time, seconds since epoch
    char chksum[8]{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};                     // 148    checksum: six octal bytes followed by null and ' '.  Checksum is the octal sum of all bytes in the header, with chksum field set to 8 spaces.
    char typeflag{'0'};                                                         // 156    '0'
    char linkname[100]{};                                                       // 157    null bytes when not a link
    char magic[6]{'u', 's', 't', 'a', 'r', ' '};                                // 257    format: Unix Standard TAR: "ustar ", not null-terminated
    char version[2]{" "};                                                       // 263    " "
    char uname[32]{};                                                           // 265    user name
    char gname[32]{};                                                           // 297    group name
    char devmajor[8]{};                                                         // 329    null bytes
    char devminor[8]{};                                                         // 337    null bytes
    char prefix[155]{};                                                         // 345    null bytes
    char padding[12]{};                                                         // 500    padding to reach 512 block size
  } header;                                                                     // 512

  file.filemode.insert(file.filemode.begin(), 7 - file.filemode.length(), '0'); // zero-pad the file mode

  std::strncpy(header.name,  file.filename.c_str(), sizeof(header.name ) - 1);  // leave one char for the final null
  std::strncpy(header.mode,  file.filemode.c_str(), sizeof(header.mode ) - 1);
  std::strncpy(header.uname, file.uname.c_str(),    sizeof(header.uname) - 1);
  std::strncpy(header.gname, file.gname.c_str(),    sizeof(header.gname) - 1);

  sprintf(header.size,  "%011lo",  file.data.size());
  sprintf(header.mtime, "%011llo", file.mtime);
  sprintf(header.uid,   "%07o",    file.uid);
  sprintf(header.gid,   "%07o",    file.gid);

  {
    unsigned int checksum_value = 0;
    for(size_t i{0}; i != sizeof(header); ++i) {
      checksum_value += reinterpret_cast<uint8_t*>(&header)[i];
    }
    sprintf(header.chksum, "%06o", checksum_value);
  }

  size_t const padding{(512u - file.data.size() % 512) & 511u};
  stream << std::string_view{header.name, sizeof(header)}
         << std::string_view{reinterpret_cast<char const*>(file.data.data()), file.data.size()}
         << std::string(padding, '\0');
}

template<typename T>
[[deprecated("Use tar_to_stream_properties as argument: tar_to_stream(stream, {...}) - this allows use of designated initialisers and cleaner code.  Refer to tar_to_stream's README for example usage")]]
void tar_to_stream(T &stream,                                                   /// stream to write to, e.g. ostream or ofstream
                   std::string const &filename,                                 /// name of the file to write
                   char const *data_ptr,                                        /// pointer to the data in this archive segment
                   size_t data_size,                                            /// size of the data
                   uint64_t mtime = 0u,                                         /// file modification time, in seconds since epoch
                   std::string filemode = "644",                                /// file mode
                   unsigned int uid = 0u,                                       /// file owner user ID
                   unsigned int gid = 0u,                                       /// file owner group ID
                   std::string const &uname = "root",                           /// file owner username
                   std::string const &gname = "root") {                         /// file owner group name
  /// Explicit argument constructor, for backwards compatibility
  tar_to_stream(stream, tar_to_stream_properties{
    .filename{filename},
    .data{std::as_bytes(std::span{data_ptr, data_size})},
    .mtime{mtime},
    .filemode{filemode},
    .uid{uid},
    .gid{gid},
    .uname{uname},
    .gname{gname},
  });
}

template<typename T>
void tar_to_stream_tail(T &stream, unsigned int tail_length = 512u * 2u) {
  /// TAR archives expect a tail of null bytes at the end - min of 512 * 2, but implementations often add more
  stream << std::string(tail_length, '\0');
}
