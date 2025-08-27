#include "converter.h"

#include <algorithm>
#include <cctype>
#include <cstring>

extern bool run_cbp_to_text(const std::string& in,
                            const std::string& out, uint64_t limit);

extern bool run_cbp_to_asm(const std::string& in,
                           const std::string& out, uint64_t limit);

// ------------------------------------

bool Converter::ends_with_ext(const std::string& s, const char* ext) const {
  const size_t n = std::strlen(ext);
  if (s.size() < n) return false;

  if (!case_insensitive_ext_) {
    return std::equal(s.end() - n, s.end(), ext);
  }

  return std::equal(s.end() - n, s.end(), ext,
                    [](char a, char b){ return std::tolower(a) == std::tolower(b); });
}

bool Converter::strip_suffix(std::string& s, const char* ext) const {
  if (!ends_with_ext(s, ext)) return false;
  s.resize(s.size() - std::strlen(ext));
  return true;
}

Comp Converter::parse_comp_suffix(std::string& stem) const {
  if (strip_suffix(stem, ".gz"))  return Comp::GZ;
  if (strip_suffix(stem, ".xz"))  return Comp::XZ;
  if (strip_suffix(stem, ".bz2")) return Comp::BZ2;
  if (strip_suffix(stem, ".zst")) return Comp::ZST;
  return Comp::NONE;
}

BaseFmt Converter::parse_base_ext(std::string& stem) const {
  if (strip_suffix(stem, ".cbp"))   return BaseFmt::CBP_BIN;
  if (strip_suffix(stem, ".txt"))   return BaseFmt::CBP_TEXT;
  if (strip_suffix(stem, ".jsonl")) return BaseFmt::NDJSON;
  if (strip_suffix(stem, ".asm"))   return BaseFmt::ASM;   // output-only
  if (strip_suffix(stem, ".stf"))   return BaseFmt::STF;   // output-only
  if (strip_suffix(stem, ".memh"))  return BaseFmt::MEMH;  // output-only
  return BaseFmt::UNKNOWN; // no recognized base ext
}

const char* Converter::fmt_name(BaseFmt f) const {
  switch (f) {
    case BaseFmt::CBP_BIN:  return "CBP_BIN";
    case BaseFmt::CBP_TEXT: return "CBP_TEXT";
    case BaseFmt::NDJSON:   return "NDJSON";
    case BaseFmt::ASM:      return "ASM";
    case BaseFmt::STF:      return "STF";
    case BaseFmt::MEMH:     return "MEMH";
    default:                return "UNKNOWN";
  }
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
bool Converter::convert(const ConvertPlan& plan, std::string* err) {
  // CBP -> text
  if (plan.in.fmt == BaseFmt::CBP_BIN && plan.out.fmt == BaseFmt::CBP_TEXT) {
    return cbp_to_text(plan, err);
  }

  // CBP -> asm
  if (plan.in.fmt == BaseFmt::CBP_BIN && plan.out.fmt == BaseFmt::ASM)
    return cbp_to_asm(plan, err);

  if (err) {
    *err = std::string("route not implemented: ")
         + fmt_name(plan.in.fmt) + " -> " + fmt_name(plan.out.fmt);
  }
  return false;
}

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
bool Converter::convert(const std::string& in_path,
                        const std::string& out_path,
                        uint64_t limit,
                        std::string* err) {
  ConvertPlan plan = make_plan(in_path, out_path, limit);
  return convert(plan, err);
}

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
FileSpec Converter::parse_path(const std::string& path) const {
  FileSpec spec;
  spec.path = path;

  // Work on a temporary "stem" we can strip suffixes from
  std::string stem = path;

  // 1) Compression (last)
  spec.comp = parse_comp_suffix(stem);

  // 2) Base format (middle)
  BaseFmt f = parse_base_ext(stem);

  // 3) Default: no recognized base ext => CBP binary per spec
  if (f == BaseFmt::UNKNOWN) f = BaseFmt::CBP_BIN;

  spec.fmt = f;
  return spec;
}
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
ConvertPlan Converter::make_plan(const std::string& in_path,
                                 const std::string& out_path,
                                 uint64_t limit) const {
  ConvertPlan plan;
  plan.in  = parse_path(in_path);
  plan.out = parse_path(out_path);
  plan.limit = limit;
  return plan;
}
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
bool Converter::cbp_to_text(const ConvertPlan& plan, std::string* err) {
  // Validate route: input must be CBP_BIN; output must be CBP_TEXT
  if (plan.in.fmt != BaseFmt::CBP_BIN) {
    if (err) *err = "cbp_to_text: input is not CBP binary (expected <none> or .cbp).";
    return false;
  }
  if (plan.out.fmt != BaseFmt::CBP_TEXT) {
    if (err) *err = "cbp_to_text: output is not .txt (CBP text).";
    return false;
  }

  // Dispatch to existing converter (streaming, handles compression by path)
  const bool ok = run_cbp_to_text(plan.in.path, plan.out.path, plan.limit);
  if (!ok && err) {
    *err = "run_cbp_to_text failed.";
  }
  return ok;
}

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
bool Converter::cbp_to_asm(const ConvertPlan& plan, std::string* err) {
  if (plan.in.fmt != BaseFmt::CBP_BIN) {
    if (err) *err = "cbp_to_asm: input must be CBP binary.";
    return false;
  }
  if (plan.out.fmt != BaseFmt::ASM) {
    if (err) *err = "cbp_to_asm: output must be .asm.";
    return false;
  }

  const bool ok = run_cbp_to_asm(plan.in.path, plan.out.path, plan.limit);
  if (!ok && err) *err = "run_cbp_to_asm failed.";
  return ok;
}
