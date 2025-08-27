#include "converter.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
extern void usage(const char*);

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
static bool ends_with_ci(const std::string& s, const char* suf){
  const size_t n = std::strlen(suf);
  if (s.size() < n) return false;
  for (size_t i=0;i<n;i++){
    char a = s[s.size()-n+i], b = suf[i];
    if (a>='A'&&a<='Z') a = char(a - 'A' + 'a');
    if (b>='A'&&b<='Z') b = char(b - 'A' + 'a');
    if (a!=b) return false;
  }
  return true;
}

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
static inline bool starts_with(const char* s, const char* p) {
  return std::strncmp(s, p, std::strlen(p)) == 0;
}

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
static bool parse_args(int argc, char** argv,
                       std::string& in, std::string& out, uint64_t& limit,
                       std::string& err)
{
  in.clear(); out.clear(); limit = ~0ULL;

  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];

    // --help / -h early out
    if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
      err.clear();
      return false; // caller will call usage() and exit 0
    }

    // --in <path>  or  --in=<path>
    if (std::strcmp(a, "--in") == 0) {
      if (++i >= argc) { err = "missing value for --in"; return false; }
      in = argv[i];
      continue;
    } else if (starts_with(a, "--in=")) {
      in = a + 5;
      if (in.empty()) { err = "empty value for --in="; return false; }
      continue;
    }

    // --out <path>  or  --out=<path>
    if (std::strcmp(a, "--out") == 0) {
      if (++i >= argc) { err = "missing value for --out"; return false; }
      out = argv[i];
      continue;
    } else if (starts_with(a, "--out=")) {
      out = a + 6;
      if (out.empty()) { err = "empty value for --out="; return false; }
      continue;
    }

    // --limit <n>  or  --limit=<n>  (accepts 10/0x10)
    if (std::strcmp(a, "--limit") == 0) {
      if (++i >= argc) { err = "missing value for --limit"; return false; }
      const char* s = argv[i];
      errno = 0;
      char* end = nullptr;
      unsigned long long v = std::strtoull(s, &end, 0);
      if (errno || end == s || *end != '\0') { err = "bad --limit value"; return false; }
      limit = static_cast<uint64_t>(v);
      continue;
    } else if (starts_with(a, "--limit=")) {
      const char* s = a + 8;
      if (!*s) { err = "empty value for --limit="; return false; }
      errno = 0;
      char* end = nullptr;
      unsigned long long v = std::strtoull(s, &end, 0);
      if (errno || end == s || *end != '\0') { err = "bad --limit value"; return false; }
      limit = static_cast<uint64_t>(v);
      continue;
    }

    // Unknown arg
    err = std::string("unknown arg: ") + a;
    return false;
  }

  if (in.empty())  { err = "missing --in";  return false; }
  if (out.empty()) { err = "missing --out"; return false; }
  return true;
}

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
int main(int argc, char** argv) {
  std::string in, out, perr;
  uint64_t limit = ~0ULL;

  if (!parse_args(argc, argv, in, out, limit, perr)) {
    if (perr.empty()) { usage(argv[0]); return 0; }  // -h/--help path
    std::fprintf(stderr, "-E: %s\n", perr.c_str());
    usage(argv[0]);
    return 2;
  }

  Converter conv;
  std::string err;
  if (!conv.convert(in, out, limit, &err)) {
    std::fprintf(stderr, "-E: %s\n", err.c_str());
    return 1;
  }
  return 0;
}

