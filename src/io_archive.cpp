#include "io_archive.h"
#include <archive.h>
#include <archive_entry.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
static bool has_suffix(const std::string& s, const char* suf)
{
  const size_t n = std::strlen(suf);
  return s.size() >= n && s.compare(s.size()-n, n, suf)==0;
}

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
bool ArchiveWriter::ends_with(const std::string& s, const char* suf)
{
  return has_suffix(s, suf);
}

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
bool ArchiveWriter::open(const std::string& path) {
  close();
  path_ = path;

  auto is_tar_path = [](const std::string& p)->bool{
    return has_suffix(p, ".tar")
        || has_suffix(p, ".tar.gz")
        || has_suffix(p, ".tar.xz")
        || has_suffix(p, ".tar.bz2")
        || has_suffix(p, ".tar.zst");
  };

  isTar_ = is_tar_path(path);

  // ----- TAR OUTPUT (libarchive + staging to temp file) -----
  if (isTar_) {
    a_ = archive_write_new();
    if (!a_) return false;

    // choose compression filter
    if (has_suffix(path, ".gz"))  archive_write_add_filter_gzip(a_);
    if (has_suffix(path, ".xz"))  archive_write_add_filter_xz(a_);
    if (has_suffix(path, ".bz2")) archive_write_add_filter_bzip2(a_);
    if (has_suffix(path, ".zst")) archive_write_add_filter_zstd(a_);

    archive_write_set_format_pax_restricted(a_);
    if (archive_write_open_filename(a_, path.c_str()) != ARCHIVE_OK) return fail();

    char tmpl[] = "/tmp/trace2json.XXXXXXXX.jsonl";
    tmpFd_ = mkstemp(tmpl);
    if (tmpFd_ < 0) {
      std::fprintf(stderr, "mkstemp failed: %s\n", std::strerror(errno));
      return fail();
    }
    tmpPath_.assign(tmpl);
    tmpFp_ = fdopen(tmpFd_, "wb+");
    if (!tmpFp_) {
      std::fprintf(stderr, "fdopen failed: %s\n", std::strerror(errno));
      ::close(tmpFd_); tmpFd_ = -1;
      return fail();
    }
    staging_ = true;
    return true;
  }

  // ----- NON-TAR OUTPUT (pipe to compressor or plain file) -----
  usePipe_ = false; pipe_ = nullptr; rawFile_ = nullptr;

  // Decide compression
  std::string cmd;
       if (has_suffix(path, ".gz"))  { cmd = "gzip -c > \"" + path + "\""; }
  else if (has_suffix(path, ".xz"))  { cmd = "xz -c > \"" + path + "\""; }
  else if (has_suffix(path, ".bz2")) { cmd = "bzip2 -c > \"" + path + "\""; }
  else if (has_suffix(path, ".zst")) { cmd = "zstd -q -c > \"" + path + "\""; }

  if (!cmd.empty()) {
    // open pipe to compressor
    usePipe_ = true;
    std::string shell = "sh -c '" + cmd + "'";
    pipe_ = popen(shell.c_str(), "w");
    if (!pipe_) {
      std::fprintf(stderr, "popen failed for: %s (%s)\n",
                   shell.c_str(), std::strerror(errno));
      return false;
    }
  } else {
    // plain file
    rawFile_ = std::fopen(path.c_str(), "wb");
    if (!rawFile_) {
      std::fprintf(stderr, "fopen failed for %s: %s\n",
                   path.c_str(), std::strerror(errno));
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
bool ArchiveWriter::write_line(const std::string& line) {
  const char nl = '\n';

  // TAR path (staging to temp file)
  if (staging_) {
    if (!tmpFp_) return false;
    if (std::fwrite(line.data(), 1, line.size(), tmpFp_) != line.size()) {
      std::fprintf(stderr, "staging write failed: %s\n", std::strerror(errno));
      return false;
    }
    if (std::fwrite(&nl, 1, 1, tmpFp_) != 1) {
      std::fprintf(stderr, "staging write failed: %s\n", std::strerror(errno));
      return false;
    }
    return true;
  }

  // NON-TAR path: pipe or plain file
  FILE* out = usePipe_ ? pipe_ : rawFile_;
  if (!out) return false;

  if (std::fwrite(line.data(), 1, line.size(), out) != line.size()) {
    std::fprintf(stderr, "write failed: %s\n", std::strerror(errno));
    return false;
  }
  if (std::fwrite(&nl, 1, 1, out) != 1) {
    std::fprintf(stderr, "write failed: %s\n", std::strerror(errno));
    return false;
  }
  return true;
}

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
void ArchiveWriter::close() {
  // finalize tar if staging
  if (staging_) {
    if (tmpFp_) std::fflush(tmpFp_);
    struct stat st{};
    if (tmpFd_ >= 0 && fstat(tmpFd_, &st) == 0) {
      const la_int64_t total = static_cast<la_int64_t>(st.st_size);

      entry_ = archive_entry_new();
      archive_entry_set_pathname(entry_, "trace.jsonl");
      archive_entry_set_filetype(entry_, AE_IFREG);
      archive_entry_set_perm(entry_, 0644);
      archive_entry_set_size(entry_, total);

      if (archive_write_header(a_, entry_) != ARCHIVE_OK) {
        std::fprintf(stderr, "archive header error: %s\n",
                     archive_error_string(a_));
      } else {
        std::rewind(tmpFp_);
        std::vector<char> buf(1<<20);
        while (true) {
          size_t n = std::fread(buf.data(), 1, buf.size(), tmpFp_);
          if (n == 0) break;
          if (archive_write_data(a_, buf.data(), n) < 0) {
            std::fprintf(stderr, "archive write data error: %s\n",
                         archive_error_string(a_));
            break;
          }
        }
      }
      if (entry_) { archive_entry_free(entry_); entry_ = nullptr; }
    } else {
      std::fprintf(stderr, "staging stat failed: %s\n", std::strerror(errno));
    }
  }

  // close archive (tar)
  if (a_) { archive_write_close(a_); archive_write_free(a_); a_ = nullptr; }

  // close pipe or plain file (non-tar)
  if (pipe_)    {
    int rc = pclose(pipe_);
    (void)rc;
    pipe_ = nullptr;
  }

  if (rawFile_) {
    std::fclose(rawFile_);
    rawFile_ = nullptr;
  }

  // cleanup temp
  if (tmpFp_)            { std::fclose(tmpFp_); tmpFp_ = nullptr; }
  if (tmpFd_ >= 0)       { ::close(tmpFd_); tmpFd_ = -1; }
  if (!tmpPath_.empty()) { ::unlink(tmpPath_.c_str()); tmpPath_.clear(); }

  staging_ = false; isTar_ = false; usePipe_ = false;
}

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
bool ArchiveWriter::write_raw(const void* data, size_t len) 
{
  la_ssize_t w = archive_write_data(a_, data, len);
  if (w < 0) {
    std::fprintf(stderr, "archive write error: %s\n", archive_error_string(a_));
    return false;
  }
  return (static_cast<size_t>(w) == len);
}

// -------------------------------------------------------------------------------
// -------------------------------------------------------------------------------
bool ArchiveWriter::fail()
{
  if (a_) {
    const char* err = archive_error_string(a_);
    std::fprintf(stderr, "archive write error: %s\n", err ? err : "(unknown)");
    archive_write_free(a_);
    a_ = nullptr;
  }
  if (pipe_)   { pclose(pipe_);  pipe_ = nullptr; }
  if (rawFile_){ std::fclose(rawFile_); rawFile_ = nullptr; }
  if (tmpFp_)  { std::fclose(tmpFp_); tmpFp_ = nullptr; }
  if (tmpFd_ >= 0) { ::close(tmpFd_); tmpFd_ = -1; }
  if (!tmpPath_.empty()) { ::unlink(tmpPath_.c_str()); tmpPath_.clear(); }
  staging_ = false; isTar_ = false; usePipe_ = false;
  return false;
}
