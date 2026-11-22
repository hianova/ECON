/-
  ECON v1.0.0 — Pure Ring-0 Market Config, Schema ABI & Bid-Ask .fir Emitter (`import Flow`)
  ============================================================================================
  SSOT compliance: all Pure-GPR 2-Jet manifold / 512b-Schema / QTT-Morphism proofs &
  compilers live in domain-free `../FLOW`; formal economics, finance, actuarial & clearing
  theorems (`M1-M18`) live in `lean/EconCert.lean`.
  This root file absorbs and upgrades `../SNIPER` (`TAIFEX` `TXO`/`TXF`/`TMF` `320B .flow`
  + `64B` wire ABI + executable Bid-Ask/Tax/Fee Conversion & Reversal arbitrage `.fir`)
  with ZERO local theorems and ZERO FFI bindings.
-/
import Flow

namespace Econ

def version          : String := "1.0.0"
def econDomainTag    : UInt32 := 0x45434F4E -- "ECON"
def bEnterQ16        : Int    := 2 * Flow.q16One
def bExitQ16         : Int    := Flow.q16One / 2
def tmfMult          : Int    := 10
def taifexTaxDivisor : Int    := 50000
def wireMagic        : UInt64 := 0x45434F4E57495231 -- "ECONWIR1"

def regimeFlat : Flow.Rule where
  b0 := 0; b1 := Flow.q16One; b2 := 0; bExit := -bEnterQ16; bEnter := bEnterQ16
  dtMax := 50 * Flow.q16One; uGain := Flow.q16One; nextLo := 2; nextHi := 1; kanLock := 0x0007

def regimeLongHold : Flow.Rule where
  b0 := 0; b1 := Flow.q16One; b2 := Flow.q16One / 16; bExit := bExitQ16; bEnter := 500 * Flow.q16One
  dtMax := 50 * Flow.q16One; uGain := Flow.q16One; nextLo := 0; nextHi := 3; kanLock := 0x0001

def regimeShortHold : Flow.Rule where
  b0 := 0; b1 := Flow.q16One; b2 := 0; bExit := -500 * Flow.q16One; bEnter := -bExitQ16
  dtMax := 50 * Flow.q16One; uGain := Flow.q16One; nextLo := 3; nextHi := 0; kanLock := 0x0001

def regimeHalted : Flow.Rule where
  b0 := 0; b1 := Flow.q16One; b2 := 0; bExit := -500 * Flow.q16One; bEnter := 500 * Flow.q16One
  dtMax := 25 * Flow.q16One; uGain := 0; nextLo := 3; nextHi := 3; kanLock := 0x0009

def econCharts : List Flow.Rule := [regimeFlat, regimeLongHold, regimeShortHold, regimeHalted]
def econInitState : Flow.Cell64 := Flow.mintState 2 0 (initChart := 0) (initMode := 0x0007)

def wireFrameLayout : Flow.Schema.CellLayout64 where
  name      := "wire_frame"
  cTypeName := "econ_wire_frame_t"
  fields    := [
    { name := "magic",         bitOffset := 0,   ty := .u64 },
    { name := "recv_ns",       bitOffset := 64,  ty := .u64 },
    { name := "sym_packed56",  bitOffset := 128, ty := .u64 },
    { name := "wire_bid_tok8", bitOffset := 192, ty := .u64 },
    { name := "wire_ask_tok8", bitOffset := 256, ty := .u64 },
    { name := "strike_q16",    bitOffset := 320, ty := .i64 },
    { name := "inst_kind",     bitOffset := 384, ty := .u32 },
    { name := "quote_flags",   bitOffset := 416, ty := .u32 },
    { name := "seq_no",        bitOffset := 448, ty := .u64 }
  ]

def manifoldStateLayout : Flow.Schema.CellLayout64 where
  name      := "manifold_state"
  cTypeName := "econ_manifold_state_t"
  fields    := [
    { name := "syn_future_eq_q16",    bitOffset := 0,   ty := .i64 },
    { name := "conv_arb_edge_q16",    bitOffset := 64,  ty := .i64 },
    { name := "rev_arb_edge_q16",     bitOffset := 128, ty := .i64 },
    { name := "butterfly_convex_q16", bitOffset := 192, ty := .i64 },
    { name := "legacy_mid_res_q16",   bitOffset := 256, ty := .i64 },
    { name := "shadow_pos_lots",      bitOffset := 320, ty := .i64 },
    { name := "shadow_entry_q16",     bitOffset := 384, ty := .i64 },
    { name := "shadow_net_pnl_q16",   bitOffset := 448, ty := .i64 }
  ]

