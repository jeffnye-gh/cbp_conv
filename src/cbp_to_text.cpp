#include "trace_reader.h"
#include "io_archive.h"
#include "text_fmt.h"
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
// -------------------------------------------------------------------------
// CBP -> TEXT path
// -------------------------------------------------------------------------
bool run_cbp_to_text(const std::string& in,
                     const std::string& out, uint64_t limit)
{
  TraceReader tr(in.c_str());

  // Write either to file (any extension) or stdout
  FILE* ofp = nullptr;
  const bool to_file = !out.empty();
  if (to_file) {
    ofp = std::fopen(out.c_str(), "w");
    if (!ofp) {
      std::fprintf(stderr, "Failed to open output: %s\n", out.c_str());
      return false;
    }
  } else {
    ofp = stdout;
  }

  uint64_t n = 0;
  while (limit == ~0ULL || n < limit) {
    db_t* rec = tr.get_inst();
    if (!rec) break;

    const std::string line = format_text_line(*rec);
    std::fputs(line.c_str(), ofp);
    std::fputc('\n', ofp);

    delete rec;
    ++n;
  }

  if (to_file && ofp && ofp != stdout) std::fclose(ofp);
  std::fprintf(stderr, "Text lines emitted=%llu\n", (unsigned long long)n);
  return true;
}

