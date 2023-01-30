#ifndef TAR_TO_STREAM_H_INCLUDED
#define TAR_TO_STREAM_H_INCLUDED

#include <cstring>

template<typename T>
void tar_to_stream(T &stream,                                                   /// stream to write to, e.g. ostream or ofstream
                   std::string const &filename,                                 /// name of the file to write
                   char const *data,                                            /// pointer to the data in this archive segment
                   size_t size,                                                 /// size of the data
                   uint64_t mtime = 0u,                                         /// file modification time, in seconds since epoch
                   std::string filemode = "644",                                /// file mode
                   unsigned int uid = 0u,                                       /// file owner user ID
                   unsigned int gid = 0u,                                       /// file owner group ID
                   std::string const &uname = "root",                           /// file owner username
                   std::string const &gname = "root") {                         /// file owner group name
  /// Read a "file" in memory, and write it as a TAR archive to the stream
  struct {                                                                      // offset
    char name[100]     = {};                                                    //   0    filename
    char mode[8]       = {};                                                    // 100    file mode: 0000644 etc
    char uid[8]        = {};                                                    // 108    user id, ascii representation of octal value: "0001750" (for UID 1000)
    char gid[8]        = {};                                                    // 116    group id, ascii representation of octal value: "0001750" (for GID 1000)
    char size[12]      = {};                                                    // 124    file size, ascii representation of octal value
    char mtime[12]     = "00000000000";                                         // 136    modification time, seconds since epoch
    char chksum[8]     = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};              // 148    checksum: six octal bytes followed by null and ' '.  Checksum is the octal sum of all bytes in the header, with chksum field set to 8 spaces.
    char typeflag      = '0';                                                   // 156    '0'
    char linkname[100] = {};                                                    // 157    null bytes when not a link
    char magic[6]      = {'u', 's', 't', 'a', 'r', ' '};                        // 257    format: Unix Standard TAR: "ustar ", not null-terminated
    char version[2]    = " ";                                                   // 263    " "
    char uname[32]     = {};                                                    // 265    user name
    char gname[32]     = {};                                                    // 297    group name
    char devmajor[8]   = {};                                                    // 329    null bytes
    char devminor[8]   = {};                                                    // 337    null bytes
    char prefix[155]   = {};                                                    // 345    null bytes
    char padding[12]   = {};                                                    // 500    padding to reach 512 block size
  } header;                                                                     // 512

  filemode.insert(filemode.begin(), 7 - filemode.length(), '0');                // zero-pad the file mode

  std::strncpy(header.name,  filename.c_str(),  sizeof(header.name ) - 1);      // leave one char for the final null
  std::strncpy(header.mode,  filemode.c_str(),  sizeof(header.mode ) - 1);
  std::strncpy(header.uname, uname.c_str(),     sizeof(header.uname) - 1);
  std::strncpy(header.gname, gname.c_str(),     sizeof(header.gname) - 1);

  sprintf(header.size,  "%011lo", size);
  sprintf(header.mtime, "%011llo", mtime);
  sprintf(header.uid,   "%07o",   uid);
  sprintf(header.gid,   "%07o",   gid);

  unsigned int checksum_value = 0;
  for(unsigned int i = 0; i != sizeof(header); ++i) {
    checksum_value += reinterpret_cast<uint8_t*>(&header)[i];
  }
  sprintf(header.chksum, "%06o", checksum_value);

  auto const padding{512u - static_cast<unsigned int>(size % 512)};
  stream << std::string_view(header.name, sizeof(header)) << std::string_view(data, size) << std::string(padding, '\0');
}

template<typename T>
void tar_to_stream_tail(T &stream, unsigned int tail_length = 512u * 2u) {
  /// TAR archives expect a tail of null bytes at the end - min of 512 * 2, but implementations often add more
  stream << std::string(tail_length, '\0');
}

#endif // TAR_TO_STREAM_H_INCLUDED
