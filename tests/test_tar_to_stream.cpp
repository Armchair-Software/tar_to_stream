#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <span>
#include <string>

#include "tar_to_stream.h"

/// Mirror of the internal 512-byte TAR header for byte-level verification
struct ParsedHeader {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char padding[12];
};
static_assert(sizeof(ParsedHeader) == 512, "TAR header must be exactly 512 bytes");

/// Extract the first 512-byte header block from the stream output
ParsedHeader get_header(std::string const &stream_data) {
  REQUIRE(stream_data.size() >= 512);
  ParsedHeader h{};
  std::memcpy(&h, stream_data.data(), sizeof(h));
  return h;
}

/// Recompute the TAR checksum treating the chksum field as 8 spaces (per POSIX spec)
unsigned int recompute_checksum(ParsedHeader h) {
  std::memset(h.chksum, ' ', sizeof(h.chksum));
  unsigned int sum{0};
  auto const *bytes{reinterpret_cast<uint8_t const *>(&h)};
  for(size_t i{0}; i != sizeof(h); ++i) {
    sum += bytes[i];
  }
  return sum;
}

// ---------------------------------------------------------------------------
// Basic output
// ---------------------------------------------------------------------------

TEST_CASE("Single file produces at least one 512-byte block") {
  std::ostringstream ss;
  std::string const content{"Hello, world!\n"};
  std::string const filename{"test.txt"};
  tar_to_stream(ss, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
  });
  CHECK(ss.str().size() >= 512);
}

TEST_CASE("Header magic and format fields") {
  std::ostringstream ss;
  std::string const content{"data"};
  std::string const filename{"f.txt"};
  tar_to_stream(ss, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
  });
  auto const hdr{get_header(ss.str())};

  SECTION("Magic is 'ustar '") {
    CHECK(std::string(hdr.magic, sizeof(hdr.magic)) == "ustar ");
  }
  SECTION("Version field: first byte is ' ', second byte is null") {
    CHECK(hdr.version[0] == ' ');
    CHECK(hdr.version[1] == '\0');
  }
  SECTION("Typeflag is '0' (regular file)") {
    CHECK(hdr.typeflag == '0');
  }
  SECTION("Linkname is all null bytes") {
    for(char c : hdr.linkname) {
      CHECK(c == '\0');
    }
  }
}

TEST_CASE("File content appears immediately after the 512-byte header") {
  std::ostringstream ss;
  std::string const content{"Hello, world!\n"};
  std::string const filename{"test.txt"};
  tar_to_stream(ss, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
  });
  std::string const output{ss.str()};
  REQUIRE(output.size() >= 512 + content.size());
  CHECK(output.substr(512, content.size()) == content);
}

// ---------------------------------------------------------------------------
// Filename
// ---------------------------------------------------------------------------

TEST_CASE("Filename stored correctly in header") {
  SECTION("Short filename") {
    std::ostringstream ss;
    std::string const filename{"hello.txt"};
    std::string const content{"x"};
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.name) == filename);
  }

  SECTION("Maximum length filename (99 characters)") {
    std::ostringstream ss;
    std::string const filename(99, 'a');
    std::string const content{"x"};
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.name, 99) == filename);
  }

  SECTION("Filename with path separators") {
    std::ostringstream ss;
    std::string const filename{"dir/subdir/file.txt"};
    std::string const content{"x"};
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.name) == filename);
  }
}

// ---------------------------------------------------------------------------
// File mode
// ---------------------------------------------------------------------------

TEST_CASE("File mode zero-padded to 7 digits") {
  std::string const content{"data"};
  std::string const filename{"f.txt"};

  SECTION("Default mode '644' becomes '0000644'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.mode, 7) == "0000644");
  }

  SECTION("Mode '755' becomes '0000755'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .filemode = "755",
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.mode, 7) == "0000755");
  }

  SECTION("Already 7-digit mode is preserved") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .filemode = "0000600",
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.mode, 7) == "0000600");
  }
}

// ---------------------------------------------------------------------------
// UID / GID
// ---------------------------------------------------------------------------

TEST_CASE("UID and GID stored as zero-padded octal") {
  std::string const content{"data"};
  std::string const filename{"f.txt"};

  SECTION("Default UID 0 stored as '0000000'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.uid) == "0000000");
  }

  SECTION("Default GID 0 stored as '0000000'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.gid) == "0000000");
  }

  SECTION("UID 1000 stored as octal '0001750'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .uid      = 1000,
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.uid) == "0001750");
  }

  SECTION("GID 1000 stored as octal '0001750'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .gid      = 1000,
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.gid) == "0001750");
  }

  SECTION("Stored value round-trips through octal parse") {
    std::ostringstream ss;
    unsigned int const test_uid{42};
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .uid      = test_uid,
    });
    auto const hdr{get_header(ss.str())};
    unsigned int const parsed{static_cast<unsigned int>(std::stoul(std::string(hdr.uid), nullptr, 8))};
    CHECK(parsed == test_uid);
  }
}

// ---------------------------------------------------------------------------
// File size
// ---------------------------------------------------------------------------

