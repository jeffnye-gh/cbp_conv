#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <optional>
#include <algorithm>

#include "trace_reader.h"

// -----------------------------------------------------------------------------
// Normalized op (reader-agnostic) — fill from CBP reader in the adapter.
// -----------------------------------------------------------------------------
enum class OpKind {
  ALU, CALL_DIR, CALL_IND, COND_BR, FP, LOAD, RET, SLOW_ALU, STORE, UNCOND_DIR, UNCOND_IND, UNKNOWN
};

// -----------------------------------------------------------------------------
struct RegRef {
  uint32_t idx = 0;         // raw reg index from CBP (may be >31)
  std::string val_hex;      // original hex value (for comments)
};

// -----------------------------------------------------------------------------
struct Op {
  uint64_t pc = 0;

  OpKind kind = OpKind::UNKNOWN;

  // Branch/call metadata
  bool     taken = false;
  uint64_t target = 0;

  // Memory
  uint64_t ea = 0;
  uint32_t size = 0;        // bytes

  // Registers
  std::vector<RegRef> inputs;     // R1, R2, R3 in docs
  std::optional<RegRef> output;   // RD (destination)
};

// -----------------------------------------------------------------------------
// Helper: register naming & capping rules (from docs).
// -----------------------------------------------------------------------------
static inline uint32_t cap_reg(uint32_t raw) {
  // "Any operands which exceed their encoding range are capped at the maximum value."
  // For RISC-V integer regs, cap to x31.
  return std::min<uint32_t>(raw, 31);
}

// -----------------------------------------------------------------------------
// RD special cases:
//   - RD:64 => x31
//   - RD:0  => x1  (avoid nop optimizations)
// Inputs: just cap >31 to 31; allow x0.
// -----------------------------------------------------------------------------
static inline std::string rd_name(uint32_t rd_raw) {
  if (rd_raw == 64) return "x31";
  if (rd_raw == 0)  return "x1";
  return std::string("x") + std::to_string(cap_reg(rd_raw));
}
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline std::string rx_name(uint32_t r_raw) {
  return std::string("x") + std::to_string(cap_reg(r_raw));
}

// -----------------------------------------------------------------------------
// Format hex like 0xABC or ABC (docs mix styles; we'll use uppercase hex w/o 0x in comments to match examples)
// -----------------------------------------------------------------------------
static inline std::string hex_uc(uint64_t v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%llX", (unsigned long long)v);
  return std::string(buf);
}
// -----------------------------------------------------------------------------
static inline std::string hex_uc_pref(uint64_t v) {
  char buf[34];
  std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)v);
  return std::string(buf);
}

// -----------------------------------------------------------------------------
// Offsets & masking helpers.
// JAL: 20-bit signed; BR/JALR: 12-bit signed. We also echo masked hex like examples (e.g., f7c).
// -----------------------------------------------------------------------------
static inline int64_t signed_delta(uint64_t pc, uint64_t target) {
  return (int64_t)target - (int64_t)pc;
}
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline uint64_t mask_nbits(uint64_t v, unsigned nbits) {
  const uint64_t mask = (nbits >= 64) ? ~0ull : ((1ull << nbits) - 1);
  return v & mask;
}
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline bool fits_signed_nbits(int64_t v, unsigned nbits) {
  const int64_t minv = -(1ll << (nbits - 1));
  const int64_t maxv =  (1ll << (nbits - 1)) - 1;
  return (v >= minv && v <= maxv);
}

