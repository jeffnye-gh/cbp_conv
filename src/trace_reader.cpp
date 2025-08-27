#include "trace_reader.h"
#include <cstring>
#include <iterator>

static constexpr uint8_t vecOffset  = 32;
static constexpr uint8_t ccOffset   = 64;
static constexpr uint8_t ZeroOffset = 65;

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
bool TraceReader::Instr::capture_base_update_log_reg()
{
  if (!is_mem(mType)) return false;

  if (is_store(mType)) {
    if (mNumOutRegs == 1) { mBaseUpdReg.emplace(mOutRegs.at(0)); return true; }
    return false;
  }

  // load case
  if (mOutRegs.empty()) return false;

  if (mOutRegs.size() <= 1) return false;

  auto src = mInRegs, dst = mOutRegs;

  std::sort(src.begin(), src.end());
  std::sort(dst.begin(), dst.end());

  auto cut = std::lower_bound(src.begin(), src.end(), vecOffset);

  src.erase(cut, src.end());
  cut = std::lower_bound(dst.begin(), dst.end(), vecOffset);
  dst.erase(cut, dst.end());

  std::vector<uint8_t> overlap;
  std::set_intersection(src.begin(), src.end(),
                        dst.begin(), dst.end(), std::back_inserter(overlap));

  if (overlap.size() == 1) {
    if (mBaseUpd == 1) mBaseUpdReg.emplace(overlap[0]);
    return mBaseUpd == 1;
  }

  return false;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
db_t* TraceReader::populateNewInstr()
{
  db_t* inst = new db_t();
  const bool is_macro_mem = is_mem(mInstr.mType);
  const bool create_base_update_op =
      is_macro_mem && (mProcessedPieces>=1) && (mMemPieces == mProcessedPieces)
      && (mMemPieces == (mTotalPieces -1));

  inst->insn_class = create_base_update_op ? InstClass::aluInstClass : mInstr.mType;
  inst->pc = mInstr.mPc;
  inst->is_taken = mInstr.mTaken;
  inst->next_pc = mInstr.mNextPc;

  const bool base_upd_present = mInstr.mBaseUpdReg.has_value();
  const uint8_t base_upd_reg  = mInstr.mBaseUpdReg.value_or(0xff);

  // inputs
  uint8_t in_done = 0;
  if (create_base_update_op) {
    in_done++;
    inst->A.valid = true;
    inst->A.is_int = reg_is_int(base_upd_reg);
    inst->A.log_reg = base_upd_reg;
    inst->A.value = 0xdeadbeef;
    inst->B.valid = inst->C.valid = false;
  } else if (is_store(mInstr.mType)) {
    const uint8_t max_val_regs_per_piece = 1;
    // addr reg
    in_done++;
    inst->A.valid = true;
    inst->A.is_int = reg_is_int(mInstr.mInRegs[0]);
    inst->A.log_reg = mInstr.mInRegs[0];
    inst->A.value = 0xdeadbeef;

    const uint8_t val_off = 1 
                          + mInstr.mHasRegOffset
                          + mProcessedPieces*max_val_regs_per_piece;

    if (mInstr.mHasRegOffset) {
      // offset
      in_done++;
      inst->B.valid = true;
      inst->B.is_int = reg_is_int(mInstr.mInRegs[1]);
      inst->B.log_reg = mInstr.mInRegs[1];
      inst->B.value = 0xdeadbeef;
      // value if present
      if (val_off < mInstr.mNumInRegs) {
        in_done++;
        inst->C.valid = true;
        inst->C.is_int = reg_is_int(mInstr.mInRegs[val_off]);
        inst->C.log_reg = mInstr.mInRegs[val_off];
        inst->C.value = 0xdeadbeef;
      } else inst->C.valid = false;
    } else {
      if (val_off < mInstr.mNumInRegs) {
        in_done++;
        inst->B.valid = true;
        inst->B.is_int = reg_is_int(mInstr.mInRegs[val_off]);
        inst->B.log_reg = mInstr.mInRegs[val_off];
        inst->B.value = 0xdeadbeef;
        inst->C.valid = false;
      } else {
        inst->B.valid = inst->C.valid = false;
      }
    }
  } else {
    if (mInstr.mNumInRegs >= 1) {
      in_done++;
      inst->A.valid = true;
      inst->A.is_int = reg_is_int(mInstr.mInRegs[0]);
      inst->A.log_reg = mInstr.mInRegs[0];
      inst->A.value=0xdeadbeef;
    } else inst->A.valid = false;

    if (mInstr.mNumInRegs >= 2) {
      in_done++;
      inst->B.valid = true;
      inst->B.is_int = reg_is_int(mInstr.mInRegs[1]);
      inst->B.log_reg = mInstr.mInRegs[1];
      inst->B.value=0xdeadbeef;
    }
    else inst->B.valid = false;

    if (mInstr.mNumInRegs >= 3) {
      in_done++;
      inst->C.valid = true;
      inst->C.is_int = reg_is_int(mInstr.mInRegs[2]);
      inst->C.log_reg = mInstr.mInRegs[2];
      inst->C.value=0xdeadbeef; }
    else inst->C.valid = false;
  }

  // output
  if (create_base_update_op) {
    inst->D.valid = true; inst->D.is_int = reg_is_int(base_upd_reg);
    inst->D.log_reg = base_upd_reg;
    inst->D.value = *mInstr.mOutRegsValues.rbegin();
  } else if (!is_store(mInstr.mType) && mInstr.mNumOutRegs >= 1) {
    inst->D.valid = true;
    inst->D.is_int = reg_is_int(mInstr.mOutRegs[mCrackRegIdx]);
    inst->D.log_reg = mInstr.mOutRegs[mCrackRegIdx];
    inst->D.value   = mInstr.mOutRegsValues[mCrackValIdx];
    if (!inst->D.is_int) start_fp_reg++; else start_fp_reg = 0;
  } else {
    inst->D.valid = false; start_fp_reg = 0;
  }

  inst->is_load  = create_base_update_op ? false
                 : (mInstr.mType == InstClass::loadInstClass);
  inst->is_store = create_base_update_op ? false 
                 : (mInstr.mType == InstClass::storeInstClass);

  inst->addr = mInstr.mEffAddr + (mProcessedPieces * mSizeFactor);
  inst->size = std::max<uint64_t>(1, mSizeFactor);

  mProcessedPieces++;
  inst->is_last_piece = (mProcessedPieces == mTotalPieces);

  if (   mInstr.mNumOutRegs > mCrackRegIdx
      && !reg_is_int(mInstr.mOutRegs[mCrackRegIdx]))
  {
    mCrackValIdx++;
    if (start_fp_reg % 2 == 0) mCrackRegIdx++;
  } else {
    mCrackValIdx++;
    mCrackRegIdx++;
  }

  return inst;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
bool TraceReader::readInstr(){
  mInstr.reset();
  start_fp_reg = 0;

  if (!read_raw(mInstr.mPc)) return false; // EOF

  // reset bookkeeping
  mTotalPieces = mMemPieces = mProcessedPieces = 0;
  mSizeFactor = 1; mCrackRegIdx = mCrackValIdx = 0;
  mInstr.mNextPc = mInstr.mPc + 4;

  if (!read_raw(mInstr.mType)) return false;

  if (   mInstr.mType == InstClass::loadInstClass
      || mInstr.mType == InstClass::storeInstClass)
  {
    read_raw(mInstr.mEffAddr);
    read_raw(mInstr.mMemSize);
    read_raw(mInstr.mBaseUpd);
    if (mInstr.mType == InstClass::storeInstClass) read_raw(mInstr.mHasRegOffset);
  }

  if (is_br(mInstr.mType)) {
    read_raw(mInstr.mTaken);
    if (!is_cond_br(mInstr.mType)) { assert(mInstr.mTaken); }
    if (mInstr.mTaken) read_raw(mInstr.mNextPc);
  }

  read_raw(mInstr.mNumInRegs);
  for (uint8_t i=0;i<mInstr.mNumInRegs;i++) {
    uint8_t r;
    read_raw(r);
    mInstr.mInRegs.push_back(r);
  }

  read_raw(mInstr.mNumOutRegs);
  for (uint8_t i=0;i<mInstr.mNumOutRegs;i++) {
    uint8_t r;
    read_raw(r);
    mInstr.mOutRegs.push_back(r);
  }

  mTotalPieces = (mInstr.mNumOutRegs > 0) ? mInstr.mNumOutRegs : 1;

  const bool base_update_present = mInstr.capture_base_update_log_reg();

  uint8_t base_upd_pos = 0xff;
  uint64_t base_upd_val = ~0ULL;

  for (uint8_t i=0;i<mInstr.mNumOutRegs;i++) {
    uint64_t val;
    read_raw(val);
    const bool is_base = base_update_present
                       && mInstr.mBaseUpdReg.value() == mInstr.mOutRegs[i];
    if (is_base) {
      base_upd_pos = i; base_upd_val = val;
    } else {
      mInstr.mOutRegsValues.push_back(val);
      if (!reg_is_int(mInstr.mOutRegs[i])) {
        uint64_t hi;
        read_raw(hi);
        mInstr.mOutRegsValues.push_back(hi);
        if (hi != 0) mTotalPieces++;
      }
    }
  }

  const bool is_macro_mem = is_mem(mInstr.mType);

  if (base_update_present) {
    assert(is_macro_mem);
    if (mInstr.mOutRegs.size() > 1) {
      mInstr.mOutRegs.erase(mInstr.mOutRegs.begin()+base_upd_pos);
      mInstr.mOutRegs.push_back(mInstr.mBaseUpdReg.value());
    }
    mInstr.mOutRegsValues.push_back(base_upd_val);
  }

  if (is_store(mInstr.mType)) {
    const uint8_t str_val_regs = mInstr.mNumInRegs - (1 + mInstr.mHasRegOffset);
    uint8_t true_vals = (str_val_regs==0)? 1 : str_val_regs;
    assert(mInstr.mMemSize % true_vals == 0);
    mMemPieces = true_vals;
    mTotalPieces = mMemPieces + (base_update_present?1:0);
    mSizeFactor = mInstr.mMemSize / mMemPieces;
  } else if (is_load(mInstr.mType)) {
    mMemPieces = mTotalPieces - (base_update_present?1:0);
    assert(mMemPieces > 0);
    mSizeFactor = mInstr.mMemSize / mMemPieces;
  } else {
    mMemPieces = 0;
    mSizeFactor = 0;
  }

  nInstr++;
  if (nInstr % 5000000ULL == 0) std::cout << nInstr << " instrs " << std::endl;
  return true;
}
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
db_t* TraceReader::get_inst(){
  if (mProcessedPieces != mTotalPieces) return populateNewInstr();
  else if (readInstr())                 return populateNewInstr();
  else                                  return nullptr;
}

