#pragma once
#include <string>
#include <vector>

class ArchiveWriter {
public:
  ~ArchiveWriter() { close(); }

  // Open output by extension:
  //   raw: .jsonl, or compressed: .gz/.xz/.bz2/.zst
  //   or tar containers: .tar, .tar.gz/.xz/.bz2/.zst
  // For tar, content is a single entry named "trace.jsonl".
  bool open(const std::string& path);

  // Append one NDJSON line (adds '\n')
  bool write_line(const std::string& line);

  void close();

private:
  static bool ends_with(const std::string& s, const char* suf);
  bool write_raw(const void* data, size_t len);
  bool fail();

  // state
  std::string path_;
  struct archive* a_ = nullptr;
  struct archive_entry* entry_ = nullptr;
  bool isTar_ = false;

  // streaming tar staging (disk spill)
  bool staging_ = false;
  std::string tmpPath_;
  int   tmpFd_ = -1;
  FILE* tmpFp_ = nullptr;


  // non-tar path: write either to a normal FILE* 
  // or to a compressor pipe via popen()
  bool usePipe_ = false;
  FILE* pipe_ = nullptr;    // when usePipe_ == true
  FILE* rawFile_ = nullptr; // when writing uncompressed .jsonl
};