TEST_CASE("File size stored as 11-digit octal in header") {
  std::string const filename{"f.txt"};

  SECTION("Empty file size is '00000000000'") {
    std::ostringstream ss;
    std::string const content{};
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.size, 11) == "00000000000");
  }

  SECTION("Size round-trips correctly through octal parse") {
    std::ostringstream ss;
    std::string const content(137, 'X');
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    size_t const parsed{std::stoul(std::string(hdr.size), nullptr, 8)};
    CHECK(parsed == content.size());
  }

  SECTION("Size 512 round-trips correctly") {
    std::ostringstream ss;
    std::string const content(512, 'A');
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    size_t const parsed{std::stoul(std::string(hdr.size), nullptr, 8)};
    CHECK(parsed == 512);
  }
}

// ---------------------------------------------------------------------------
// Modification time
// ---------------------------------------------------------------------------

TEST_CASE("Modification time stored as 11-digit octal") {
  std::string const content{"data"};
  std::string const filename{"f.txt"};

  SECTION("Default mtime 0 is '00000000000'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.mtime, 11) == "00000000000");
  }

  SECTION("Custom mtime round-trips through octal parse") {
    std::ostringstream ss;
    uint64_t const test_mtime{1700000000ULL};
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .mtime    = test_mtime,
    });
    auto const hdr{get_header(ss.str())};
    uint64_t const parsed{std::stoull(std::string(hdr.mtime), nullptr, 8)};
    CHECK(parsed == test_mtime);
  }
}

// ---------------------------------------------------------------------------
// Username and group name
// ---------------------------------------------------------------------------

TEST_CASE("Username and group name in header") {
  std::string const content{"data"};
  std::string const filename{"f.txt"};

  SECTION("Default uname is 'root'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.uname) == "root");
  }

  SECTION("Default gname is 'root'") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.gname) == "root");
  }

  SECTION("Custom uname is stored correctly") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .uname    = "alice",
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.uname) == "alice");
  }

  SECTION("Custom gname is stored correctly") {
    std::ostringstream ss;
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
      .gname    = "developers",
    });
    auto const hdr{get_header(ss.str())};
    CHECK(std::string(hdr.gname) == "developers");
  }
}

// ---------------------------------------------------------------------------
// Checksum
// ---------------------------------------------------------------------------

TEST_CASE("Header checksum is correct") {
  std::ostringstream ss;
  std::string const content{"Hello, world!\n"};
  std::string const filename{"test.txt"};
  tar_to_stream(ss, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
  });
  auto const hdr{get_header(ss.str())};

  unsigned int const stored{static_cast<unsigned int>(std::stoul(std::string(hdr.chksum), nullptr, 8))};
  unsigned int const computed{recompute_checksum(hdr)};
  CHECK(stored == computed);
}

TEST_CASE("Checksum changes when header fields change") {
  std::string const content{"data"};
  std::string const filename{"f.txt"};

  std::ostringstream ss1;
  tar_to_stream(ss1, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
    .uid      = 0,
  });

  std::ostringstream ss2;
  tar_to_stream(ss2, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
    .uid      = 1000,
  });

  auto const hdr1{get_header(ss1.str())};
  auto const hdr2{get_header(ss2.str())};
  unsigned int const chk1{static_cast<unsigned int>(std::stoul(std::string(hdr1.chksum), nullptr, 8))};
  unsigned int const chk2{static_cast<unsigned int>(std::stoul(std::string(hdr2.chksum), nullptr, 8))};
  CHECK(chk1 != chk2);
}

// ---------------------------------------------------------------------------
// Data padding
// ---------------------------------------------------------------------------

TEST_CASE("Output is padded to 512-byte blocks") {
  std::string const filename{"f.txt"};

  SECTION("Data of 5 bytes: output is 1024 bytes (header + one data block)") {
    std::ostringstream ss;
    std::string const content(5, 'X');
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    CHECK(ss.str().size() == 1024);
  }

  SECTION("Padding bytes are null") {
    std::ostringstream ss;
    std::string const content(5, 'X');
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    std::string const output{ss.str()};
    for(size_t i{512 + content.size()}; i < output.size(); ++i) {
      CHECK(output[i] == '\0');
    }
  }

  SECTION("Data of exactly 512 bytes: no padding added") {
    std::ostringstream ss;
    std::string const content(512, 'X');
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    // header (512) + data (512) + padding (0) = 1024
    CHECK(ss.str().size() == 1024);
  }

  SECTION("Data of 513 bytes: 511 bytes of padding") {
    std::ostringstream ss;
    std::string const content(513, 'X');
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    // header (512) + data (513) + padding (511) = 1536
    CHECK(ss.str().size() == 1536);
  }

  SECTION("Empty data: no padding") {
    std::ostringstream ss;
    std::string const content{};
    tar_to_stream(ss, tar_to_stream_properties{
      .filename = filename,
      .data     = std::as_bytes(std::span{content.data(), content.size()}),
    });
    // header (512) + data (0) + padding (0) = 512
    CHECK(ss.str().size() == 512);
  }
}

