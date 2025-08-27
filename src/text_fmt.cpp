#include "text_fmt.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

static inline const char* type_name(const db_t& d) {
  switch (d.insn_class) {
    case InstClass::callDirectInstClass:         return  "callDirBrOp";
    case InstClass::callIndirectInstClass:       return  "callIndBrOp";
    case InstClass::condBranchInstClass:         return  "condBrOp";
    case InstClass::fpInstClass:                 return  "fpOp";
    case InstClass::loadInstClass:               return  "loadOp";
    case InstClass::ReturnInstClass:             return  "retBrOp";
    case InstClass::slowAluInstClass:            return  "slowAluOp";
    case InstClass::storeInstClass:              return  "stOp";
    case InstClass::uncondDirectBranchInstClass: return  "uncondDirBrOp";
    case InstClass::uncondIndirectBranchInstClass:return "uncondIndBrOp";
    default:                                     return "aluOp";
  }
}

// lowercase 0x + no leading zeros (except single 0)
static inline std::string norm_hex_0x(uint64_t x) {
  std::ostringstream os;
  os << std::hex << std::nouppercase << x;
  std::string body = os.str();
  // remove leading zeros by converting through integer already; just ensure "0"
  if (body.empty()) body = "0";
  return std::string("0x") + body;
}

// hex body with no 0x, lowercase, no leading zeros
static inline std::string hex_body(uint64_t x) {
  std::ostringstream os;
  os << std::hex << std::nouppercase << x;
  std::string s = os.str();
  if (s.empty()) return "0";
  // remove all leading zeros but keep one if all zeros
  auto it = std::find_if(s.begin(), s.end(), [](char c){ return c!='0'; });
  if (it == s.end()) return "0";
  return std::string(it, s.end());
}

static inline void add_input(std::ostringstream& oss,
                             const char* ordinal,
                             const db_operand_t& o)
{
  if (!o.valid) return;
  oss << ordinal << " input:  "
      << "(int: " << (o.is_int ? 1 : 2)
      << ", idx: " << std::dec << o.log_reg
      << " val: " << hex_body(o.value) << ")  ";
}

std::string format_text_line(const db_t& d)
{
  std::ostringstream oss;
  oss << "[PC: " << norm_hex_0x(d.pc)
      << " type: " << type_name(d) << ' ';

  // memory metadata
  if (d.is_load || d.is_store) {
    oss << "ea: "   << norm_hex_0x(d.addr) << ' '
        << "size: " << std::dec << d.size << ' ';
  }

  // inputs A/B/C
  add_input(oss, "1st", d.A);
  add_input(oss, "2nd", d.B);
  add_input(oss, "3rd", d.C);

  // output D
  if (d.D.valid) {
    oss << "output:  "
        << "(int: " << (d.D.is_int ? 1 : 2)
        << ", idx: " << std::dec << d.D.log_reg
        << " val: " << hex_body(d.D.value) << ")  ";
  }

  oss << " ]";
  return oss.str();
}

