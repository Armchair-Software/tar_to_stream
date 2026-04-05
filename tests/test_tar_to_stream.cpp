#include <tar_to_stream.h>
#include <sstream>
#include <cassert>
#include <cstring>
#include <iostream>

// Verify the standard TAR magic bytes and typeflag in the first 512-byte header block
static void check_header(const std::string &output) {
  assert(output.size() >= 512);
  assert(std::memcmp(output.data() + 257, "ustar ", 6) == 0); // magic bytes
  assert(output[263] == ' ');                                  // version
  assert(output[156] == '0');                                  // typeflag: regular file
}

int main() {
  // Test 1: basic usage – small data triggers non-zero block padding
  {
    std::ostringstream stream;
    std::string data{"Hello world!\n"};
    tar_to_stream(stream, {
      .filename{"test.txt"},
      .data{std::as_bytes(std::span{data})},
    });
    std::string output{stream.str()};
    assert(output.size() == 512 + 512); // header + 13 bytes padded to one 512-byte block
    check_header(output);
    assert(std::memcmp(output.data(), "test.txt", 8) == 0);
  }

  // Test 2: data exactly filling a 512-byte block – padding must be zero
  {
    std::ostringstream stream;
    std::string data(512, 'x');
    tar_to_stream(stream, {
      .filename{"exact_block.bin"},
      .data{std::as_bytes(std::span{data})},
    });
    std::string output{stream.str()};
    assert(output.size() == 512 + 512); // header + 512 data bytes, no padding
    check_header(output);
  }

  // Test 3: all optional properties specified
  {
    std::ostringstream stream;
    std::string data{"test content"};
    tar_to_stream(stream, {
      .filename{"dir/file.txt"},
      .data{std::as_bytes(std::span{data})},
      .mtime{1234567890u},
      .filemode{"755"},
      .uid{1000u},
      .gid{1000u},
      .uname{"testuser"},
      .gname{"testgroup"},
    });
    std::string output{stream.str()};
    assert(output.size() == 512 + 512);
    check_header(output);
    assert(std::memcmp(output.data(), "dir/file.txt", 12) == 0);
  }

  // Test 4: filemode already 7 chars long – zero-padding insert is skipped
  {
    std::ostringstream stream;
    std::string data{"x"};
    tar_to_stream(stream, {
      .filename{"file.txt"},
      .data{std::as_bytes(std::span{data})},
      .filemode{"0000755"},
    });
    std::string output{stream.str()};
    assert(output.size() == 512 + 512);
    check_header(output);
    assert(std::memcmp(output.data() + 100, "0000755", 7) == 0);
  }

  // Test 5: deprecated positional-argument overload
  {
    std::ostringstream stream;
    std::string data{"Hello world!\n"};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    tar_to_stream(stream, std::string{"compat.txt"}, data.data(), data.size());
#pragma GCC diagnostic pop
    std::string output{stream.str()};
    assert(output.size() == 512 + 512);
    check_header(output);
  }

  // Test 6: deprecated overload with all optional arguments
  {
    std::ostringstream stream;
    std::string data{"data"};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    tar_to_stream(stream, std::string{"opts.txt"}, data.data(), data.size(),
                  1000u, "755", 42u, 42u, "user", "group");
#pragma GCC diagnostic pop
    std::string output{stream.str()};
    assert(output.size() == 512 + 512);
    check_header(output);
  }

  // Test 7: default tail (1024 null bytes)
  {
    std::ostringstream stream;
    tar_to_stream_tail(stream);
    std::string output{stream.str()};
    assert(output.size() == 1024u);
    assert(output == std::string(1024, '\0'));
  }

  // Test 8: custom tail length
  {
    std::ostringstream stream;
    tar_to_stream_tail(stream, 512u);
    assert(stream.str().size() == 512u);
    assert(stream.str() == std::string(512, '\0'));
  }

  std::cout << "All tests passed!\n";
  return 0;
}
