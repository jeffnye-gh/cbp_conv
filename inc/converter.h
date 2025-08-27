#pragma once
#include <string>
#include <cstdint>

// Formats & compression 
enum class BaseFmt {
  CBP_BIN,   // <none> or .cbp
  CBP_TEXT,  // .txt
  NDJSON,    // .jsonl
  ASM,       // .asm (output-only)
  STF,       // .stf (output-only)
  MEMH,      // .memh (output-only)
  UNKNOWN
};

enum class Comp { NONE, GZ, XZ, BZ2, ZST };

struct FileSpec {
  std::string path; // original path
  BaseFmt     fmt = BaseFmt::UNKNOWN;
  Comp        comp = Comp::NONE;
};

struct ConvertPlan {
  FileSpec  in;
  FileSpec  out;
  uint64_t  limit = 0; // 0 = unlimited
};

// Single-class converter 
class Converter {
public:
  Converter() = default;
  ~Converter() = default;

  // Optional behavior knob
  void set_case_insensitive_ext(bool v) { case_insensitive_ext_ = v; }
  bool case_insensitive_ext() const { return case_insensitive_ext_; }

  // Parse a path into (fmt, comp)
  FileSpec    parse_path(const std::string& path) const;

  const char* fmt_name(BaseFmt f) const;

  // Compose a plan from input/output paths + limit
  ConvertPlan make_plan(const std::string& in_path,
                        const std::string& out_path,
                        uint64_t limit = 0) const;

  bool convert(const ConvertPlan& plan, std::string* err);
  bool convert(const std::string& in_path,
               const std::string& out_path,
               uint64_t limit,
               std::string* err);

  // Perform CBP(binary) -> FORMAT conversion
  // Returns false and fills *err on validation/dispatch failure.
  bool cbp_to_text(const ConvertPlan& plan, std::string* err);
  bool cbp_to_asm (const ConvertPlan& plan, std::string* err);

private:
  // Helpers
  bool ends_with_ext(const std::string& s, const char* ext) const;
  bool strip_suffix(std::string& s, const char* ext) const;

  // pops .gz/.xz/.bz2/.zst
  Comp     parse_comp_suffix(std::string& stem) const;

  // pops .cbp/.txt/.jsonl/.asm/.stf/.memh
  BaseFmt  parse_base_ext(std::string& stem) const;

  bool case_insensitive_ext_ = true;
};