def econArenaSchema : Flow.Schema.ArenaSpec where
  cPrefix     := "econ"
  headerGuard := "ECON_RING0_ABI_H"
  totalCells  := 2
  regions     := [
    { name := "wire_frame",     baseSlot := 0, count := 1 },
    { name := "manifold_state", baseSlot := 1, count := 1 }
  ]
  layouts     := [wireFrameLayout, manifoldStateLayout]

def econBidAskArbGraph : Flow.Morphism.Graph :=
  let b := Flow.Morphism.Builder.init "econ_bidask_arb" .linear true
  let (b, kQ16)   := b.input "strike"   64
  let (b, cBid)   := b.input "c_bid"    64
  let (b, cAsk)   := b.input "c_ask"    64
  let (b, pBid)   := b.input "p_bid"    64
  let (b, pAsk)   := b.input "p_ask"    64
  let (b, futBid) := b.input "fut_bid"  64
  let (b, futAsk) := b.input "fut_ask"  64
  let (b, costQ)  := b.input "cost_q16" 64
  let (b, oneSh)  := b.constU64 "one_sh" 8 1
  -- Strict Conversion: syn_ask = K + C_ask - P_bid; conv_edge = F_bid - syn_ask - cost
  let (b, kcAsk)  := b.add "kc_ask"   64 kQ16 cAsk
  let (b, synAsk) := b.sub "syn_ask"  64 kcAsk pBid
  let (b, cRaw)   := b.sub "conv_raw" 64 futBid synAsk
  let (b, convE)  := b.sub "conv_edge" 64 cRaw costQ
  -- Strict Reversal: syn_bid = K + C_bid - P_ask; rev_edge = syn_bid - F_ask - cost
  let (b, kcBid)  := b.add "kc_bid"   64 kQ16 cBid
  let (b, synBid) := b.sub "syn_bid"  64 kcBid pAsk
  let (b, rRaw)   := b.sub "rev_raw"  64 synBid futAsk
  let (b, revE)   := b.sub "rev_edge" 64 rRaw costQ
  -- Legacy SNIPER mid-price residual: ((K + (C_b+C_a)>>1 - (P_b+P_a)>>1)) - ((F_b+F_a)>>1)
  let (b, cSum)   := b.add "c_sum"    64 cBid cAsk
  let (b, cMid)   := b.shrVar "c_mid" 64 cSum oneSh
  let (b, pSum)   := b.add "p_sum"    64 pBid pAsk
  let (b, pMid)   := b.shrVar "p_mid" 64 pSum oneSh
  let (b, fSum)   := b.add "f_sum"    64 futBid futAsk
  let (b, fMid)   := b.shrVar "f_mid" 64 fSum oneSh
  let (b, kcMid)  := b.add "kc_mid"   64 kQ16 cMid
  let (b, synMid) := b.sub "syn_mid"  64 kcMid pMid
  let (b, midRes) := b.sub "mid_res"  64 synMid fMid
  b.build [convE, revE, midRes, synMid]

def main (args : List String) : IO UInt32 := do
  let outDir : System.FilePath := match args with
    | d :: _ => System.FilePath.mk d
    | []     => System.FilePath.mk "build"
  IO.FS.createDirAll outDir
  Flow.emitBundle (outDir / "econ_taifex.flow") 2 econInitState econCharts regimeHalted 1
  if !(← econArenaSchema.emitC17HeaderChecked (outDir / "econ_ring0_abi.h")) then return 1
  if !(← Flow.Morphism.emitFirChecked (outDir / "econ_bidask_arb.fir") econBidAskArbGraph) then return 1
  IO.println s!"  [ECON-LEAN] Emitted 320B econ_taifex.flow, econ_ring0_abi.h & econ_bidask_arb.fir into {outDir} (0 local theorems)"
  return 0

end Econ

def main (args : List String) : IO UInt32 :=
  Econ.main args
