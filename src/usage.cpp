#include <cstring>
#include <cstdio>

void usage(const char* a0)
{
  std::fprintf(stderr,
R"(
  Usage:
       %s --in <INPUT> [--out <OUTPUT>] [--limit N] [-h|--help]

  ---------------------------------------------------------------------
  Operations are auto mode by file extension:
    • If INPUT looks like JSON/NDJSON (.json / .jsonl, 
      optionally .gz/.xz/.bz2/.zst or inside .tar.*):

        Read NDJSON lines  →  write human-readable text (sample-style).

    • Otherwise (e.g., .cbp or any non-JSON binary stream,
      optionally compressed or in .tar.*):

        Read CBP binary trace  →  write NDJSON (one JSON object per line).

  ---------------------------------------------------------------------
  Supported inputs (decompressed transparently):
    Containers/filters:   .tar, .tar.gz, .tar.xz, .tar.bz2, .tar.zst
    Raw streams:          .gz, .xz, .bz2, .zst, or uncompressed files
    Formats:
      - CBP binary trace (default path when not JSON/NDJSON)
      - NDJSON (when INPUT extension indicates .json or .jsonl)

  ---------------------------------------------------------------------
  Supported outputs (chosen by OUTPUT extension; stdout if --out omitted):
    Text (sample-style):  .txt   (plain file)
    NDJSON:               .jsonl or .json
    Compression (optional): append .gz / .xz / .bz2 / .zst
    Tar container (single entry "trace.jsonl" for NDJSON, "trace.txt" for text):
                          .tar, .tar.gz, .tar.xz, .tar.bz2, .tar.zst

  ---------------------------------------------------------------------
  Decision rule:
    INPUT is considered JSON/NDJSON if its name ends with:
      .json, .jsonl, .json.gz, .jsonl.gz, .json.xz, .jsonl.xz,
      .json.bz2, .jsonl.bz2, .json.zst, .jsonl.zst, or inside .tar.* with those.
    Otherwise it’s treated as a CBP binary trace.

  ---------------------------------------------------------------------
  Examples:
    # Full circle:
      # 1) CBP binary → NDJSON (compressed)
      %s --in traces/sample_int_trace.gz --out output/sample.jsonl.gz

      # 2) NDJSON → human-readable text (compressed input; plain text output)
      %s --in output/sample.jsonl.gz --out output/sample.txt

    # Stream to stdout (useful for piping):
      %s --in traces/sample_int_trace.gz | gzip -9 > output/sample.jsonl.gz
      %s --in output/sample.jsonl.gz | head

    # Limit processing (first N records/pieces)
      %s --in traces/sample_int_trace.gz --out output/sample.jsonl --limit 1000

  ---------------------------------------------------------------------
  Notes:
    • Large files supported; reading and writing are fully streaming.
    • For non-tar compressed outputs (.gz/.xz/.bz2/.zst), 
      external compressors are used (gzip/xz/bzip2/zstd).
    • Tar outputs are built via libarchive and contain a single file:
      NDJSON → trace.jsonl,  Text → trace.txt
)",
    a0,a0,a0,a0,a0,a0);
}

