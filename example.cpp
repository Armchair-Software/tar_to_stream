#include <tar_to_stream.h>
#include <fstream>
#include <ctime>

auto main()->int {
  std::string const my_buffer{"Hello world!\n"};
  std::ofstream stream("my_tarball.tar", std::ios::binary);

  tar_to_stream(stream, "hello_world_basic.txt", my_buffer.data(), my_buffer.size()); // add one file to the archive

  tar_to_stream(
    stream,                              // the stream to write to
    "hello_world_advanced.txt",          // filename (string)
    my_buffer.data(),                    // pointer to buffer data (file contents)
    my_buffer.size(),                    // size of buffer data (file contents)
    static_cast<uint64_t>(std::time(0)), // file modification time (mtime): this sets it to "now"
    "755",                               // file mode
    1000,                                // file owner user ID
    1000,                                // file owner group ID
    "my_username",                       // file owner username
    "my_group"                           // file owner group name
  );

  tar_to_stream_tail(stream); // finalise the archive by adding a tail of zeros

  return EXIT_SUCCESS;
}
