#include "byte_reader.h"
#include <archive.h>
#include <archive_entry.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
bool ArchiveByteReader::open(const std::string& path, bool force_raw) {
    close();

    auto open_with = [&](bool raw_only) -> bool {
        a_ = archive_read_new();
        if (!a_) return false;

        archive_read_support_filter_all(a_);
        if (!raw_only) {
            archive_read_support_format_all(a_);
        }
        archive_read_support_format_raw(a_); // allow raw compressed streams

        int r = archive_read_open_filename(a_, path.c_str(), 1 << 20);
        if (r != ARCHIVE_OK) {
            std::fprintf(stderr, "open%s: %s\n",
                         raw_only ? " (raw-only)" : "",
                         archive_error_string(a_));
            archive_read_free(a_);
            a_ = nullptr;
            return false;
        }
        return true;
    };

    if (force_raw) {
        // Caller insists on raw mode
        if (!open_with(true)) {
            eof_ = true;
            return false;
        }
    } else {
        // Try normal (formats + raw), then fallback to raw-only
        if (!open_with(false)) {
            if (!open_with(true)) {
                eof_ = true;
                return false;
            }
        }
    }

    if (!next_entry()) {
        eof_ = true;
        return false;
    }

    eof_ = false;
    buf_.clear();
    pos_ = 0;
    return true;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
bool ArchiveByteReader::next_entry() {
  archive_entry* e=nullptr;
  int r = archive_read_next_header(a_, &e);
  if (r == ARCHIVE_EOF) return false;
  if (r != ARCHIVE_OK) {
    // Try to reopen RAW-only once if we somehow got here with a non-RAW handle.
    std::fprintf(stderr, "next_header: %s\n", archive_error_string(a_));
    return fail("next_header");
  }
  return true;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
bool ArchiveByteReader::fill() {
  if (!a_) return false;
  const void* blk=nullptr; size_t sz=0; la_int64_t off=0;
  int r = archive_read_data_block(a_, &blk, &sz, &off);
  if (r == ARCHIVE_EOF) {
    // try next entry (tar)
    if (next_entry()) return fill();
    eof_ = true; return false;
  }
  if (r != ARCHIVE_OK) return fail("read_data_block");
  buf_.assign(static_cast<const unsigned char*>(blk),
              static_cast<const unsigned char*>(blk)+sz);
  pos_ = 0;
  return true;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
size_t ArchiveByteReader::read(void* dst, size_t n) {
  if (!a_ || eof_ || n==0) return 0;
  unsigned char* out = static_cast<unsigned char*>(dst);
  size_t copied = 0;
  while (copied < n) {
    if (pos_ >= buf_.size()) {
      if (!fill()) break;
    }
    size_t avail = buf_.size() - pos_;
    size_t take = (n - copied < avail) ? (n - copied) : avail;
    std::memcpy(out + copied, buf_.data() + pos_, take);
    pos_ += take;
    copied += take;
  }
  if (copied==0 && eof_) return 0;
  return copied;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
bool ArchiveByteReader::fail(const char* where){
  if (a_) std::fprintf(stderr, "%s: %s\n", where, archive_error_string(a_));
  return false;
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------
void ArchiveByteReader::close() {
  if (a_) {
    archive_read_close(a_);
    archive_read_free(a_);
    a_ = nullptr;
  }
  eof_ = true;
  buf_.clear();
  pos_ = 0;
}

