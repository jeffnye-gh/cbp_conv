#pragma once
#include <cstdint>
#include <vector>
#include <optional>
#include <algorithm>
#include <cassert>
#include <iostream>
#include "byte_reader.h"
#include "sim_common_structs.h" // from cbp2025 distro

// -----------------------------------------------------------------------------
// from CBP except the input source is an ArchiveByteReader
// (libarchive) instead of gzstream.
// -----------------------------------------------------------------------------
inline bool reg_is_int(uint8_t reg_offset) {
  constexpr uint8_t vecOffset  = 32;
  constexpr uint8_t ccOffset   = 64;
  constexpr uint8_t zeroOffset = 65;
  return (reg_offset < vecOffset)
      || (reg_offset == ccOffset)
      || (reg_offset == zeroOffset);
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
struct db_operand_t {
  bool valid{};
  bool is_int{};
  uint64_t log_reg{};
  uint64_t value{};
  void print() const { std::cout << *this; }
  friend std::ostream& operator<<(std::ostream& os, const db_operand_t& e){
    os << " (int: " << e.is_int << ", idx: " << e.log_reg
       << " val: " << std::hex << e.value << std::dec << ") ";
    return os;
  }
};

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
struct db_t {
  InstClass insn_class{};
  uint64_t pc{};
  bool is_taken{};
  uint64_t next_pc{};
  db_operand_t A,B,C,D;
  bool is_load{};
  bool is_store{};
  uint64_t addr{};
  uint64_t size{};
  bool is_last_piece{};
  friend std::ostream& operator<<(std::ostream& os, const db_t& e)
  {
    os << "[PC: 0x" << std::hex << e.pc << std::dec
       << " type: "  << cInfo[(uint8_t)e.insn_class];

    if (   e.insn_class == InstClass::loadInstClass
        || e.insn_class == InstClass::storeInstClass)
    {
      os << " ea: 0x" << std::hex << e.addr << std::dec << " size: " << e.size;
    }
    if (is_br(e.insn_class)) {
      os << " ( tkn:" << (e.next_pc != e.pc + 4)
         << " tar: 0x" << std::hex << e.next_pc << ") " << std::dec;
    }
    if (e.A.valid) os << " 1st input: " << e.A;
    if (e.B.valid) os << "2nd input: " << e.B;
    if (e.C.valid) os << "3rd input: " << e.C;
    if (e.D.valid) os << " output: "   << e.D;
    os << " ]"; return os;
  }
  void printInst(uint64_t cyc) const {
    std::cout << cyc<<"::uOP:: "<<*this<<std::endl;
  }
};

// -----------------------------------------------------------------------------
// Binary CBP reader (compatible with the sample trace layout).
// -----------------------------------------------------------------------------
struct TraceReader {

  struct Instr {
    uint64_t mPc{}, mNextPc{}, mEffAddr{};
    InstClass mType{InstClass::undefInstClass};
    bool mTaken{};
    uint8_t  mMemSize{}, mBaseUpd{}, mHasRegOffset{};
    uint8_t  mNumInRegs{}, mNumOutRegs{};
    std::vector<uint8_t> mInRegs, mOutRegs;
    std::optional<uint8_t> mBaseUpdReg;
    std::vector<uint64_t> mOutRegsValues;
    Instr(){ reset(); }
    void reset(){
      mPc = mNextPc = mEffAddr = 0xdeadbeefULL;
      mMemSize = mBaseUpd = mHasRegOffset = 0;
      mType = InstClass::undefInstClass;
      mTaken = false;
      mNumInRegs = mNumOutRegs = 0;
      mInRegs.clear(); mOutRegs.clear();
      mBaseUpdReg.reset();
      mOutRegsValues.clear();
    }

    bool capture_base_update_log_reg();
    friend std::ostream& operator<<(std::ostream& os, const Instr& x){
      os << "mOP:: [PC: 0x" << std::hex << x.mPc << std::dec  << " type: "
         << cInfo[(uint8_t)x.mType];
      if (x.mType == InstClass::loadInstClass || x.mType == InstClass::storeInstClass)
        os << " ea: 0x" << std::hex << x.mEffAddr << std::dec
           << " size: " << (uint64_t)x.mMemSize
           << " baseupdreg: " << (uint64_t)x.mBaseUpdReg.value_or(0xff);
      if (is_br(x.mType))
        os << " ( tkn:" << x.mTaken 
           << " tar: 0x" << std::hex << x.mNextPc << ") "<<std::dec;
      return os << " ]";
    }
  };

  explicit TraceReader(const char* path): nInstr(0) { rdr.open(path); }
  ~TraceReader(){ std::cout << " Read " << nInstr << " instrs " << std::endl; }

  db_t* get_inst();      // allocates a db_t* 
  bool  readInstr();     // fill mInstr from stream

  // internal state (matches the original)
  Instr mInstr;
  uint8_t mTotalPieces=0, mMemPieces=0, 
          mProcessedPieces=0, mCrackRegIdx=0,
          mCrackValIdx=0, mSizeFactor=0;
  uint64_t nInstr=0;
  uint8_t start_fp_reg=0;

private:
  ArchiveByteReader rdr;

  // helpers
  template<typename T>
  bool read_raw(T& v) {
    size_t got = rdr.read(&v, sizeof(T));
    return got == sizeof(T);
  }
  bool read_bytes(void* p, size_t n) {
    size_t got = rdr.read(p, n);
    return got == n;
  }

  db_t* populateNewInstr();
};

