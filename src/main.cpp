#include "cbp_trace_reader.h"
#include "io_archive.h"
#include "line_reader.h"
#include "ndjson_parse.h"
#include "text_fmt.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
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
static bool looks_json_input(const std::string& in){
  return ends_with_ci(in, ".json") || ends_with_ci(in, ".jsonl") ||
         ends_with_ci(in, ".json.gz") || ends_with_ci(in, ".jsonl.gz") ||
         ends_with_ci(in, ".json.xz") || ends_with_ci(in, ".jsonl.xz") ||
         ends_with_ci(in, ".json.zst")|| ends_with_ci(in, ".jsonl.zst") ||
         ends_with_ci(in, ".json.bz2")|| ends_with_ci(in, ".jsonl.bz2") ||
         ends_with_ci(in, ".tar.json")|| ends_with_ci(in, ".tar.jsonl"); // tar cases covered by libarchive reader anyway
}

//// -------------------------------------------------------------------------
//// -------------------------------------------------------------------------
//static void usage(const char* a0){
//  std::fprintf(stderr,
//    "Usage: %s --in <input> [--out <output>] [--limit N]\n"
//    "  If <input> ends with .json/.jsonl (optionally compressed), convert NDJSON -> text.\n"
//    "  Otherwise, convert CBP binary -> NDJSON.\n",
//    a0);
//}
//
// -------------------------------------------------------------------------
// NDJSON -> text path
// -------------------------------------------------------------------------
static int run_json_to_text(const std::string& in,
                            const std::string& out, uint64_t limit)
{
  ArchiveLineReader lr;
  // Force RAW since .jsonl(.gz/.xz/...) is just a raw compressed byte stream
  if (!lr.open(in, /*force_raw=*/true)) {
    std::fprintf(stderr, "Failed to open input: %s\n", in.c_str());
    return 1;
  }

  ArchiveWriter w;
  bool to_file = !out.empty();
  if (to_file && !w.open(out)) {
    std::fprintf(stderr, "Failed to open output: %s\n", out.c_str());
    return 1;
  }

  std::string line; uint64_t n=0;

  while ((limit==~0ULL || n<limit) && lr.next_line(line)) {
    db_t rec{}; if (!parse_ndjson_line(line, rec)) continue;
    std::string txt = format_text_line(rec);

    if (to_file) {
      if (!w.write_line(txt)) {
        std::fprintf(stderr, "Write failed\n");
        return 1;
      }
    } else {
      std::puts(txt.c_str());
    }
    ++n;
  }
  if (to_file) w.close();
  std::fprintf(stderr, "Text lines emitted=%llu\n", (unsigned long long)n);
  return 0;
}

// -------------------------------------------------------------------------
// CBP -> NDJSON path
// -------------------------------------------------------------------------
static void dump_json(const db_t& d, FILE* f){
  std::fprintf(f, "{\"pc\":\"0x%016llx\",\"type\":\"%s\"",
               (unsigned long long)d.pc, cInfo[(uint8_t)d.insn_class]);
  if (d.is_load || d.is_store) {
    std::fprintf(f, ",\"ea\":\"0x%016llx\",\"size\":%llu",
      (unsigned long long)d.addr, (unsigned long long)d.size);
  }
  if (is_br(d.insn_class)) {
    std::fprintf(f, ",\"taken\":%s,\"target\":\"0x%016llx\"",
      d.is_taken ? "true":"false", (unsigned long long)d.next_pc);
  }
  auto add = [&](const char* nm, const db_operand_t& o){
    if (!o.valid) return;
    std::fprintf(f, ",\"%s\":{\"bank\":%d,\"idx\":%llu,\"val\":\"0x%016llx\"}",
      nm, o.is_int?1:2, (unsigned long long)o.log_reg, (unsigned long long)o.value);
  };
  add("A", d.A); add("B", d.B); add("C", d.C); add("D", d.D);
  std::fputs("}\n", f);
}

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
static int run_cbp_to_json(const std::string& in, const std::string& out, uint64_t limit){
  TraceReader tr(in.c_str());
  ArchiveWriter w;
  bool to_file = !out.empty();
  if (to_file && !w.open(out)) {
    std::fprintf(stderr, "Failed to open output: %s\n", out.c_str());
    return 1;
  }

  uint64_t n=0;
  while (limit==~0ULL || n<limit) {
    db_t* rec = tr.get_inst();
    if (!rec) break;
    if (to_file) {
      // build one JSON line (small string on stack)
      char tmp[128];
      std::string s;
      std::snprintf(tmp, sizeof(tmp), "{\"pc\":\"0x%016llx\",\"type\":\"%s\"",
                    (unsigned long long)rec->pc, cInfo[(uint8_t)rec->insn_class]);
      s += tmp;
      if (rec->is_load || rec->is_store) {
        std::snprintf(tmp, sizeof(tmp), ",\"ea\":\"0x%016llx\",\"size\":%llu",
                      (unsigned long long)rec->addr, (unsigned long long)rec->size);
        s += tmp;
      }
      if (is_br(rec->insn_class)) {
        std::snprintf(tmp, sizeof(tmp), ",\"taken\":%s,\"target\":\"0x%016llx\"",
             rec->is_taken ? "true":"false", (unsigned long long)rec->next_pc);
        s += tmp;
      }
      auto add = [&](const char* nm, const db_operand_t& o){
        if (!o.valid) return;
        char t2[160];
        std::snprintf(t2, sizeof(t2),
             ",\"%s\":{\"bank\":%d,\"idx\":%llu,\"val\":\"0x%016llx\"}",
             nm, o.is_int?1:2, (unsigned long long)o.log_reg,
                               (unsigned long long)o.value);
        s += t2;
      };
      add("A", rec->A); add("B", rec->B); add("C", rec->C); add("D", rec->D);
      s += "}";
      if (!w.write_line(s)) {
        std::fprintf(stderr, "Write failed\n");
        delete rec;
        return 1;
      }
    } else {
      dump_json(*rec, stdout);
    }
    delete rec; ++n;
  }

  if (to_file) w.close();

  std::fprintf(stderr, "NDJSON lines emitted=%llu\n", (unsigned long long)n);

  return 0;
}

// -------------------------------------------------------------------------
// -------------------------------------------------------------------------
int main(int argc, char** argv){
  std::string in, out;
  uint64_t limit=~0ULL;
  for (int i=1;i<argc;i++){
    std::string a=argv[i];
    if (a=="--in" && i+1<argc)    { in=argv[++i]; continue; }
    if (a=="--out" && i+1<argc)   { out=argv[++i]; continue; }
    if (a=="--limit" && i+1<argc) { limit=strtoull(argv[++i],nullptr,10); continue; }
    if (a=="-h" || a=="--help")   { usage(argv[0]); return 0; }
  }

  if (in.empty()) { usage(argv[0]); return 2; }

  if (looks_json_input(in)) {
    // NDJSON -> text
    return run_json_to_text(in, out, limit);
  } else {
    // CBP -> NDJSON
    return run_cbp_to_json(in, out, limit);
  }
}
