#include "ndjson_parse.h"
#include <cctype>
#include <cstdio>
#include <cstring>

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
static bool find_key(const std::string& s, const char* key, size_t& p) {
  // finds "key": starting at any position
  std::string needle = std::string("\"") + key + "\"";
  size_t k = s.find(needle);
  if (k == std::string::npos) return false;
  k += needle.size();
  // skip spaces and :
  while (k < s.size() && (std::isspace((unsigned char)s[k]) || s[k]==':')) ++k;
  p = k; return true;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
static bool parse_hex64_at(const std::string& s, size_t p, uint64_t& v) {
  // expect "0x..." or possibly bare hex digits
  if (p+2 < s.size() && s[p]=='\"'
      && s[p+1]=='0'
      && (s[p+2]=='x' || s[p+2]=='X')) 
  {
    p+=3;
  } else if (   p+1 < s.size()
             && s[p]=='0'
             && (s[p+1]=='x'||s[p+1]=='X')) 
  {
    p+=2;
  }

  else if (p < s.size() && s[p]=='\"') {
    ++p;
  }

  uint64_t val=0; bool any=false;

  while (p < s.size()) {
    char c = s[p];
    int d = -1;
    if (c>='0'&&c<='9') d=c-'0';
    else if (c>='a'&&c<='f') d=c-'a'+10;
    else if (c>='A'&&c<='F') d=c-'A'+10;
    else break;
    val = (val<<4) | (uint64_t)d; any=true; ++p;
  }
  if (p < s.size() && s[p]=='\"') ++p;
  if (!any) return false;
  v = val; return true;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
static bool parse_uint_at(const std::string& s, size_t p, uint64_t& v) {
  if (p < s.size() && s[p]=='\"') ++p;
  uint64_t val=0; bool any=false;
  while (p < s.size() && std::isdigit((unsigned char)s[p])) {
    val = val*10 + (s[p]-'0'); any=true; ++p;
  }
  if (p < s.size() && s[p]=='\"') ++p;
  if (!any) return false;
  v = val; return true;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
static bool parse_bool_at(const std::string& s, size_t p, bool& b) {
  if (s.compare(p, 4, "true")==0) { b=true; return true; }
  if (s.compare(p, 5, "false")==0){ b=false; return true; }
  return false;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
static bool parse_string_at(const std::string& s, size_t p, std::string& out) {
  if (p>=s.size() || s[p] != '\"') return false;
  ++p;
  size_t q = s.find('\"', p);
  if (q == std::string::npos) return false;
  out.assign(s.begin()+p, s.begin()+q);
  return true;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
static void maybe_parse_operand(const std::string& s, const char* name,
                                db_operand_t& o)
{
  size_t p;
  if (!find_key(s, name, p)) { o.valid=false; return; }
  // Expect object: {"bank":1,"idx":N,"val":"0x..."}
  size_t pb;
  if (!find_key(s.substr(p), "bank", pb)) { o.valid=false; return; }

  pb += p;

  uint64_t bank;
  if (!parse_uint_at(s, pb, bank)) { o.valid=false; return; }

  size_t pi;
  if (!find_key(s.substr(p), "idx", pi)) { o.valid=false; return; } pi += p;

  uint64_t idx;
  if (!parse_uint_at(s, pi, idx)) { o.valid=false; return; }

  size_t pv;
  if (!find_key(s.substr(p), "val", pv)) { o.valid=false; return; } pv += p;

  uint64_t val;
  if (!parse_hex64_at(s, pv, val)) { o.valid=false; return; }

  o.valid = true; o.is_int = (bank==1); o.log_reg = idx; o.value = val;
}

// --------------------------------------------------------------------------
// --------------------------------------------------------------------------
bool parse_ndjson_line(const std::string& s, db_t& out) {
  // mandatory: pc, type
  size_t p;
  if (!find_key(s, "pc", p)) return false;
  if (!parse_hex64_at(s, p, out.pc)) return false;

  if (!find_key(s, "type", p)) return false;
  std::string ty; if (!parse_string_at(s, p, ty)) return false;
  // Map type string to InstClass (names from your cInfo table)
  // Common ones
  if (ty=="loadOp") out.insn_class = InstClass::loadInstClass;
  else if (ty=="stOp" || ty=="storeOp") out.insn_class = InstClass::storeInstClass;
  else if (ty=="condBrOp") out.insn_class = InstClass::condBranchInstClass;
  else if (ty=="retBrOp")  out.insn_class = InstClass::uncondIndirectBranchInstClass;
  else if (ty=="uncondIndBrOp") out.insn_class = InstClass::uncondIndirectBranchInstClass;
  else if (ty=="uncondDirBrOp" || ty=="callDirBrOp" || ty=="callIndBrOp") out.insn_class = InstClass::uncondDirectBranchInstClass;
  else out.insn_class = InstClass::aluInstClass;

  // optional fields
  out.is_load  = (out.insn_class == InstClass::loadInstClass);
  out.is_store = (out.insn_class == InstClass::storeInstClass);

  if (find_key(s, "taken", p)) { parse_bool_at(s, p, out.is_taken); }
  if (find_key(s, "target", p)) { parse_hex64_at(s, p, out.next_pc); }
  if (find_key(s, "ea", p)) { parse_hex64_at(s, p, out.addr); }
  if (find_key(s, "size", p)) {
    uint64_t sz;
    if (parse_uint_at(s,p,sz)) out.size = sz;
  }

  // operands
  maybe_parse_operand(s, "A", out.A);
  maybe_parse_operand(s, "B", out.B);
  maybe_parse_operand(s, "C", out.C);
  maybe_parse_operand(s, "D", out.D);

  return true;
}

