#pragma once
#include <string>
#include <cstddef>
#include <cstdint>
#include <vector>

// Minimal streaming byte reader over raw/compressed/tar inputs via libarchive.
// Presents a simple read(void*, size) and eof() interface like istream::read.
class ArchiveByteReader {
public:
  ArchiveByteReader() {}
  ~ArchiveByteReader(){ close(); }

  // Open any of: raw, .gz, .xz, .bz2, .zst, .tar, .tar.{gz,xz,bz2,zst}
  // Returns true on success.
  bool open(const std::string& path, bool force_raw = false);

  // Read exactly 'n' bytes into dst, unless EOF occurs earlier.
  // Returns number of bytes copied (0 only at EOF).
  size_t read(void* dst, size_t n);

  // True iff no more bytes will be produced from this source.
  bool eof() const { return eof_; }

  void close();

private:
  struct archive* a_ = nullptr;
  bool eof_ = true;

  // Decompressed block buffered from libarchive
  std::vector<unsigned char> buf_;
  size_t pos_ = 0; // read offset within buf_

  bool next_entry();
  bool fill(); // fetch next data block when buffer is empty
  bool fail(const char* where);
};