// ---------------------------------------------------------------------------
// Tail
// ---------------------------------------------------------------------------

TEST_CASE("tar_to_stream_tail produces null bytes") {
  SECTION("Default tail length is 1024 bytes") {
    std::ostringstream ss;
    tar_to_stream_tail(ss);
    std::string const output{ss.str()};
    CHECK(output.size() == 1024);
    for(char c : output) {
      CHECK(c == '\0');
    }
  }

  SECTION("Custom tail length") {
    std::ostringstream ss;
    tar_to_stream_tail(ss, 2048);
    std::string const output{ss.str()};
    CHECK(output.size() == 2048);
    for(char c : output) {
      CHECK(c == '\0');
    }
  }
}

// ---------------------------------------------------------------------------
// Multiple files
// ---------------------------------------------------------------------------

TEST_CASE("Multiple files are laid out correctly in the archive") {
  std::ostringstream ss;
  std::string const content1{"First file content"};
  std::string const content2{"Second file content!"};
  std::string const name1{"file1.txt"};
  std::string const name2{"file2.txt"};

  tar_to_stream(ss, tar_to_stream_properties{
    .filename = name1,
    .data     = std::as_bytes(std::span{content1.data(), content1.size()}),
  });
  tar_to_stream(ss, tar_to_stream_properties{
    .filename = name2,
    .data     = std::as_bytes(std::span{content2.data(), content2.size()}),
  });
  tar_to_stream_tail(ss);

  std::string const output{ss.str()};

  auto const hdr1{get_header(output)};
  CHECK(std::string(hdr1.name) == name1);

  // Second header starts after header1 + first data block (padded to 512)
  size_t const offset2{512 + ((content1.size() + 511) / 512) * 512};
  REQUIRE(output.size() >= offset2 + 512);
  ParsedHeader hdr2{};
  std::memcpy(&hdr2, output.data() + offset2, sizeof(hdr2));
  CHECK(std::string(hdr2.name) == name2);
}

TEST_CASE("Content of each file in a multi-file archive is correct") {
  std::ostringstream ss;
  std::string const content1{"Alpha content"};
  std::string const content2{"Beta content here"};
  std::string const name1{"alpha.txt"};
  std::string const name2{"beta.txt"};

  tar_to_stream(ss, tar_to_stream_properties{
    .filename = name1,
    .data     = std::as_bytes(std::span{content1.data(), content1.size()}),
  });
  tar_to_stream(ss, tar_to_stream_properties{
    .filename = name2,
    .data     = std::as_bytes(std::span{content2.data(), content2.size()}),
  });

  std::string const output{ss.str()};

  CHECK(output.substr(512, content1.size()) == content1);

  size_t const data2_start{512 + ((content1.size() + 511) / 512) * 512 + 512};
  CHECK(output.substr(data2_start, content2.size()) == content2);
}

// ---------------------------------------------------------------------------
// Deprecated API (backwards compatibility)
// ---------------------------------------------------------------------------

TEST_CASE("Deprecated positional API produces identical output to the new API") {
  std::string const content{"Hello, world!\n"};
  std::string const filename{"test.txt"};

  std::ostringstream ss_new;
  tar_to_stream(ss_new, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
    .mtime    = 12345,
    .filemode = "755",
    .uid      = 1000,
    .gid      = 2000,
    .uname    = "alice",
    .gname    = "staff",
  });

  std::ostringstream ss_old;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  tar_to_stream(ss_old, filename, content.data(), content.size(),
                12345, "755", 1000, 2000, "alice", "staff");
#pragma GCC diagnostic pop

  CHECK(ss_new.str() == ss_old.str());
}

// ---------------------------------------------------------------------------
// Full archive round-trip (write + verify with known-good values)
// ---------------------------------------------------------------------------

TEST_CASE("Complete archive has valid structure") {
  std::ostringstream ss;
  std::string const content{"The quick brown fox jumps over the lazy dog.\n"};
  std::string const filename{"fox.txt"};

  tar_to_stream(ss, tar_to_stream_properties{
    .filename = filename,
    .data     = std::as_bytes(std::span{content.data(), content.size()}),
    .mtime    = 1000000000ULL,
    .filemode = "644",
    .uid      = 0,
    .gid      = 0,
    .uname    = "root",
    .gname    = "root",
  });
  tar_to_stream_tail(ss);

  std::string const output{ss.str()};
  auto const hdr{get_header(output)};

  CHECK(std::string(hdr.name) == filename);
  CHECK(std::string(hdr.magic, 6) == "ustar ");
  CHECK(hdr.typeflag == '0');

  size_t const stored_size{std::stoul(std::string(hdr.size), nullptr, 8)};
  CHECK(stored_size == content.size());

  uint64_t const stored_mtime{std::stoull(std::string(hdr.mtime), nullptr, 8)};
  CHECK(stored_mtime == 1000000000ULL);

  unsigned int const stored_chk{static_cast<unsigned int>(std::stoul(std::string(hdr.chksum), nullptr, 8))};
  CHECK(stored_chk == recompute_checksum(hdr));

  CHECK(output.substr(512, content.size()) == content);
}