// -------------------------------------------------------------------------
// ASM formatting per op kind (returns full line including trailing metadata).
// Note: Where the spec/examples are inconsistent, we follow the commentary
// rules and add clear TODOs where you may want to tweak behavior.
// -------------------------------------------------------------------------
static std::string fmt_meta_pc(uint64_t pc) {
  return std::string("//PC:") + hex_uc(pc);
}
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string fmt_reg_meta(const char* tag, const RegRef& r) {
  // e.g., " RD:64 V:6"  or  " R1:10 V:deadbeef"
  return std::string(" ") + tag + ":" + std::to_string(r.idx) + " V:" + r.val_hex;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string format_alu(const Op& op) {
  const size_t n_in = op.inputs.size();
  const bool has_rd = op.output.has_value();

  // No operands
  if (!has_rd && n_in == 0) {
    return "fence.i          " + fmt_meta_pc(op.pc);
  }

  // One input (no RD in CBP) => "add x1, xR1" (use x1 as RD per docs)
  if (!has_rd && n_in == 1) {
    const auto& r1 = op.inputs[0];
    return "add x1," + rx_name(r1.idx) + "        " + fmt_meta_pc(op.pc)
         + fmt_reg_meta(" R1", r1);
  }

  // Input + RD => "add RD, R1"
  if (has_rd && n_in == 1) {
    const auto& r1 = op.inputs[0];
    const auto& rd = *op.output;
    return "add " + rd_name(rd.idx) 
                  + "," + rx_name(r1.idx) 
                  + "      " 
                  + fmt_meta_pc(op.pc)
                  + fmt_reg_meta(" RD", rd)
                  + fmt_reg_meta(" R1", r1);
  }

  // Two inputs + RD => "add RD, R1, R2"
  if (has_rd && n_in == 2) {
    const auto& r1 = op.inputs[0];
    const auto& r2 = op.inputs[1];
    const auto& rd = *op.output;
    return "add " + rd_name(rd.idx) + "," + rx_name(r1.idx) + "," + rx_name(r2.idx)
         + "   " + fmt_meta_pc(op.pc)
         + fmt_reg_meta(" RD", rd)
         + fmt_reg_meta(" R1", r1)
         + fmt_reg_meta(" R2", r2);
  }

  // Three inputs + RD => use 4-op form shown ("fsl")
  if (has_rd && n_in == 3) {
    const auto& r1 = op.inputs[0];
    const auto& r2 = op.inputs[1];
    const auto& r3 = op.inputs[2];
    const auto& rd = *op.output;
    return "fsl " + rd_name(rd.idx) + "," + rx_name(r1.idx) + "," + rx_name(r2.idx) + "," + rx_name(r3.idx)
         + " " + fmt_meta_pc(op.pc)
         + fmt_reg_meta(" RD", rd)
         + fmt_reg_meta(" R1", r1)
         + fmt_reg_meta(" R2", r2)
         + fmt_reg_meta(" R3", r3);
  }

  // Fallback
  return "fence.i          " + fmt_meta_pc(op.pc) + "  // TODO: unhandled aluOp arity";
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string format_call_dir(const Op& op) {
  // Use RD from output if present; else x1 is conventional link 
  // (but example uses x30).
  const std::string rd = op.output ? rd_name(op.output->idx) : std::string("x1");
  const int64_t d = signed_delta(op.pc, op.target);
  const bool fits = fits_signed_nbits(d, 20);
  const std::string off = fits ? hex_uc_pref((uint64_t)d) : "0x0";
  std::string line = "jal " + rd + ", " + off + "     " + fmt_meta_pc(op.pc)
                   + "  TAR:" + hex_uc(op.target) + " OFF:" + (fits ? hex_uc((uint64_t)d) : "0")
                   + " TKN:" + (op.taken ? "1" : "0");
  if (!fits) line += " TOO_LRG_OFF";
  if (op.output) line += fmt_reg_meta(" RD", *op.output);
  return line;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string format_call_ind(const Op& op) {
  const std::string rd = op.output ? rd_name(op.output->idx) : std::string("x1");
  const std::string rs = (op.inputs.size() >= 1) ? rx_name(op.inputs[0].idx) : "x0";
  return "jalr " + rd + ", " + rs + ", 0 " + fmt_meta_pc(op.pc)
       + "  TAR:" + hex_uc(op.target) + " OFF:0x0 TKN:" + (op.taken ? "1" : "0")
       + (op.output ? fmt_reg_meta(" RD", *op.output) : "")
       + (op.inputs.size() >= 1 ? fmt_reg_meta(" R1", op.inputs[0]) : "");
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string format_cond_br(const Op& op) {
  const int64_t d = signed_delta(op.pc, op.target);
  const bool fits = fits_signed_nbits(d, 12);
  const std::string off = op.taken ? (fits ? hex_uc_pref((uint64_t)d) : "0x0") : "0";
  std::string insn = op.taken ? "BEQ x0,x0," : "BNE x0,x0,";
  std::string line = insn + off + "  " + fmt_meta_pc(op.pc)
                   + "  TAR:" + hex_uc(op.target)
                   + " OFF:" + (op.taken ? (fits ? hex_uc((uint64_t)d) : "0") : "0")
                   + " TKN:" + (op.taken ? "1" : "0");
  if (op.inputs.size() >= 1) line += fmt_reg_meta(" R1", op.inputs[0]);
  if (op.taken && !fits) line += " TOO_LRG_OFF";
  return line;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string format_load(const Op& op) {
  const char* mnem = "ld";
  switch (op.size) {
    case 1: mnem = "lbu"; break;
    case 2: mnem = "lhu"; break;
    case 4: mnem = "lwu"; break;
    case 8: mnem = "ld";  break;
    default: mnem = "ld"; break; // TODO: warn unknown size
  }
  const std::string rd = op.output ? rd_name(op.output->idx) : "x0";
  // Per examples: use base x0, offset 0; EA appears only in metadata.
  std::string line = std::string(mnem) 
                   + "  x0, 0(x0) " 
                   + "//PC:" + hex_uc(op.pc)
                   + "  EA:"  + hex_uc(op.ea) 
                   + " SZ:" + std::to_string(op.size);
  if (op.output) line += fmt_reg_meta(" RD", *op.output);
  if (op.inputs.size() >= 1) line += fmt_reg_meta(" R1", op.inputs[0]);
  return line;
}

static std::string format_ret(const Op& op) {
  // Example shows jalr x0, x1, 0
  std::string rs = (op.inputs.size() >= 1) ? rx_name(op.inputs[0].idx) : "x1";
  return "jalr x0, " + rs + ", 0 //PC:" + hex_uc(op.pc)
       + "  TAR:" + hex_uc(op.target)
       + (op.inputs.size() >= 1 ? fmt_reg_meta(" R1", op.inputs[0]) : "");
}

static std::string format_slow_alu(const Op& op) {
  (void)op;
  return "divu x0,x0,x0  //PC:" + hex_uc(op.pc);
}

static std::string format_store(const Op& op) {
  const char* mnem = "std";
  switch (op.size) {
    case 1: mnem = "stb"; break;
    case 2: mnem = "sth"; break;
    case 4: mnem = "stw"; break;
    case 8: mnem = "std"; break;
    default: mnem = "std"; break; // TODO: warn unknown size
  }
  const std::string rs2 = (op.inputs.size() >= 2) 
                        ? rx_name(op.inputs[1].idx) : "x0"; // data
  const std::string rs1 = (op.inputs.size() >= 1) 
                        ? rx_name(op.inputs[0].idx) : "x0"; // base
  std::string line = std::string(mnem) 
                   + " " + rs2 
                   + ",0(" + rs1 + ") // PC:" 
                   + hex_uc(op.pc)
                   + " EA:" + hex_uc(op.ea) 
                   + " SIZE:" + std::to_string(op.size);
  if (op.inputs.size() >= 1) line += fmt_reg_meta(" R1", op.inputs[0]);
  if (op.inputs.size() >= 2) line += fmt_reg_meta(" R2", op.inputs[1]);
  return line;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string format_uncond_dir(const Op& op) {
  const int64_t d = signed_delta(op.pc, op.target);
  const bool fits = fits_signed_nbits(d, 20);
  const std::string off = fits ? hex_uc_pref((uint64_t)d) : "0x0";
  std::string line = "jal x0," + off + " //PC:" + hex_uc(op.pc)
                   + "  TAR:" + hex_uc(op.target)
                   + " OFF:" + (fits ? hex_uc((uint64_t)d) : "0")
                   + " TKN:" + (op.taken ? "1" : "0");
  if (!fits) line += " TOO_LRG_OFF";
  return line;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static std::string format_uncond_ind(const Op& op) {
  const int64_t d = signed_delta(op.pc, op.target);
  // JALR imm is 12-bit signed; examples show hex-masked form like f7c
  const uint64_t masked = mask_nbits((uint64_t)d, 12);
  std::string rs = (op.inputs.size() >= 1) ? rx_name(op.inputs[0].idx) : "x0";
  return "jalr x0," + rs + ",0x" + hex_uc(masked) + " //PC:" + hex_uc(op.pc)
       + "  TAR:" + hex_uc(op.target)
       + " OFF:" + hex_uc(masked)
       + " TKN:" + (op.taken ? "1" : "0");
}

// Master formatter
static std::string format_asm_line(const Op& op) {
  switch (op.kind) {
    case OpKind::ALU:         return format_alu(op);
    case OpKind::CALL_DIR:    return format_call_dir(op);
    case OpKind::CALL_IND:    return format_call_ind(op);
    case OpKind::COND_BR:     return format_cond_br(op);
    case OpKind::FP:          return "//PC:" + hex_uc(op.pc) 
                                     + "  // fpOp (no mapping yet)";
    case OpKind::LOAD:        return format_load(op);
    case OpKind::RET:         return format_ret(op);
    case OpKind::SLOW_ALU:    return format_slow_alu(op);
    case OpKind::STORE:       return format_store(op);
    case OpKind::UNCOND_DIR:  return format_uncond_dir(op);
    case OpKind::UNCOND_IND:  return format_uncond_ind(op);
    default:                  return "//PC:" + hex_uc(op.pc) 
                                     + "  // UNKNOWN op";
  }
}

// lower-case hex (no 0x) to match comment examples
static inline std::string hex_lower(uint64_t v) {
  char b[32];
  std::snprintf(b, sizeof b, "%llx", (unsigned long long)v);
  return std::string(b);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline OpKind to_kind(InstClass c) {
  switch (c) {
    case InstClass::aluInstClass:                  return OpKind::ALU;
    case InstClass::callDirectInstClass:           return OpKind::CALL_DIR;
    case InstClass::callIndirectInstClass:         return OpKind::CALL_IND;
    case InstClass::condBranchInstClass:           return OpKind::COND_BR;
    case InstClass::fpInstClass:                   return OpKind::FP;
    case InstClass::loadInstClass:                 return OpKind::LOAD;
    case InstClass::ReturnInstClass:               return OpKind::RET;
    case InstClass::slowAluInstClass:              return OpKind::SLOW_ALU;
    case InstClass::storeInstClass:                return OpKind::STORE;
    case InstClass::uncondDirectBranchInstClass:   return OpKind::UNCOND_DIR;
    case InstClass::uncondIndirectBranchInstClass: return OpKind::UNCOND_IND;
    default:                                       return OpKind::UNKNOWN;
  }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline bool is_branch_class(InstClass c) {
  switch (c) {
    case InstClass::callDirectInstClass:
    case InstClass::callIndirectInstClass:
    case InstClass::condBranchInstClass:
    case InstClass::ReturnInstClass:
    case InstClass::uncondDirectBranchInstClass:
    case InstClass::uncondIndirectBranchInstClass:
      return true;
    default: return false;
  }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline void map_db_to_op(const db_t& d, Op& op) {
  op = {};

  // scalars
  op.pc   = d.pc;
  op.kind = to_kind(d.insn_class);

  // branch/call meta
  if (is_branch_class(d.insn_class)) {
    // Prefer the field you already have; 
    // (next_pc != pc+4) was how << prints it
    op.taken  = d.is_taken;
    op.target = d.next_pc;
  } else {
    op.taken  = false;
    op.target = 0;
  }

  // memory meta
  if (d.insn_class == InstClass::loadInstClass || d.is_load ||
      d.insn_class == InstClass::storeInstClass || d.is_store) {
    op.ea   = d.addr;
    op.size = static_cast<uint32_t>(d.size);
  } else {
    op.ea = 0;
    op.size = 0;
  }

  // inputs A/B/C (in order), using log_reg/value
  auto push_in = [&](const db_operand_t& x){
    if (x.valid) op.inputs.push_back(RegRef{ static_cast<uint32_t>(x.log_reg),
                                             hex_lower(x.value) });
  };
  push_in(d.A);
  push_in(d.B);
  push_in(d.C);

  // output D
  if (d.D.valid) {
    op.output = RegRef{ static_cast<uint32_t>(d.D.log_reg),
                        hex_lower(d.D.value) };
  }
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
static inline void emit_aligned_asm_line(FILE* ofp,
                                         const std::string& raw,
                                         int indent_cols = 4,
                                         int comment_col = 20)
{
  const std::string indent(indent_cols, ' ');

  // Find the first "//"
  const std::size_t pos = raw.find("//");
  if (pos == std::string::npos) {
    // No comment: just indent + raw
    std::fputs(indent.c_str(), ofp);
    std::fputs(raw.c_str(), ofp);
    std::fputc('\n', ofp);
    return;
  }

  // Split left side (instr/operands) and comment
  std::string left = raw.substr(0, pos);
  // Trim trailing spaces from left
  while (!left.empty() && (left.back() == ' ' || left.back() == '\t'))
    left.pop_back();

  const std::string comment = raw.substr(pos); // includes leading "//"

  // Compute padding so that the '/' of '//' lands at column `comment_col`
  // Columns are counted from the start of the physical line, including indent.
  const int cols_before = indent_cols + static_cast<int>(left.size());
  int pad = comment_col - cols_before;
  if (pad < 1) pad = 1; // at least one space before the comment

  std::fputs(indent.c_str(), ofp);
  std::fputs(left.c_str(), ofp);
  for (int i = 0; i < pad; ++i) std::fputc(' ', ofp);
  std::fputs(comment.c_str(), ofp);
  std::fputc('\n', ofp);
}

// -----------------------------------------------------------------------------
// CBP to ASM
// -----------------------------------------------------------------------------
bool run_cbp_to_asm(const std::string& in,
                    const std::string& out,
                    uint64_t limit)
{
  TraceReader tr(in.c_str());

  FILE* ofp = nullptr;
  if (!out.empty()) {
    ofp = std::fopen(out.c_str(), "w");
    if (!ofp) {
      std::fprintf(stderr, "-E: run_cbp_to_asm Failed to open output: %s\n", 
                   out.c_str());
      return false;
    }
  } else {
    ofp = stdout;
  }

  uint64_t n = 0;

  std::fputs(".section .text\n", ofp);
  std::fputs(".global _start\n", ofp);
  std::fputs("\n", ofp);
  std::fputs("_start:\n", ofp);

  while (limit == ~0ULL || n < limit) {
    db_t* d = tr.get_inst();
    if(!d) break;

    Op op{};
    map_db_to_op(*d, op);

    const std::string line = format_asm_line(op);
    emit_aligned_asm_line(ofp, line, 4, 24);

    ++n;
  }

  if (ofp && ofp != stdout) std::fclose(ofp);
  return true;
}
