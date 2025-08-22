#include "text_fmt.h"
#include <sstream>
#include <iomanip>

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline const char* type_name(const db_t& d)
{
  // Render with the same strings as in the sample 
  switch (d.insn_class) {
    case InstClass::loadInstClass: return "loadOp";
    case InstClass::storeInstClass: return "stOp";
    case InstClass::condBranchInstClass: return "condBrOp";
    case InstClass::uncondDirectBranchInstClass: return "uncondDirBrOp";
    case InstClass::uncondIndirectBranchInstClass: return "uncondIndBrOp";
    default: return "aluOp";
  }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static void add_op(std::ostringstream& oss, const char* label,
                   const db_operand_t& o)
{
  if (!o.valid) return;
  oss << " " << label << " input:  "
      << "(int: " << (o.is_int?1:0) << ", idx: " << std::dec << o.log_reg
      << " val: " << std::hex << std::nouppercase 
      << std::setfill('0') << o.value << std::dec << ")  ";
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
std::string format_text_line(const db_t& d)
{
  std::ostringstream oss;
  oss << "[PC: 0x" << std::hex << std::nouppercase << std::setfill('0')
      << std::setw(0) << d.pc << std::dec
      << " type: " << type_name(d);

  if (d.is_load || d.is_store) {
    oss << " ea: 0x" << std::hex << d.addr << std::dec
        << " size: " << d.size;
  }

  if (is_br(d.insn_class)) {
    oss << " ( tkn:" << (d.is_taken ? 1 : 0)
        << " tar: 0x" << std::hex << d.next_pc << std::dec << ")  ";
  }

  // inputs (A,B,C)
  if (d.A.valid) add_op(oss, "1st", d.A);
  if (d.B.valid) add_op(oss, "2nd", d.B);
  if (d.C.valid) add_op(oss, "3rd", d.C);

  // output (D)
  if (d.D.valid) {
    oss << " output:  "
        << "(int: " << (d.D.is_int?1:0) << ", idx: " << std::dec << d.D.log_reg
        << " val: " << std::hex << d.D.value << std::dec << ")  ";
  }

  oss << " ]";
  return oss.str();
}

