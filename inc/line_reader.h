#pragma once
#include <string>
#include "byte_reader.h"

// Simple '\n' line splitter over ArchiveByteReader
class ArchiveLineReader {
public:
  bool open(const std::string& path, bool force_raw=false) {
    buf_.clear(); pos_=0; eof_ = false; return rdr_.open(path, force_raw);
  }

  void close() { rdr_.close(); buf_.clear(); pos_=0; eof_ = true; }

  bool next_line(std::string& out) {
    out.clear();
    while (true) {
      // scan existing buffer
      for (; pos_ < buf_.size(); ++pos_) {
        if (buf_[pos_] == '\n') {
          out.append(reinterpret_cast<const char*>(buf_.data()+tok_), pos_-tok_);
          ++pos_;
          tok_ = pos_;
          return true;
        }
      }
      // take remaining as partial token
      if (tok_ < buf_.size()) {
        out.append(reinterpret_cast<const char*>(buf_.data()+tok_), buf_.size()-tok_);
      }
      // refill
      tok_ = pos_ = 0;
      buf_.resize(1<<20);
      size_t got = rdr_.read(buf_.data(), buf_.size());
      if (got == 0) { eof_ = true; return !out.empty(); }
      buf_.resize(got);
    }
  }

  bool eof() const { return eof_; }

private:
  ArchiveByteReader rdr_;
  std::vector<unsigned char> buf_;
  size_t pos_ = 0, tok_ = 0;
  bool eof_ = true;
};

