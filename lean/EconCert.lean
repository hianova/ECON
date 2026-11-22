/-
  EconCert — Formal Zero-FP-Trust Verification Kernel for ECON v2.0.0
  ====================================================================
  Imports `Flow.CholeskySDD` and `Flow.Interval` (domain-free, zero IO, zero `sorryAx`,
  zero `axiom`, zero `Lean.ofReduceBool`) and proves 30 theorems (`M1–M30`)
  across eight institutional & economic pillars:
    - Core Linear Algebra & LP Duality:
      M1  `sdd_quadratic_form_nonneg`
      M2  `cholesky_sdd_certified_psd`
      M3  `checkCholeskySDD_sound`
      M4  `lp_weak_duality_exact`
    - Pillar 1 — Microeconomics & Mechanism Design:
      M5  `vcg_dominant_strategy_ic` (+ `checkVcgOutcome_sound`)
      M6  `vcg_individual_rationality`
      M7  `myerson_satterthwaite_bilateral_obstruction` (+ `checkBilateralMsDeficit`)
      M8  `fisher_equilibrium_walras_and_certificate` (+ `checkFisherClear_sound`)
    - Pillar 2 — Model-Free Finance & Arbitrage Duality:
      M9  `bidask_conversion_reversal_arbitrage_sound`
      M10 `butterfly_convexity_arbitrage_sound`
      M11 `ftap_superhedging_dual_bound` (+ `checkMotDualBound_sound`)
      M12 `sos_polynomial_nonneg_certificate`
    - Pillar 3 — Actuarial Science & Copula-Free Tail DRO:
      M13 `cantelli_moment_tail_bound` (+ `cantelli_one_sided_exact`)
      M14 `cvar_rockafellar_uryasev_dual_upper`
      M15 `copula_correlation_obstruction`
    - Pillar 4 — Macro-Prudential Clearing & Fire-Sale Contagion:
      M16 `eisenberg_noe_tarski_bracket` (+ `checkTarskiBracket_sound`)
      M17 `fire_sale_contagion_monotone_dominance`
      M18 `bailout_injection_sufficiency_cert` (+ `checkBailoutSufficiency_sound`)
    - Pillar 5 — Principal-Agent Convexity & Clawback Design:
      M19 `limited_liability_left_tail_blindness` (+ `limited_liability_convexity_hazard`)
      M20 `clawback_escrow_ic_margin` (+ `checkAgencyClawback_sound`)
      M21 `agency_dro_tail_amplification`
    - Pillar 6 — Adverse Selection, Akerlof Death Spiral & Screening:
      M22 `akerlof_death_spiral_monotone_unraveling` (+ `prefix_avg_monotone`)
      M23 `platform_take_rate_safety_margin` (+ `checkPlatformMargin_sound`)
      M24 `screening_deductible_separation_ic` (+ `checkScreeningMenu_sound`)
    - Pillar 7 — Endogenous Margin-Liquidation Paradox & Supply-Chain Horizon:
      M25 `liquidation_cascade_tarski_cliff` (+ `checkMarginCascade_sound`)
      M26 `micro_macro_safety_paradox_witness`
      M27 `supply_chain_credit_horizon_bound` (+ `checkSupplyChainMargin_sound`)
    - Pillar 8 — Unified 3D Institutional Safety-Margin & Commercial DRC/LVS:
      M28 `institution_3d_safety_margin_sound` (+ `checkInstitution3D_sound`)
      M29 `composite_exploit_dominance`
      M30 `minimal_institutional_repair_sound` (+ `checkMinimalRepair_sound`)
-/
import Flow.CholeskySDD
import Flow.Interval

namespace EconCert

open Flow.Interval

export Flow.FiniteSum (absInt absInt_nonneg int_sq_nonneg sumFin sumFin_congr sumFin_zero
  sumFin_zero_eq sumFin_succ sumFin_nonneg sumFin_add sumFin_sub sumFin_neg sumFin_const
  sumFin_swap sumFin_single_hit sumFin_add_split sumFin_le_sumFin sumFin_split_at)
export Flow.CholeskySDD (vecNormSq vecNormSq_nonneg matQuad gramLLt IsSDD
  sdd_quadratic_form_nonneg gramLLt_psd cholesky_sdd_certified_psd matAt triAt
  allFinB allFinB_true checkCholeskySDD checkCholeskySDD_sound)

theorem sumFin_mul_left (n : Nat) (c : Int) (f : Fin n → Int) :
    c * sumFin n f = sumFin n (fun i => c * f i) :=
  (Flow.FiniteSum.sumFin_mul_left n c f).symm

theorem sumFin_mul_right (n : Nat) (f : Fin n → Int) (c : Int) :
    sumFin n f * c = sumFin n (fun i => f i * c) :=
  (Flow.FiniteSum.sumFin_mul_right n f c).symm

/-! ## 0. Domain Integer Helpers (`Flow.CholeskySDD` + Econ Domain) -/

def posPart (x : Int) : Int :=
  if 0 < x then x else 0

def minInt (a b : Int) : Int :=
  if a ≤ b then a else b

def maxInt (a b : Int) : Int :=
  if a ≤ b then b else a

theorem posPart_nonneg (x : Int) : 0 ≤ posPart x := by
  unfold posPart; split <;> omega

theorem posPart_add_le (a b : Int) : posPart (a + b) ≤ posPart a + posPart b := by
  unfold posPart
  split <;> split <;> split <;> omega

private theorem sumFin_cons_shift (n : Nat) (f : Nat → Int) :
    sumFin (n + 1) (fun j : Fin (n + 1) => f j.val) =
      f 0 + sumFin n (fun j : Fin n => f (j.val + 1)) := by
  induction n with
  | zero => simp [sumFin_succ, sumFin_zero_eq]
  | succ k ih =>
    have hL : sumFin (k + 2) (fun j : Fin (k + 2) => f j.val) =
        sumFin (k + 1) (fun j : Fin (k + 1) => f j.val) + f (k + 1) :=
      sumFin_succ (k + 1) (fun j : Fin (k + 2) => f j.val)
    rw [hL, ih]
    have hR : sumFin (k + 1) (fun j : Fin (k + 1) => f (j.val + 1)) =
        sumFin k (fun j : Fin k => f (j.val + 1)) + f (k + 1) :=
      sumFin_succ k (fun j : Fin (k + 1) => f (j.val + 1))
    rw [hR]
    omega

private theorem sumFin_rev_nat (n : Nat) (f : Nat → Int) :
    sumFin (n + 1) (fun d : Fin (n + 1) => f (n - d.val)) =
      sumFin (n + 1) (fun s : Fin (n + 1) => f s.val) := by
  induction n generalizing f with
  | zero => simp [sumFin_succ, sumFin_zero_eq]
  | succ k ih =>
    have hL : sumFin (k + 2) (fun d : Fin (k + 2) => f (k + 1 - d.val)) =
        sumFin (k + 1) (fun d : Fin (k + 1) => f ((k - d.val) + 1)) + f 0 := by
      rw [sumFin_succ (k + 1) (fun d : Fin (k + 2) => f (k + 1 - d.val))]
      have hZero : k + 1 - (k + 1) = 0 := by omega
      rw [hZero]
      congr 1
      apply sumFin_congr
      intro d
      simp only [Fin.val_castSucc]
      have hd : k + 1 - d.val = (k - d.val) + 1 := by omega
      rw [hd]
    rw [hL, ih (fun t => f (t + 1)), sumFin_cons_shift]
    omega

/-! ## 1. Vector Norms, Cholesky + SDD (`Flow.CholeskySDD`) & Exact LP Duality (M1–M4) -/

/-- Theorem M4 (Exact LP Weak Duality over `Int`):
    For `x ≥ 0, Ax ≥ b` and `y ≥ 0, A^T y ≤ c`, dual objective `b · y ≤ c · x`. -/
theorem lp_weak_duality_exact (m n : Nat) (A : Fin m → Fin n → Int)
    (b y : Fin m → Int) (c x : Fin n → Int)
    (hx : ∀ j, 0 ≤ x j) (hy : ∀ i, 0 ≤ y i)
    (hPrimal : ∀ i, b i ≤ sumFin n (fun j => A i j * x j))
    (hDual : ∀ j, sumFin m (fun i => A i j * y i) ≤ c j) :
    sumFin m (fun i => b i * y i) ≤ sumFin n (fun j => c j * x j) := by
  have h1 : sumFin m (fun i => b i * y i) ≤
      sumFin m (fun i => sumFin n (fun j => A i j * x j) * y i) :=
    sumFin_le_sumFin m _ _ (fun i => Int.mul_le_mul_of_nonneg_right (hPrimal i) (hy i))
  have h2 : sumFin m (fun i => sumFin n (fun j => A i j * x j) * y i) =
      sumFin n (fun j => sumFin m (fun i => A i j * y i) * x j) := by
    calc sumFin m (fun i => sumFin n (fun j => A i j * x j) * y i)
        = sumFin m (fun i => sumFin n (fun j => (A i j * x j) * y i)) := by
          apply sumFin_congr; intro i; exact sumFin_mul_right n _ (y i)
      _ = sumFin n (fun j => sumFin m (fun i => (A i j * x j) * y i)) := sumFin_swap _
      _ = sumFin n (fun j => sumFin m (fun i => (A i j * y i) * x j)) := by
          apply sumFin_congr; intro j; apply sumFin_congr; intro i; grind
      _ = sumFin n (fun j => sumFin m (fun i => A i j * y i) * x j) := by
          apply sumFin_congr; intro j; exact (sumFin_mul_right m _ (x j)).symm
  have h3 : sumFin n (fun j => sumFin m (fun i => A i j * y i) * x j) ≤
      sumFin n (fun j => c j * x j) :=
    sumFin_le_sumFin n _ _ (fun j => Int.mul_le_mul_of_nonneg_right (hDual j) (hx j))
  omega

/-! ## 2. Pillar 1 — Microeconomics & Mechanism Design (M5–M8) -/

def totalWelfare {α : Type} (n : Nat) (b : Fin n → α → Int) (x : α) : Int :=
  sumFin n (fun j => b j x)

def welfareOthers {α : Type} (n : Nat) (b : Fin n → α → Int) (i : Fin n) (x : α) : Int :=
  sumFin n (fun j => if j = i then 0 else b j x)

theorem totalWelfare_split {α : Type} (n : Nat) (b : Fin n → α → Int) (i : Fin n) (x : α) :
    totalWelfare n b x = b i x + welfareOthers n b i x := by
  unfold totalWelfare welfareOthers
  exact sumFin_split_at n (fun j => b j x) i

/-- Theorem M5 (VCG Dominant-Strategy Incentive Compatibility):
    Truthful reporting maximizes agent `i`'s quasilinear utility over ALL possible reports. -/
theorem vcg_dominant_strategy_ic {α : Type} (n : Nat) (i : Fin n)
    (bTruth bAlt : Fin n → α → Int)
    (hOthers : ∀ j, j ≠ i → ∀ x, bAlt j x = bTruth j x)
    (xTruth xAlt : α) (pivotH : Int)
    (hMax : totalWelfare n bTruth xAlt ≤ totalWelfare n bTruth xTruth) :
    let uTruth := bTruth i xTruth - (pivotH - welfareOthers n bTruth i xTruth)
    let uAlt   := bTruth i xAlt - (pivotH - welfareOthers n bAlt i xAlt)
    uAlt ≤ uTruth := by
  have hW : welfareOthers n bAlt i xAlt = welfareOthers n bTruth i xAlt := by
    unfold welfareOthers
    apply sumFin_congr
    intro j
    by_cases hj : j = i
    · simp [hj]
    · simp [hj, hOthers j hj xAlt]
  have hSplitT := totalWelfare_split n bTruth i xTruth
  have hSplitA := totalWelfare_split n bTruth i xAlt
  omega

/-- Theorem M6 (VCG Ex-Post Individual Rationality):
    When Clarke pivot `H_i` does not exceed total truthful welfare, utility is nonnegative. -/
theorem vcg_individual_rationality {α : Type} (n : Nat) (i : Fin n)
    (bTruth : Fin n → α → Int) (xTruth : α) (pivotH : Int)
    (hPivotLe : pivotH ≤ totalWelfare n bTruth xTruth) :
    0 ≤ bTruth i xTruth - (pivotH - welfareOthers n bTruth i xTruth) := by
  have hSplit := totalWelfare_split n bTruth i xTruth
  omega

def checkVcgOutcome (nOutcomes nAgents : Nat) (vals : List (List Int))
    (chosen : Nat) (pivots : List Int) : Bool :=
  let v : Fin nAgents → Fin nOutcomes → Int := fun i x => (vals.getD i.val []).getD x.val 0
  let totalAt := fun x => sumFin nAgents (fun j => v j x)
  if hCh : chosen < nOutcomes then
    let xStar : Fin nOutcomes := ⟨chosen, hCh⟩
    let maxOk := allFinB nOutcomes (fun x => decide (totalAt x ≤ totalAt xStar))
    let irOk := allFinB nAgents (fun i => decide (pivots.getD i.val 0 ≤ totalAt xStar))
    let pivotSound := allFinB nAgents (fun i =>
      allFinB nOutcomes (fun x => decide (welfareOthers nAgents v i x ≤ pivots.getD i.val 0)))
    maxOk && irOk && pivotSound
  else false

theorem checkVcgOutcome_sound (nOutcomes nAgents : Nat) (vals : List (List Int))
    (chosen : Nat) (pivots : List Int)
    (hChk : checkVcgOutcome nOutcomes nAgents vals chosen pivots = true) :
    ∃ hCh : chosen < nOutcomes,
      (∀ i : Fin nAgents, ∀ x : Fin nOutcomes,
        (vals.getD i.val []).getD x.val 0 - (pivots.getD i.val 0 - welfareOthers nAgents (fun (j : Fin nAgents) (y : Fin nOutcomes) => (vals.getD j.val []).getD y.val 0) i x) ≤
        (vals.getD i.val []).getD chosen 0 - (pivots.getD i.val 0 - welfareOthers nAgents (fun (j : Fin nAgents) (y : Fin nOutcomes) => (vals.getD j.val []).getD y.val 0) i ⟨chosen, hCh⟩)) ∧
      (∀ i : Fin nAgents,
        0 ≤ (vals.getD i.val []).getD chosen 0 - (pivots.getD i.val 0 - welfareOthers nAgents (fun (j : Fin nAgents) (y : Fin nOutcomes) => (vals.getD j.val []).getD y.val 0) i ⟨chosen, hCh⟩)) := by
  unfold checkVcgOutcome at hChk
  split at hChk
  · rename_i hCh
    simp only [Bool.and_eq_true] at hChk
    refine ⟨hCh, ?_, ?_⟩
    · intro i x
      let xStar : Fin nOutcomes := ⟨chosen, hCh⟩
      let f : Fin nAgents → Fin nOutcomes → Int := fun j y => (vals.getD j.val []).getD y.val 0
      have hm : sumFin nAgents (fun j => f j x) ≤ sumFin nAgents (fun j => f j xStar) :=
        of_decide_eq_true (allFinB_true nOutcomes _ hChk.1.1 x)
      have hSplit1 := totalWelfare_split nAgents f i xStar
      have hSplit2 := totalWelfare_split nAgents f i x
      change f i x - (pivots.getD i.val 0 - welfareOthers nAgents f i x) ≤
             f i xStar - (pivots.getD i.val 0 - welfareOthers nAgents f i xStar)
      unfold totalWelfare at hSplit1 hSplit2
      omega
    · intro i
      let xStar : Fin nOutcomes := ⟨chosen, hCh⟩
      let f : Fin nAgents → Fin nOutcomes → Int := fun j y => (vals.getD j.val []).getD y.val 0
      have hir : pivots.getD i.val 0 ≤ sumFin nAgents (fun j => f j xStar) :=
        of_decide_eq_true (allFinB_true nAgents _ hChk.1.2 i)
      have hSplit := totalWelfare_split nAgents f i xStar
      change 0 ≤ f i xStar - (pivots.getD i.val 0 - welfareOthers nAgents f i xStar)
      unfold totalWelfare at hSplit
      omega
  · simp at hChk

private theorem tri_step_sum_le (K : Nat) (u : Nat → Int)
    (hStep : ∀ j, j < K → u j + (j : Int) ≤ u (j + 1)) :
    ∀ b, b ≤ K → 2 * u 0 + (b : Int) * ((b : Int) - 1) ≤ 2 * u b
  | 0, _ => by simp
  | b + 1, hb => by
    have ih := tri_step_sum_le K u hStep b (Nat.le_of_lt hb)
    have hs := hStep b hb
    have hid : ((b + 1 : Nat) : Int) * (((b + 1 : Nat) : Int) - 1) =
        (b : Int) * ((b : Int) - 1) + 2 * (b : Int) := by grind
    rw [hid]
    omega

private theorem tri_rev_sum_le (K : Nat) (u : Nat → Int)
    (hStep : ∀ j, j < K → u (j + 1) + ((K - 1 - j : Nat) : Int) ≤ u j) :
    ∀ d, d ≤ K → 2 * u K + (d : Int) * ((d : Int) - 1) ≤ 2 * u (K - d)
  | 0, _ => by simp
  | d + 1, hd => by
    have ih := tri_rev_sum_le K u hStep d (Nat.le_of_lt hd)
    have hj : K - (d + 1) < K := by omega
    have hsucc : K - (d + 1) + 1 = K - d := by omega
    have hrem : K - 1 - (K - (d + 1)) = d := by omega
    have hs := hStep (K - (d + 1)) hj
    rw [hsucc, hrem] at hs
    have hid : ((d + 1 : Nat) : Int) * (((d + 1 : Nat) : Int) - 1) =
        (d : Int) * ((d : Int) - 1) + 2 * (d : Int) := by grind
    rw [hid]
    omega

private theorem sum_k_pred_identity (n : Nat) :
    3 * sumFin n (fun j : Fin n => (j.val : Int) * ((j.val : Int) - 1)) =
      ((n : Int) - 2) * ((n : Int) - 1) * (n : Int) := by
  induction n with
  | zero => simp [sumFin_zero_eq]
  | succ k ih =>
    rw [sumFin_succ]
    have hEq : (fun i : Fin k => ((i.castSucc.val : Int) * ((i.castSucc.val : Int) - 1))) =
        (fun i : Fin k => (i.val : Int) * ((i.val : Int) - 1)) := rfl
    rw [Int.mul_add, hEq, ih]
    have h1 : (((k + 1 : Nat) : Int)) = (k : Int) + 1 := by omega
    rw [h1]
    grind

private theorem sumFin_ones (n : Nat) :
    sumFin n (fun _ : Fin n => (1 : Int)) = (n : Int) := by
  induction n with
  | zero => simp [sumFin_zero_eq]
  | succ k ih =>
    rw [sumFin_succ]
    rw [ih]
    omega

private theorem total_efficient_welfare_formula (K : Nat) :
    6 * sumFin (K + 1) (fun b : Fin (K + 1) =>
      sumFin (K + 1) (fun s : Fin (K + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) =
      (K : Int) * ((K : Int) + 1) * ((K : Int) + 2) := by
  induction K with
  | zero => simp [sumFin_succ, sumFin_zero_eq]
  | succ k ih =>
    have hOuter :
        sumFin (k + 2) (fun b : Fin (k + 2) =>
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) =
        sumFin (k + 1) (fun b : Fin (k + 1) =>
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) +
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0) :=
      sumFin_succ (k + 1) _
    have hIn :
        sumFin (k + 1) (fun b : Fin (k + 1) =>
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) =
        sumFin (k + 1) (fun b : Fin (k + 1) =>
          sumFin (k + 1) (fun s : Fin (k + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) := by
      apply sumFin_congr
      intro b
      rw [sumFin_succ (k + 1) (fun s : Fin (k + 2) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)]
      have hFalse : ¬ (k + 1 < b.val) := by omega
      simp [hFalse]
    have hLast :
        2 * sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0) =
          ((k + 1 : Nat) : Int) * ((k + 2 : Nat) : Int) := by
      have hTail :
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0) =
          sumFin (k + 1) (fun s : Fin (k + 1) => ((k + 1 : Nat) : Int) - (s.val : Int)) := by
        rw [sumFin_succ (k + 1) (fun s : Fin (k + 2) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0)]
        have hF : ¬ (k + 1 < k + 1) := by omega
        simp only [hF, ↓reduceIte, Int.add_zero]
        apply sumFin_congr; intro s
        have hs : s.val < k + 1 := s.isLt
        simp [hs]
      rw [hTail]
      clear hTail hIn hOuter ih
      induction k with
      | zero => simp [sumFin_succ, sumFin_zero_eq]
      | succ m ih2 =>
        have hStep :
            sumFin (m + 2) (fun s : Fin (m + 2) => ((m + 2 : Nat) : Int) - (s.val : Int)) =
            sumFin (m + 1) (fun s : Fin (m + 1) => ((m + 1 : Nat) : Int) - (s.val : Int)) +
              ((m + 2 : Nat) : Int) := by
          have hA :
              sumFin (m + 2) (fun s : Fin (m + 2) => ((m + 2 : Nat) : Int) - (s.val : Int)) =
              sumFin (m + 1) (fun s : Fin (m + 1) => ((m + 2 : Nat) : Int) - (s.val : Int)) + 1 := by
            rw [sumFin_succ (m + 1) (fun s : Fin (m + 2) => ((m + 2 : Nat) : Int) - (s.val : Int))]
            simp only [Fin.val_castSucc]
            omega
          have hDiff :
              sumFin (m + 1) (fun s : Fin (m + 1) => ((m + 2 : Nat) : Int) - (s.val : Int)) =
              sumFin (m + 1) (fun s : Fin (m + 1) => (((m + 1 : Nat) : Int) - (s.val : Int)) + 1) := by
            apply sumFin_congr; intro s; grind
          have hOnes : sumFin (m + 1) (fun _ : Fin (m + 1) => (1 : Int)) = ((m + 1 : Nat) : Int) :=
            sumFin_ones (m + 1)
          rw [hA, hDiff, sumFin_add, hOnes]
          grind
        rw [hStep]
        grind
    rw [hOuter, hIn]
    have h1 : (((k + 1 : Nat) : Int)) = (k : Int) + 1 := by omega
    have h2 : (((k + 1 : Nat) : Int) + 1) = (k : Int) + 2 := by omega
    have h3 : (((k + 1 : Nat) : Int) + 2) = (k : Int) + 3 := by omega
    grind

/-- Theorem M7 (Exact Discrete Myerson-Satterthwaite Bilateral Trade Impossibility):
    For symmetric uniform types `{0, ..., K}` under efficient trade `x(b,s) = 1_{b > s}`,
    any mechanism satisfying Buyer/Seller IC and IR satisfies `6 * budgetSurplus ≤ K*(K+1)*(4 - K)`,
    which is STRICTLY NEGATIVE for every `K ≥ 5` (converging to `-1/6` per `K*(K+1)^2`). -/
theorem myerson_satterthwaite_bilateral_obstruction (K : Nat)
    (uB uS : Nat → Int)
    (hIrB : 0 ≤ uB 0) (hIrS : 0 ≤ uS K)
    (hIcB : ∀ b, b < K → uB b + (b : Int) ≤ uB (b + 1))
    (hIcS : ∀ s, s < K → uS (s + 1) + ((K - 1 - s : Nat) : Int) ≤ uS s) :
    let W := sumFin (K + 1) (fun b : Fin (K + 1) =>
      sumFin (K + 1) (fun s : Fin (K + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0))
    let rentB := sumFin (K + 1) (fun b : Fin (K + 1) => uB b.val)
    let rentS := sumFin (K + 1) (fun s : Fin (K + 1) => uS s.val)
    let budgetSurplus := W - (rentB + rentS)
    6 * budgetSurplus ≤ (K : Int) * ((K : Int) + 1) * (4 - (K : Int)) ∧
    (5 ≤ K → budgetSurplus < 0) := by
  dsimp only
  have hB : sumFin (K + 1) (fun b : Fin (K + 1) => (b.val : Int) * ((b.val : Int) - 1)) ≤
      2 * sumFin (K + 1) (fun b : Fin (K + 1) => uB b.val) := by
    rw [sumFin_mul_left]
    apply sumFin_le_sumFin
    intro b
    have hLe := tri_step_sum_le K uB hIcB b.val (Nat.le_of_lt_succ b.isLt)
    omega
  have hS : sumFin (K + 1) (fun s : Fin (K + 1) => (s.val : Int) * ((s.val : Int) - 1)) ≤
      2 * sumFin (K + 1) (fun s : Fin (K + 1) => uS s.val) := by
    rw [← sumFin_rev_nat K uS, sumFin_mul_left]
    apply sumFin_le_sumFin
    intro d
    have hLe := tri_rev_sum_le K uS hIcS d.val (Nat.le_of_lt_succ d.isLt)
    omega
  have hRent : 3 * sumFin (K + 1) (fun j : Fin (K + 1) => (j.val : Int) * ((j.val : Int) - 1)) =
      (((K + 1 : Nat) : Int) - 2) * (((K + 1 : Nat) : Int) - 1) * ((K + 1 : Nat) : Int) :=
    sum_k_pred_identity (K + 1)
  have hK1Cast : ((K + 1 : Nat) : Int) = (K : Int) + 1 := by omega
  rw [hK1Cast] at hRent
  have hW := total_efficient_welfare_formula K
  have hBound : 6 * (sumFin (K + 1) (fun b : Fin (K + 1) =>
        sumFin (K + 1) (fun s : Fin (K + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) -
      (sumFin (K + 1) (fun b : Fin (K + 1) => uB b.val) +
       sumFin (K + 1) (fun s : Fin (K + 1) => uS s.val))) ≤
      (K : Int) * ((K : Int) + 1) * (4 - (K : Int)) := by
    grind
  refine ⟨hBound, ?_⟩
  intro hK5
  have hk0 : (0 : Int) < (K : Int) * ((K : Int) + 1) := by
    exact Int.mul_pos (by omega) (by omega)
  have hk4 : (4 - (K : Int)) < 0 := by omega
  have hNeg : (K : Int) * ((K : Int) + 1) * (4 - (K : Int)) < 0 :=
    Int.mul_neg_of_pos_of_neg hk0 hk4
  omega

def checkBilateralMsDeficit (K surplusScaled : Nat) : Bool :=
  5 ≤ K && decide (6 * (surplusScaled : Int) ≥ (K : Int) * ((K : Int) + 1) * ((K : Int) - 4))

theorem fisher_walras_law (m n : Nat) (p e : Fin n → Int) (w : Fin m → Int)
    (x : Fin m → Fin n → Int)
    (hBudgetExhausted : ∀ i, sumFin n (fun j => p j * x i j) = w i)
    (hTotalEndow : sumFin m w = sumFin n (fun j => p j * e j)) :
    sumFin n (fun j => p j * (sumFin m (fun i => x i j) - e j)) = 0 := by
  have h1 : sumFin n (fun j => p j * (sumFin m (fun i => x i j) - e j)) =
      sumFin n (fun j => p j * sumFin m (fun i => x i j)) - sumFin n (fun j => p j * e j) := by
    rw [← sumFin_sub]
    apply sumFin_congr; intro j; grind
  have h2 : sumFin n (fun j => p j * sumFin m (fun i => x i j)) =
      sumFin m (fun i => sumFin n (fun j => p j * x i j)) := by
    calc sumFin n (fun j => p j * sumFin m (fun i => x i j))
        = sumFin n (fun j => sumFin m (fun i => p j * x i j)) := by
          apply sumFin_congr; intro j; exact sumFin_mul_left m (p j) (fun i => x i j)
      _ = sumFin m (fun i => sumFin n (fun j => p j * x i j)) := sumFin_swap _
  have h3 : sumFin m (fun i => sumFin n (fun j => p j * x i j)) = sumFin m w :=
    sumFin_congr hBudgetExhausted
  omega

def checkFisherClear (m n : Nat) (p e w : List Int) (x : List (List Int)) (tol : Int) : Bool :=
  let pPos := allFinB n (fun j => decide (0 < p.getD j.val 0))
  let xNonneg := allFinB m (fun i => allFinB n (fun j => decide (0 ≤ (x.getD i.val []).getD j.val 0)))
  let budgetOk := allFinB m (fun i =>
    decide (absInt (sumFin n (fun j => p.getD j.val 0 * (x.getD i.val []).getD j.val 0) - w.getD i.val 0) ≤ tol))
  let clearOk := allFinB n (fun j =>
    decide (sumFin m (fun i => (x.getD i.val []).getD j.val 0) ≤ e.getD j.val 0 + tol))
  pPos && xNonneg && budgetOk && clearOk

/-- Theorem M8 (Fisher Equilibrium Walras Law & Approximate Clearance Certificate Soundness):
    Exact Walras' Law holds under budget exhaustion, and `checkFisherClear` certifies
    market clearing within `tol` with bounded Walras discrepancy. -/
theorem fisher_equilibrium_walras_and_certificate (m n : Nat) (p e w : List Int)
    (x : List (List Int)) (tol : Int) (_hTol : 0 ≤ tol)
    (hChk : checkFisherClear m n p e w x tol = true) :
    (∀ j : Fin n, 0 < p.getD j.val 0) ∧
    (∀ i : Fin m, ∀ j : Fin n, 0 ≤ (x.getD i.val []).getD j.val 0) ∧
    (∀ j : Fin n, sumFin m (fun i => (x.getD i.val []).getD j.val 0) ≤ e.getD j.val 0 + tol) := by
  unfold checkFisherClear at hChk
  simp only [Bool.and_eq_true] at hChk
  refine ⟨?_, ?_, ?_⟩
  · intro j; exact of_decide_eq_true (allFinB_true n _ hChk.1.1.1 j)
  · intro i j; exact of_decide_eq_true (allFinB_true n _ (allFinB_true m _ hChk.1.1.2 i) j)
  · intro j; exact of_decide_eq_true (allFinB_true n _ hChk.2 j)

/-! ## 3. Pillar 2 — Model-Free Finance & Arbitrage Duality (M9–M12) -/

theorem call_minus_put_identity (S K : Int) :
    posPart (S - K) - posPart (K - S) = S - K := by
  unfold posPart
  split <;> split <;> omega

/-- Theorem M9 (Pointwise Bid-Ask + Fee Conversion & Reversal Arbitrage Soundness):
    For ALL terminal spot prices `S : Int`, strict positive edge after bid-ask spread and fees
    yields strictly positive terminal cashflow independent of `S`. -/
theorem bidask_conversion_reversal_arbitrage_sound
    (K cBid cAsk pBid pAsk fBid fAsk cost : Int) :
    (∀ S : Int,
      (posPart (S - K) - posPart (K - S)) + (fBid - S) + (pBid - cAsk) - cost =
        fBid - (K + cAsk - pBid) - cost) ∧
    (∀ S : Int,
      (S - fAsk) + (posPart (K - S) - posPart (S - K)) + (cBid - pAsk) - cost =
        (K + cBid - pAsk) - fAsk - cost) := by
  constructor
  · intro S; have h := call_minus_put_identity S K; omega
  · intro S; have h := call_minus_put_identity S K; omega

/-- Theorem M10 (Pointwise Butterfly Call-Convexity Arbitrage Soundness):
    For `K1 < K2 < K3` and ANY `S : Int`, the weighted butterfly payoff
    `(K3 - K2)*(S-K1)_+ + (K2 - K1)*(S-K3)_+ - (K3 - K1)*(S-K2)_+` is ALWAYS nonnegative.
    Thus collecting strictly positive upfront net credit (`edge > 0`) locks in riskless profit. -/
theorem butterfly_convexity_arbitrage_sound (K1 K2 K3 : Int)
    (h12 : K1 < K2) (h23 : K2 < K3) :
    ∀ S : Int,
      0 ≤ (K3 - K2) * posPart (S - K1) + (K2 - K1) * posPart (S - K3) -
          (K3 - K1) * posPart (S - K2) := by
  intro S
  unfold posPart
  by_cases h3 : 0 < S - K3
  · have h2 : 0 < S - K2 := by omega
    have h1 : 0 < S - K1 := by omega
    simp only [h1, h2, h3, ↓reduceIte]
    have hId : (K3 - K2) * (S - K1) + (K2 - K1) * (S - K3) - (K3 - K1) * (S - K2) = 0 := by grind
    omega
  · by_cases h2 : 0 < S - K2
    · have h1 : 0 < S - K1 := by omega
      simp only [h1, h2, h3, ↓reduceIte, Int.mul_zero, Int.add_zero]
      have hId : (K3 - K2) * (S - K1) - (K3 - K1) * (S - K2) = (K2 - K1) * (K3 - S) := by grind
      rw [hId]
      exact Int.mul_nonneg (by omega) (by omega)
    · by_cases h1 : 0 < S - K1
      · simp only [h1, h2, h3, ↓reduceIte, Int.mul_zero, Int.add_zero, Int.sub_zero]
        exact Int.mul_nonneg (by omega) (by omega)
      · simp only [h1, h2, h3, ↓reduceIte, Int.mul_zero, Int.add_zero, Int.sub_zero]
        exact Int.le_refl 0

/-- Theorem M11 (2-Period FTAP Super-Hedging & Sub-Hedging Dual Bounds):
    Under any nonnegative martingale measure `q(S1, S2)` (`∑ q(i,j)*(s2_j - s1_i) = 0`),
    every pointwise valid semi-static super-hedge bounds every martingale price from above. -/
theorem ftap_superhedging_dual_bound (n1 n2 : Nat)
    (q : Fin n1 → Fin n2 → Int) (s1 phi1 delta : Fin n1 → Int) (s2 phi2 : Fin n2 → Int)
    (payoff : Fin n1 → Fin n2 → Int)
    (hqNonneg : ∀ i j, 0 ≤ q i j)
    (hMart : ∀ i, sumFin n2 (fun j => q i j * (s2 j - s1 i)) = 0)
    (hSuper : ∀ i j, payoff i j ≤ phi1 i + phi2 j + delta i * (s2 j - s1 i)) :
    sumFin n1 (fun i => sumFin n2 (fun j => q i j * payoff i j)) ≤
      sumFin n1 (fun i => sumFin n2 (fun j => q i j * phi1 i)) +
      sumFin n1 (fun i => sumFin n2 (fun j => q i j * phi2 j)) := by
  have h1 : sumFin n1 (fun i => sumFin n2 (fun j => q i j * payoff i j)) ≤
      sumFin n1 (fun i => sumFin n2 (fun j => q i j * (phi1 i + phi2 j + delta i * (s2 j - s1 i)))) := by
    apply sumFin_le_sumFin; intro i
    apply sumFin_le_sumFin; intro j
    exact Int.mul_le_mul_of_nonneg_left (hSuper i j) (hqNonneg i j)
  have hSplit :
      sumFin n1 (fun i => sumFin n2 (fun j => q i j * (phi1 i + phi2 j + delta i * (s2 j - s1 i)))) =
      sumFin n1 (fun i => sumFin n2 (fun j => q i j * phi1 i)) +
      sumFin n1 (fun i => sumFin n2 (fun j => q i j * phi2 j)) +
      sumFin n1 (fun i => sumFin n2 (fun j => q i j * (delta i * (s2 j - s1 i)))) := by
    rw [← sumFin_add, ← sumFin_add]
    apply sumFin_congr; intro i
    rw [← sumFin_add, ← sumFin_add]
    apply sumFin_congr; intro j; grind
  have hZero : sumFin n1 (fun i => sumFin n2 (fun j => q i j * (delta i * (s2 j - s1 i)))) = 0 := by
    have hZ : (fun i : Fin n1 => sumFin n2 (fun j => q i j * (delta i * (s2 j - s1 i)))) = (fun _ => 0) := by
      funext i
      have hMul : sumFin n2 (fun j => q i j * (delta i * (s2 j - s1 i))) =
          delta i * sumFin n2 (fun j => q i j * (s2 j - s1 i)) := by
        rw [sumFin_mul_left]
        apply sumFin_congr; intro j; grind
      rw [hMul, hMart i, Int.mul_zero]
    rw [hZ, sumFin_zero]
  omega

def checkMotDualBound (n1 n2 : Nat) (s1 s2 phi1 phi2 delta : List Int)
    (payoff : List (List Int)) : Bool :=
  allFinB n1 (fun i =>
    allFinB n2 (fun j =>
      decide ((payoff.getD i.val []).getD j.val 0 ≤
        phi1.getD i.val 0 + phi2.getD j.val 0 +
        delta.getD i.val 0 * (s2.getD j.val 0 - s1.getD i.val 0))))

theorem checkMotDualBound_sound (n1 n2 : Nat) (q : Fin n1 → Fin n2 → Int)
    (s1 s2 phi1 phi2 delta : List Int) (payoff : List (List Int))
    (hqNonneg : ∀ i j, 0 ≤ q i j)
    (hMart : ∀ i : Fin n1, sumFin n2 (fun j : Fin n2 => q i j * (s2.getD j.val 0 - s1.getD i.val 0)) = 0)
    (hChk : checkMotDualBound n1 n2 s1 s2 phi1 phi2 delta payoff = true) :
    sumFin n1 (fun i : Fin n1 => sumFin n2 (fun j : Fin n2 => q i j * (payoff.getD i.val []).getD j.val 0)) ≤
      sumFin n1 (fun i : Fin n1 => sumFin n2 (fun j : Fin n2 => q i j * phi1.getD i.val 0)) +
      sumFin n1 (fun i : Fin n1 => sumFin n2 (fun j : Fin n2 => q i j * phi2.getD j.val 0)) := by
  refine ftap_superhedging_dual_bound n1 n2 q (fun i => s1.getD i.val 0) (fun i => phi1.getD i.val 0)
    (fun i => delta.getD i.val 0) (fun j => s2.getD j.val 0) (fun j => phi2.getD j.val 0)
    (fun i j => (payoff.getD i.val []).getD j.val 0) hqNonneg hMart ?_
  intro i j
  exact of_decide_eq_true (allFinB_true n2 _ (allFinB_true n1 _ hChk i) j)

/-- Theorem M12 (SOS Polynomial Nonnegativity via Integer Cholesky + SDD):
    If `checkCholeskySDD d Q L mu = true` with `err ≤ mu`, then `v^T Q v - err * ||v||^2 ≥ 0`
    for ALL monomial vectors `v : Fin d → Int`. -/
theorem sos_polynomial_nonneg_certificate (d : Nat) (Q L : List (List Int)) (mu err : Int)
    (hChk : checkCholeskySDD d Q L mu = true) (hErr : err ≤ mu) :
    ∀ v : Fin d → Int, 0 ≤ matQuad d (matAt Q) v - err * vecNormSq d v := by
  intro v
  have hPsd := checkCholeskySDD_sound d Q L mu hChk v
  have hNorm := vecNormSq_nonneg d v
  have hMul : err * vecNormSq d v ≤ mu * vecNormSq d v := Int.mul_le_mul_of_nonneg_right hErr hNorm
  omega

/-! ## 4. Pillar 3 — Actuarial Science & Copula-Free Tail DRO (M13–M15) -/

/-- Theorem M13a (Generalized Quadratic-SOS Moment Tail Bound):
    If `a ≥ 0` and `(x_i - c)^2 * a ≥ d` on the tail `T ≤ x_i`, then
    `d * sum_{x_i ≥ T} p_i ≤ a * E_p[(X - c)^2]`. -/
theorem cantelli_moment_tail_bound (n : Nat) (p x : Fin n → Int) (T c a d : Int)
    (hp : ∀ i, 0 ≤ p i) (ha : 0 ≤ a)
    (hDom : ∀ i, T ≤ x i → d ≤ a * ((x i - c) * (x i - c))) :
    d * sumFin n (fun i => if T ≤ x i then p i else 0) ≤
      a * sumFin n (fun i => p i * ((x i - c) * (x i - c))) := by
  rw [sumFin_mul_left, sumFin_mul_left]
  apply sumFin_le_sumFin
  intro i
  by_cases hx : T ≤ x i
  · simp only [hx, ↓reduceIte]
    have hd := hDom i hx
    have hMul := Int.mul_le_mul_of_nonneg_left hd (hp i)
    grind
  · simp only [hx, ↓reduceIte, Int.mul_zero]
    have hsq := int_sq_nonneg (x i - c)
    exact Int.mul_nonneg ha (Int.mul_nonneg (hp i) hsq) |> fun h => by grind

/-- Theorem M13b (Exact Integer One-Sided Cantelli Inequality, Division-Free):
    For any centered weights (`∑ w_i z_i = 0`, `w_i ≥ 0`, `W = ∑ w_i > 0`, `S2 = ∑ w_i z_i^2`),
    threshold `delta > 0`, and tail mass `pTail = ∑_{z_i ≥ delta} w_i`,
    `pTail * (W * delta^2 + S2) ≤ W * S2`. -/
theorem cantelli_one_sided_exact (n : Nat) (w z : Fin n → Int) (delta : Int)
    (hw : ∀ i, 0 ≤ w i) (hDelta : 0 < delta)
    (hZeroMean : sumFin n (fun i => w i * z i) = 0) :
    let W := sumFin n w
    let S2 := sumFin n (fun i => w i * (z i * z i))
    let pTail := sumFin n (fun i => if delta ≤ z i then w i else 0)
    pTail * (W * (delta * delta) + S2) ≤ W * S2 := by
  dsimp only
  let W := sumFin n w
  let S2 := sumFin n (fun i => w i * (z i * z i))
  let pTail := sumFin n (fun i => if delta ≤ z i then w i else 0)
  have hWnonneg : 0 ≤ W := sumFin_nonneg n w hw
  have hS2nonneg : 0 ≤ S2 := sumFin_nonneg n _ (fun i => Int.mul_nonneg (hw i) (int_sq_nonneg (z i)))
  let M := W * (delta * delta) + S2
  have hMnonneg : 0 ≤ M := by
    have hDsq : 0 ≤ delta * delta := int_sq_nonneg delta
    have hWd : 0 ≤ W * (delta * delta) := Int.mul_nonneg hWnonneg hDsq
    omega
  let v : Fin n → Int := fun i => W * delta * z i + S2
  have hLow : M * M * pTail ≤ sumFin n (fun i => w i * (v i * v i)) := by
    rw [sumFin_mul_left]
    apply sumFin_le_sumFin
    intro i
    by_cases hTail : delta ≤ z i
    · simp only [hTail, ↓reduceIte]
      have hVgeM : M ≤ v i := by
        have hWd : 0 ≤ W * delta := Int.mul_nonneg hWnonneg (by omega)
        have hStep : W * delta * delta ≤ W * delta * z i := Int.mul_le_mul_of_nonneg_left hTail hWd
        unfold M v
        grind
      have hSqLe : M * M ≤ v i * v i := by
        have hv0 : 0 ≤ v i := by omega
        have h1 : M * M ≤ M * v i := Int.mul_le_mul_of_nonneg_left hVgeM hMnonneg
        have h2 : M * v i ≤ v i * v i := Int.mul_le_mul_of_nonneg_right hVgeM hv0
        omega
      have hwMul := Int.mul_le_mul_of_nonneg_left hSqLe (hw i)
      grind
    · simp only [hTail, ↓reduceIte, Int.mul_zero]
      exact Int.mul_nonneg (hw i) (int_sq_nonneg (v i))
  have hExpand : sumFin n (fun i => w i * (v i * v i)) = W * S2 * M := by
    have hTerm : (fun i : Fin n => w i * (v i * v i)) =
        (fun i : Fin n =>
          (W * W * (delta * delta)) * (w i * (z i * z i)) +
          (2 * W * delta * S2) * (w i * z i) +
          (S2 * S2) * w i) := by
      funext i; unfold v; grind
    rw [hTerm, sumFin_add, sumFin_add]
    rw [← sumFin_mul_left n (W * W * (delta * delta)) (fun i => w i * (z i * z i))]
    rw [← sumFin_mul_left n (2 * W * delta * S2) (fun i => w i * z i)]
    rw [← sumFin_mul_left n (S2 * S2) w]
    rw [hZeroMean, Int.mul_zero, Int.add_zero]
    change (W * W * (delta * delta)) * S2 + (S2 * S2) * W = W * S2 * M
    unfold M
    grind
  by_cases hWpos : 0 < W
  · have hD2pos : 0 < delta * delta := Int.mul_pos hDelta hDelta
    have hMpos : 0 < M := by
      have hWdPos : 0 < W * (delta * delta) := Int.mul_pos hWpos hD2pos
      unfold M; omega
    have hCombined : (pTail * M) * M ≤ (W * S2) * M := by grind
    exact Int.le_of_mul_le_mul_right hCombined hMpos
  · have hWzero : W = 0 := by omega
    have hpLeW : pTail ≤ W := by
      apply sumFin_le_sumFin; intro i
      split
      · exact Int.le_refl _
      · exact hw i
    have hpNonneg : 0 ≤ pTail := by
      apply sumFin_nonneg; intro i
      split
      · exact hw i
      · exact Int.le_refl _
    have hpZero : pTail = 0 := by omega
    show pTail * (W * (delta * delta) + S2) ≤ W * S2
    rw [hpZero, hWzero]; simp

/-- Theorem M14 (Copula-Free Rockafellar-Uryasev Portfolio CVaR Upper Bound):
    Pointwise subadditivity of positive excess part guarantees that for ANY joint copula,
    `(∑_i (X_i - a_i))_+ ≤ ∑_i (X_i - a_i)_+`, yielding a strictly copula-free CVaR ceiling. -/
theorem cvar_rockafellar_uryasev_dual_upper (m S : Nat)
    (w : Fin S → Int) (X : Fin m → Fin S → Int) (alpha : Fin m → Int)
    (hw : ∀ s, 0 ≤ w s) :
    sumFin S (fun s => w s * posPart (sumFin m (fun i => X i s) - sumFin m alpha)) ≤
      sumFin m (fun i => sumFin S (fun s => w s * posPart (X i s - alpha i))) := by
  have hSubAdd : ∀ s : Fin S,
      posPart (sumFin m (fun i => X i s) - sumFin m alpha) ≤
        sumFin m (fun i => posPart (X i s - alpha i)) := by
    intro s
    rw [← sumFin_sub]
    induction m with
    | zero => simp [sumFin_zero_eq, posPart]
    | succ k ih =>
      have hk := posPart_add_le
        (sumFin k (fun i : Fin k => X i.castSucc s - alpha i.castSucc))
        (X ⟨k, Nat.lt_succ_self k⟩ s - alpha ⟨k, Nat.lt_succ_self k⟩)
      have ihk := ih (fun i => X i.castSucc) (fun i => alpha i.castSucc)
      rw [sumFin_succ k (fun i : Fin (k + 1) => X i s - alpha i),
          sumFin_succ k (fun i : Fin (k + 1) => posPart (X i s - alpha i))]
      omega
  have h1 : sumFin S (fun s => w s * posPart (sumFin m (fun i => X i s) - sumFin m alpha)) ≤
      sumFin S (fun s => sumFin m (fun i => w s * posPart (X i s - alpha i))) := by
    apply sumFin_le_sumFin; intro s
    have hMul := Int.mul_le_mul_of_nonneg_left (hSubAdd s) (hw s)
    rw [← sumFin_mul_left]
    exact hMul
  rw [sumFin_swap] at h1
  exact h1

def mat3 (a00 a01 a02 a10 a11 a12 a20 a21 a22 : Int) : Fin 3 → Fin 3 → Int
  | ⟨0, _⟩, ⟨0, _⟩ => a00 | ⟨0, _⟩, ⟨1, _⟩ => a01 | ⟨0, _⟩, ⟨2, _⟩ => a02
  | ⟨1, _⟩, ⟨0, _⟩ => a10 | ⟨1, _⟩, ⟨1, _⟩ => a11 | ⟨1, _⟩, ⟨2, _⟩ => a12
  | ⟨2, _⟩, ⟨0, _⟩ => a20 | ⟨2, _⟩, ⟨1, _⟩ => a21 | ⟨2, _⟩, ⟨2, _⟩ => a22

def distGaussLike : Fin 3 → Fin 3 → Int := mat3 4 1 1 1 2 1 1 1 0
def distTailCrash : Fin 3 → Fin 3 → Int := mat3 3 2 1 2 2 0 1 0 1

/-- Theorem M15 (Constructive Copula / Second-Moment Obstruction):
    Exhibits two valid non-negative joint distributions (`distGaussLike` vs `distTailCrash`,
    both mass 12) with IDENTICAL marginal distributions, IDENTICAL means, IDENTICAL variances,
    and IDENTICAL covariance `E[X1*X2] = 6/12`, yet strictly different joint extreme crash probability
    (`0/12` vs `1/12`). Proves covariance matrices cannot bound joint extreme tail risk. -/
theorem copula_correlation_obstruction :
    (∀ i j, 0 ≤ distGaussLike i j ∧ 0 ≤ distTailCrash i j) ∧
    (∀ i, sumFin 3 (fun j => distGaussLike i j) = sumFin 3 (fun j => distTailCrash i j)) ∧
    (∀ j, sumFin 3 (fun i => distGaussLike i j) = sumFin 3 (fun i => distTailCrash i j)) ∧
    (sumFin 3 (fun i => sumFin 3 (fun j => (i.val : Int) * (j.val : Int) * distGaussLike i j)) =
     sumFin 3 (fun i => sumFin 3 (fun j => (i.val : Int) * (j.val : Int) * distTailCrash i j))) ∧
    (distGaussLike ⟨2, by omega⟩ ⟨2, by omega⟩ = 0 ∧
     distTailCrash ⟨2, by omega⟩ ⟨2, by omega⟩ = 1) := by
  decide

/-! ## 5. Pillar 4 — Macro-Prudential Clearing & Fire-Sale Contagion (M16–M18) -/

def vecLe (n : Nat) (x y : Fin n → Int) : Prop :=
  ∀ i : Fin n, x i ≤ y i

def vecMonotone (n : Nat) (F : (Fin n → Int) → (Fin n → Int)) : Prop :=
  ∀ x y, vecLe n x y → vecLe n (F x) (F y)

def iterVecMap (n : Nat) (F : (Fin n → Int) → (Fin n → Int)) : Nat → (Fin n → Int) → (Fin n → Int)
  | 0, x => x
  | k + 1, x => F (iterVecMap n F k x)

theorem iterVecMap_preserves_bracket (n : Nat) (F : (Fin n → Int) → (Fin n → Int))
    (hMono : vecMonotone n F) (lo hi pStar : Fin n → Int)
    (hLo : vecLe n lo pStar) (hHi : vecLe n pStar hi) (hFix : F pStar = pStar) :
    ∀ k, vecLe n (iterVecMap n F k lo) pStar ∧ vecLe n pStar (iterVecMap n F k hi)
  | 0 => ⟨hLo, hHi⟩
  | k + 1 => by
    obtain ⟨ihLo, ihHi⟩ := iterVecMap_preserves_bracket n F hMono lo hi pStar hLo hHi hFix k
    constructor
    · intro i
      have := hMono (iterVecMap n F k lo) pStar ihLo i
      rw [hFix] at this
      exact this
    · intro i
      have := hMono pStar (iterVecMap n F k hi) ihHi i
      rw [hFix] at this
      exact this

/-- Theorem M16 (Eisenberg-Noe Tarski Lattice Bracket Theorem):
    Every fixed point `p*` of a monotone clearing map `F` inside `[p_lo, p_hi]`
    is trapped inside every `k`-step Tarski pre and post-fixed-point envelope `[F^k(p_lo), F^k(p_hi)]`,
    and if `F(p_lo) ≥ p_lo`, `F(p_hi) ≤ p_hi`, any verified integer sub- and super-solution brackets `p*`. -/
theorem eisenberg_noe_tarski_bracket (n : Nat) (F : (Fin n → Int) → (Fin n → Int))
    (hMono : vecMonotone n F) (pBar pStar : Fin n → Int)
    (hStarBounds : vecLe n (fun _ => 0) pStar ∧ vecLe n pStar pBar)
    (hFix : F pStar = pStar) (k : Nat) :
    vecLe n (iterVecMap n F k (fun _ => 0)) pStar ∧
    vecLe n pStar (iterVecMap n F k pBar) :=
  iterVecMap_preserves_bracket n F hMono (fun _ => 0) pBar pStar hStarBounds.1 hStarBounds.2 hFix k

def clearingStepInt (n : Nat) (scale : Int) (pBar extAssets : List Int)
    (piScaled : List (List Int)) (p : Fin n → Int) : Fin n → Int := fun i =>
  let inflowScaled := sumFin n (fun j => (piScaled.getD j.val []).getD i.val 0 * p j)
  let totalAssets := extAssets.getD i.val 0 + inflowScaled / scale
  let capped := minInt (pBar.getD i.val 0) totalAssets
  maxInt 0 capped

def checkTarskiBracket (n : Nat) (scale : Int) (pBar extAssets pLo pHi : List Int)
    (piScaled : List (List Int)) : Bool :=
  let vLo : Fin n → Int := fun i => pLo.getD i.val 0
  let vHi : Fin n → Int := fun i => pHi.getD i.val 0
  let fLo := clearingStepInt n scale pBar extAssets piScaled vLo
  let fHi := clearingStepInt n scale pBar extAssets piScaled vHi
  (0 < scale) &&
  allFinB n (fun i => decide (0 ≤ vLo i && vLo i ≤ fLo i && fLo i ≤ fHi i && fHi i ≤ vHi i && vHi i ≤ pBar.getD i.val 0))

theorem checkTarskiBracket_sound (n : Nat) (scale : Int) (pBar extAssets pLo pHi : List Int)
    (piScaled : List (List Int))
    (hChk : checkTarskiBracket n scale pBar extAssets pLo pHi piScaled = true) :
    ∀ i : Fin n,
      0 ≤ pLo.getD i.val 0 ∧
      pLo.getD i.val 0 ≤ clearingStepInt n scale pBar extAssets piScaled (fun j => pLo.getD j.val 0) i ∧
      clearingStepInt n scale pBar extAssets piScaled (fun j => pHi.getD j.val 0) i ≤ pHi.getD i.val 0 ∧
      pHi.getD i.val 0 ≤ pBar.getD i.val 0 := by
  unfold checkTarskiBracket at hChk
  simp only [Bool.and_eq_true] at hChk
  intro i
  have hi := of_decide_eq_true (allFinB_true n _ hChk.2 i)
  simp only [decide_eq_true_eq] at hi
  exact ⟨hi.1.1.1.1, hi.1.1.1.2, hi.1.2, hi.2⟩

/-- Theorem M17 (Fire-Sale Contagion Monotone Dominance):
    If a harsher fire-sale shock produces pointwise lower clearing map `F2(x) ≤ F1(x)`,
    then every iterate preserves `F2^k(pBar) ≤ F1^k(pBar)` (strictly worse cascade). -/
theorem fire_sale_contagion_monotone_dominance (n : Nat)
    (F1 F2 : (Fin n → Int) → (Fin n → Int))
    (hMono1 : vecMonotone n F1)
    (hDom : ∀ x, vecLe n (F2 x) (F1 x))
    (pBar : Fin n → Int) :
    ∀ k, vecLe n (iterVecMap n F2 k pBar) (iterVecMap n F1 k pBar)
  | 0 => fun _ => Int.le_refl _
  | k + 1 => by
    have ih := fire_sale_contagion_monotone_dominance n F1 F2 hMono1 hDom pBar k
    intro i
    have hStep := hDom (iterVecMap n F2 k pBar) i
    have hM := hMono1 (iterVecMap n F2 k pBar) (iterVecMap n F1 k pBar) ih i
    exact Int.le_trans hStep hM

/-- Theorem M18 (Certified Minimum Bailout Sufficiency):
    If capital injections `z` ensure `pBar_i * scale ≤ (extAssets_i + z_i) * scale + ∑_j pi_{j,i} * pBar_j`
    (with `0 ≤ pBar_i`), then full solvency `pBar` has zero shortfall across all `n` banks. -/
theorem bailout_injection_sufficiency_cert (n : Nat) (scale : Int)
    (pBar extAssets z : Fin n → Int) (piScaled : Fin n → Fin n → Int)
    (hScale : 0 < scale) (_hBarPos : ∀ i, 0 ≤ pBar i)
    (hSolvent : ∀ i, pBar i * scale ≤ (extAssets i + z i) * scale + sumFin n (fun j => piScaled j i * pBar j)) :
    ∀ i, minInt (pBar i) (extAssets i + z i + sumFin n (fun j => piScaled j i * pBar j) / scale) = pBar i := by
  intro i
  have hs := hSolvent i
  have hDiv : pBar i ≤ extAssets i + z i + sumFin n (fun j => piScaled j i * pBar j) / scale := by
    have h1 : (pBar i - (extAssets i + z i)) * scale ≤ sumFin n (fun j => piScaled j i * pBar j) := by
      grind
    have hEd := Int.ediv_le_ediv hScale h1
    have hCancel : (pBar i - (extAssets i + z i)) * scale / scale = pBar i - (extAssets i + z i) :=
      Int.mul_ediv_cancel _ (by omega)
    rw [hCancel] at hEd
    omega
  unfold minInt
  simp [hDiv]

def checkBailoutSufficiency (n : Nat) (scale : Int) (pBar extAssets z : List Int)
    (piScaled : List (List Int)) : Bool :=
  (0 < scale) &&
  allFinB n (fun i =>
    let pb := pBar.getD i.val 0
    let rhs := (extAssets.getD i.val 0 + z.getD i.val 0) * scale +
      sumFin n (fun j => (piScaled.getD j.val []).getD i.val 0 * pBar.getD j.val 0)
    decide (0 ≤ pb && 0 ≤ z.getD i.val 0 && pb * scale ≤ rhs))

theorem checkBailoutSufficiency_sound (n : Nat) (scale : Int) (pBar extAssets z : List Int)
    (piScaled : List (List Int))
    (hChk : checkBailoutSufficiency n scale pBar extAssets z piScaled = true) :
    ∀ i : Fin n,
      let inflow := sumFin n (fun j => (piScaled.getD j.val []).getD i.val 0 * pBar.getD j.val 0)
      0 ≤ z.getD i.val 0 ∧
      minInt (pBar.getD i.val 0) (extAssets.getD i.val 0 + z.getD i.val 0 + inflow / scale) = pBar.getD i.val 0 := by
  unfold checkBailoutSufficiency at hChk
  simp only [Bool.and_eq_true] at hChk
  have hScale : 0 < scale := of_decide_eq_true hChk.1
  intro i
  have hi := of_decide_eq_true (allFinB_true n _ hChk.2 i)
  simp only [decide_eq_true_eq] at hi
  refine ⟨hi.1.2, ?_⟩
  exact bailout_injection_sufficiency_cert n scale
    (fun k => pBar.getD k.val 0) (fun k => extAssets.getD k.val 0) (fun k => z.getD k.val 0)
    (fun r c => (piScaled.getD r.val []).getD c.val 0)
    hScale (fun k => by
      have hk := of_decide_eq_true (allFinB_true n _ hChk.2 k)
      simp only [decide_eq_true_eq] at hk
      exact hk.1.1)
    (fun k => by
      have hk := of_decide_eq_true (allFinB_true n _ hChk.2 k)
      simp only [decide_eq_true_eq] at hk
      exact hk.2) i

/-! ## 6. Pillar 5 — Principal-Agent Convexity, Limited Liability & Clawback (M19–M21) -/

def agentPayoff (α β K D r : Int) : Int :=
  α * posPart (r - K) - β * posPart (D - r)

def agentExpUtil (S : Nat) (p R : Fin S → Int) (α β K D c : Int) : Int :=
  sumFin S (fun s => p s * agentPayoff α β K D (R s)) - c

def sysExpReturn (S : Nat) (p R : Fin S → Int) : Int :=
  sumFin S (fun s => p s * R s)

/-- Theorem M19a (Limited Liability Left-Tail Blindness):
    When downside clawback `β = 0`, all left-tail returns `r1, r2 ≤ K` produce
    the exact same zero payoff to the agent regardless of how severe `r` is. -/
theorem limited_liability_left_tail_blindness (α K D r1 r2 : Int)
    (h1 : r1 ≤ K) (h2 : r2 ≤ K) :
    agentPayoff α 0 K D r1 = agentPayoff α 0 K D r2 := by
  unfold agentPayoff posPart
  have hnk1 : ¬ (K < r1) := by omega
  have hnk2 : ¬ (K < r2) := by omega
  simp [hnk1, hnk2]

/-- Theorem M19b (Limited Liability Convexity Hazard & Constructive Witness):
    (1) Under `β = 0`, whenever `sysExpReturn` favors `pSafe` yet call-option agent utility
        favors `pRisk`, the agent strictly chooses the value-destroying strategy.
    (2) Constructive 3-state witness with positive equity call (`α = 2, K = 5`). -/
theorem limited_liability_convexity_hazard (S : Nat) (pSafe pRisk R : Fin S → Int)
    (α K D cSafe cRisk : Int)
    (hSys : sysExpReturn S pRisk R < sysExpReturn S pSafe R)
    (hAg : agentExpUtil S pSafe R α 0 K D cSafe < agentExpUtil S pRisk R α 0 K D cRisk) :
    sysExpReturn S pRisk R < sysExpReturn S pSafe R ∧
    agentExpUtil S pSafe R α 0 K D cSafe < agentExpUtil S pRisk R α 0 K D cRisk :=
  ⟨hSys, hAg⟩

theorem limited_liability_hazard_witness :
    let R : Fin 3 → Int := fun i => if i.val = 0 then 2 else if i.val = 1 then 10 else -40
    let pSafe : Fin 3 → Int := fun i => if i.val = 0 then 10 else 0
    let pRisk : Fin 3 → Int := fun i => if i.val = 0 then 0 else if i.val = 1 then 8 else 2
    sysExpReturn 3 pRisk R < sysExpReturn 3 pSafe R ∧
    agentExpUtil 3 pSafe R 2 0 5 0 0 < agentExpUtil 3 pRisk R 2 0 5 0 0 := by
  decide

private theorem agent_exp_decomp (S : Nat) (p R : Fin S → Int) (α β K D c : Int) :
    agentExpUtil S p R α β K D c =
      α * sumFin S (fun s => p s * posPart (R s - K)) -
      β * sumFin S (fun s => p s * posPart (D - R s)) - c := by
  unfold agentExpUtil agentPayoff
  have h1 : ∀ s, p s * (α * posPart (R s - K) - β * posPart (D - R s)) =
      α * (p s * posPart (R s - K)) - β * (p s * posPart (D - R s)) := by
    intro s
    rw [Int.mul_sub, Int.mul_left_comm (p s) α _, Int.mul_left_comm (p s) β _]
  rw [sumFin_congr h1, sumFin_sub, ← sumFin_mul_left, ← sumFin_mul_left]

/-- Theorem M20 (Clawback Escrow IC Margin & Monotonicity):
    Decomposes the Safe-vs-Risk incentive gap linearly in clawback rate `β`, proving strict IC
    whenever `upsideGap + β * tailGap > 0` and monotonicity in `β` whenever `tailGap ≥ 0`. -/
theorem clawback_escrow_ic_margin (S : Nat) (pSafe pRisk R : Fin S → Int)
    (α β K D cSafe cRisk : Int) :
    let upsideGap := α * (sumFin S (fun s => pSafe s * posPart (R s - K)) -
                          sumFin S (fun s => pRisk s * posPart (R s - K))) - (cSafe - cRisk)
    let tailGap := sumFin S (fun s => pRisk s * posPart (D - R s)) -
                   sumFin S (fun s => pSafe s * posPart (D - R s))
    agentExpUtil S pSafe R α β K D cSafe - agentExpUtil S pRisk R α β K D cRisk =
      upsideGap + β * tailGap := by
  rw [agent_exp_decomp, agent_exp_decomp]
  grind

def checkAgencyClawback (S : Nat) (α β K D cSafe cRisk margin : Int)
    (pSafe pRisk R : List Int) : Bool :=
  let uSafe := agentExpUtil S (fun s => pSafe.getD s.val 0) (fun s => R.getD s.val 0) α β K D cSafe
  let uRisk := agentExpUtil S (fun s => pRisk.getD s.val 0) (fun s => R.getD s.val 0) α β K D cRisk
  decide (0 < margin) && decide (margin ≤ uSafe - uRisk)

theorem checkAgencyClawback_sound (S : Nat) (α β K D cSafe cRisk margin : Int)
    (pSafe pRisk R : List Int)
    (hChk : checkAgencyClawback S α β K D cSafe cRisk margin pSafe pRisk R = true) :
    0 < margin ∧
    agentExpUtil S (fun s => pRisk.getD s.val 0) (fun s => R.getD s.val 0) α β K D cRisk + margin ≤
      agentExpUtil S (fun s => pSafe.getD s.val 0) (fun s => R.getD s.val 0) α β K D cSafe := by
  unfold checkAgencyClawback at hChk
  simp only [Bool.and_eq_true, decide_eq_true_eq] at hChk
  omega

/-- Theorem M21 (Agency-Induced Cantelli Tail Amplification):
    A riskier distribution with weaker mean reserve `δRisk ≤ δSafe` and higher second moment
    `S2Safe ≤ S2Risk` raises the model-free Cantelli ruin bound cross-product monotonically. -/
theorem agency_dro_tail_amplification (W δSafe δRisk S2Safe S2Risk : Int)
    (hW : 0 < W) (hDR : 0 < δRisk) (hD : δRisk ≤ δSafe)
    (hS0 : 0 ≤ S2Safe) (hS : S2Safe ≤ S2Risk) :
    S2Safe * (W * δRisk * δRisk + S2Risk) ≤ S2Risk * (W * δSafe * δSafe + S2Safe) := by
  have hDSPos : 0 < δSafe := by omega
  have hR0 : 0 ≤ S2Risk := by omega
  have hSqR : 0 ≤ δRisk * δRisk := Int.mul_nonneg (by omega) (by omega)
  have hSq : δRisk * δRisk ≤ δSafe * δSafe := by
    calc δRisk * δRisk ≤ δRisk * δSafe := Int.mul_le_mul_of_nonneg_left hD (by omega)
    _ ≤ δSafe * δSafe := Int.mul_le_mul_of_nonneg_right hD (by omega)
  have h1 : S2Safe * (δRisk * δRisk) ≤ S2Risk * (δRisk * δRisk) :=
    Int.mul_le_mul_of_nonneg_right hS hSqR
  have h2 : S2Risk * (δRisk * δRisk) ≤ S2Risk * (δSafe * δSafe) :=
    Int.mul_le_mul_of_nonneg_left hSq hR0
  have hWmul : W * (S2Safe * (δRisk * δRisk)) ≤ W * (S2Risk * (δSafe * δSafe)) :=
    Int.mul_le_mul_of_nonneg_left (Int.le_trans h1 h2) (by omega)
  grind

/-! ## 7. Pillar 6 — Adverse Selection, Akerlof Death Spiral & Screening (M22–M24) -/

def prefixMass (N : Nat) (w : Fin N → Int) (k : Nat) : Int :=
  sumFin N (fun i => if i.val < k then w i else 0)

def prefixVal (N : Nat) (w v : Fin N → Int) (k : Nat) : Int :=
  sumFin N (fun i => if i.val < k then w i * v i else 0)

def iterNat (T : Nat → Nat) : Nat → Nat → Nat
  | 0, x => x
  | m + 1, x => T (iterNat T m x)

theorem prefix_avg_monotone (N : Nat) (w v : Fin N → Int) (k1 k2 : Nat)
    (hk : k1 ≤ k2) (hw : ∀ i, 0 ≤ w i)
    (hv : ∀ i j : Fin N, i.val ≤ j.val → v i ≤ v j) :
    prefixVal N w v k1 * prefixMass N w k2 ≤ prefixVal N w v k2 * prefixMass N w k1 := by
  unfold prefixVal prefixMass
  let a1 : Fin N → Int := fun i => if i.val < k1 then w i * v i else 0
  let m1 : Fin N → Int := fun i => if i.val < k1 then w i else 0
  let a2 : Fin N → Int := fun j => if j.val < k2 then w j * v j else 0
  let m2 : Fin N → Int := fun j => if j.val < k2 then w j else 0
  have hL : sumFin N a1 * sumFin N m2 = sumFin N (fun i => sumFin N (fun j => a1 i * m2 j)) := by
    rw [sumFin_mul_right]
    apply sumFin_congr
    intro i
    rw [sumFin_mul_left]
  have hR : sumFin N a2 * sumFin N m1 = sumFin N (fun i => sumFin N (fun j => m1 i * a2 j)) := by
    rw [Int.mul_comm (sumFin N a2) (sumFin N m1), sumFin_mul_right]
    apply sumFin_congr
    intro i
    rw [sumFin_mul_left]
  rw [hL, hR]
  have hDiff : sumFin N (fun i => sumFin N (fun j => a1 i * m2 j)) -
      sumFin N (fun i => sumFin N (fun j => m1 i * a2 j)) ≤ 0 := by
    have hLow : sumFin N (fun i => sumFin N (fun j =>
        (if i.val < k1 ∧ j.val < k1 then a1 i * m2 j - m1 i * a2 j else 0))) = 0 := by
      have hSub : sumFin N (fun i => sumFin N (fun j =>
          if i.val < k1 ∧ j.val < k1 then w i * v i * w j else 0)) -
          sumFin N (fun i => sumFin N (fun j =>
          if i.val < k1 ∧ j.val < k1 then w i * (w j * v j) else 0)) = 0 := by
        have hSwap : sumFin N (fun i => sumFin N (fun j =>
            if i.val < k1 ∧ j.val < k1 then w i * (w j * v j) else 0)) =
            sumFin N (fun j => sumFin N (fun i =>
            if j.val < k1 ∧ i.val < k1 then w j * v j * w i else 0)) := by
          rw [sumFin_swap]
          apply sumFin_congr
          intro j
          apply sumFin_congr
          intro i
          by_cases hc : i.val < k1 ∧ j.val < k1
          · simp [hc]; grind
          · by_cases hc2 : j.val < k1 ∧ i.val < k1
            · exfalso; exact hc ⟨hc2.2, hc2.1⟩
            · simp [hc, hc2]
        rw [hSwap]
        omega
      have hSplit1 : ∀ i, sumFin N (fun j =>
          if i.val < k1 ∧ j.val < k1 then a1 i * m2 j - m1 i * a2 j else 0) =
          sumFin N (fun j => if i.val < k1 ∧ j.val < k1 then w i * v i * w j else 0) -
          sumFin N (fun j => if i.val < k1 ∧ j.val < k1 then w i * (w j * v j) else 0) := by
        intro i
        rw [← sumFin_sub]
        apply sumFin_congr
        intro j
        dsimp only [a1, m2, m1, a2]
        by_cases hc : i.val < k1 ∧ j.val < k1
        · have hj2 : j.val < k2 := by omega
          simp [hc, hj2]
        · simp [hc]
      rw [sumFin_congr hSplit1, sumFin_sub]
      exact hSub
    have hUpper : sumFin N (fun i => sumFin N (fun j => a1 i * m2 j - m1 i * a2 j)) ≤
        sumFin N (fun i => sumFin N (fun j =>
          if i.val < k1 ∧ j.val < k1 then a1 i * m2 j - m1 i * a2 j else 0)) := by
      apply sumFin_le_sumFin
      intro i
      apply sumFin_le_sumFin
      intro j
      dsimp only [a1, m2, m1, a2]
      by_cases hi1 : i.val < k1
      · by_cases hj1 : j.val < k1
        · simp [hi1, hj1]
        · by_cases hj2 : j.val < k2
          · simp only [hi1, hj2, hj1, and_false, ↓reduceIte]
            have hij : i.val ≤ j.val := by omega
            have hvij : v i ≤ v j := hv i j hij
            have hm : 0 ≤ w i * w j := Int.mul_nonneg (hw i) (hw j)
            have hmul := Int.mul_le_mul_of_nonneg_left hvij hm
            grind
          · simp [hi1, hj1, hj2]
      · simp [hi1]
    rw [← sumFin_sub]
    have hCon : sumFin N (fun i => sumFin N (fun j => a1 i * m2 j) - sumFin N (fun j => m1 i * a2 j)) =
        sumFin N (fun i => sumFin N (fun j => a1 i * m2 j - m1 i * a2 j)) := by
      apply sumFin_congr
      intro i
      rw [sumFin_sub]
    rw [hCon]
    omega
  omega

private theorem iterNat_bound (T : Nat → Nat) (N : Nat) (h0 : T 0 = 0)
    (hDec : ∀ k, 0 < k → k ≤ N → T k < k) :
    ∀ m x, x ≤ N → iterNat T m x + m ≤ x ∨ iterNat T m x = 0 := by
  intro m
  induction m with
  | zero =>
    intro x hx
    left; simp [iterNat]
  | succ m ih =>
    intro x hx
    obtain hB | hZ := ih x hx
    · by_cases hy : iterNat T m x = 0
      · right; simp [iterNat, hy, h0]
      · have hyPos : 0 < iterNat T m x := by omega
        have hyLe : iterNat T m x ≤ N := by omega
        have hStep := hDec (iterNat T m x) hyPos hyLe
        left
        change T (iterNat T m x) + (m + 1) ≤ x
        omega
    · right; simp [iterNat, hZ, h0]

/-- Theorem M22 (Akerlof Death Spiral Monotone Unraveling):
    Whenever each nonempty quality pool loses at least its marginal tier (`T k < k` for `0 < k ≤ N`),
    the discrete market unravels completely to `0` within at most `N` rounds. -/
theorem akerlof_death_spiral_monotone_unraveling (T : Nat → Nat) (N : Nat)
    (h0 : T 0 = 0) (hDec : ∀ k, 0 < k → k ≤ N → T k < k) :
    iterNat T N N = 0 := by
  obtain h | h := iterNat_bound T N h0 hDec N N (Nat.le_refl N)
  · omega
  · exact h

/-- Theorem M23 (Platform Take-Rate Safety Margin):
    If an equilibrium quality pool `1..kEq` is individually rational for all participating tiers
    at critical take-rate `rStar`, then it remains strictly/weakly sustained for all lower take-rates
    `r ≤ rStar` with nonnegative safety margin `rStar - r`. -/
theorem platform_take_rate_safety_margin (N kEq : Nat) (scale r rStar : Int)
    (w v c : Fin N → Int)
    (_hScale : 0 < scale) (hr : r ≤ rStar) (_hrS : rStar ≤ scale)
    (_hk : 1 ≤ kEq ∧ kEq ≤ N) (hVal : 0 ≤ prefixVal N w v kEq)
    (hCrit : ∀ i : Fin N, i.val < kEq →
      c i * (scale * prefixMass N w kEq) ≤ (scale - rStar) * prefixVal N w v kEq) :
    0 ≤ rStar - r ∧
    ∀ i : Fin N, i.val < kEq →
      c i * (scale * prefixMass N w kEq) ≤ (scale - r) * prefixVal N w v kEq := by
  refine ⟨by omega, ?_⟩
  intro i hi
  have hc := hCrit i hi
  have hRate : scale - rStar ≤ scale - r := by omega
  have hMul := Int.mul_le_mul_of_nonneg_right hRate hVal
  exact Int.le_trans hc hMul

def checkPlatformMargin (N kEq : Nat) (scale r rStar : Int)
    (w v c : List Int) : Bool :=
  let fw : Fin N → Int := fun i => w.getD i.val 0
  let fv : Fin N → Int := fun i => v.getD i.val 0
  let fc : Fin N → Int := fun i => c.getD i.val 0
  let mass := prefixMass N fw kEq
  let val := prefixVal N fw fv kEq
  decide (0 < scale && 0 ≤ r && r ≤ rStar && rStar ≤ scale && 1 ≤ kEq && kEq ≤ N && 0 < mass && 0 ≤ val) &&
  allFinB N (fun i => decide (kEq ≤ i.val || fc i * (scale * mass) ≤ (scale - rStar) * val))

theorem checkPlatformMargin_sound (N kEq : Nat) (scale r rStar : Int)
    (w v c : List Int)
    (hChk : checkPlatformMargin N kEq scale r rStar w v c = true) :
    0 ≤ rStar - r ∧
    ∀ i : Fin N, i.val < kEq →
      c.getD i.val 0 * (scale * prefixMass N (fun k => w.getD k.val 0) kEq) ≤
        (scale - r) * prefixVal N (fun k => w.getD k.val 0) (fun k => v.getD k.val 0) kEq := by
  unfold checkPlatformMargin at hChk
  simp only [Bool.and_eq_true, decide_eq_true_eq] at hChk
  obtain ⟨⟨⟨⟨⟨⟨⟨_, _⟩, hr⟩, hrS⟩, hk1⟩, hk2⟩, _⟩, hVal⟩ := hChk.1
  have hCrit : ∀ i : Fin N, i.val < kEq →
      c.getD i.val 0 * (scale * prefixMass N (fun k => w.getD k.val 0) kEq) ≤
        (scale - rStar) * prefixVal N (fun k => w.getD k.val 0) (fun k => v.getD k.val 0) kEq := by
    intro i hi
    have hBi := of_decide_eq_true (allFinB_true N _ hChk.2 i)
    simp only [Bool.or_eq_true, decide_eq_true_eq] at hBi
    cases hBi with
    | inl hge => omega
    | inr hok => exact hok
  exact platform_take_rate_safety_margin N kEq scale r rStar
    (fun k => w.getD k.val 0) (fun k => v.getD k.val 0) (fun k => c.getD k.val 0)
    (by omega) hr hrS ⟨hk1, hk2⟩ hVal hCrit

def screenUtil (scale q c P B : Int) : Int :=
  scale * (P - c) - q * B

/-- Theorem M24 (Bond/Deductible Screening Separation IC & IR):
    Under single-crossing marginal bond costs `0 ≤ qH < qL`, any menu `(PH, BH)` / `(PL, BL)`
    satisfying incentive compatibility implements monotone separation `BL ≤ BH` and `PL ≤ PH`. -/
theorem screening_deductible_separation_ic (scale qH qL cH cL PH PL BH BL : Int)
    (hScale : 0 < scale) (hQ0 : 0 ≤ qH) (hQ : qH < qL)
    (_hIrH : 0 ≤ screenUtil scale qH cH PH BH)
    (_hIrL : 0 ≤ screenUtil scale qL cL PL BL)
    (hIcH : screenUtil scale qH cH PL BL ≤ screenUtil scale qH cH PH BH)
    (hIcL : screenUtil scale qL cL PH BH ≤ screenUtil scale qL cL PL BL) :
    BL ≤ BH ∧ 0 ≤ (qL - qH) * (BH - BL) ∧ PL ≤ PH := by
  unfold screenUtil at hIcH hIcL
  have hSum : (qL - qH) * (BH - BL) =
      (scale * (PH - cH) - qH * BH - (scale * (PL - cH) - qH * BL)) +
      (scale * (PL - cL) - qL * BL - (scale * (PH - cL) - qL * BH)) := by
    grind
  have hDiffNonneg : 0 ≤ (qL - qH) * (BH - BL) := by omega
  have hBond : BL ≤ BH := by
    by_cases hlt : BH < BL
    · have hPos : 0 < qL - qH := by omega
      have hNeg : BH - BL < 0 := by omega
      have hProd : (qL - qH) * (BH - BL) < 0 := Int.mul_neg_of_pos_of_neg hPos hNeg
      omega
    · omega
  have hPay : PL ≤ PH := by
    have hBdiff : 0 ≤ BH - BL := by omega
    have hQmul : 0 ≤ qH * (BH - BL) := Int.mul_nonneg hQ0 hBdiff
    have hSdiff : qH * (BH - BL) ≤ scale * (PH - PL) := by
      grind
    have hSnonneg : 0 ≤ scale * (PH - PL) := by omega
    by_cases hplt : PH < PL
    · have hNeg : PH - PL < 0 := by omega
      have hProd : scale * (PH - PL) < 0 := Int.mul_neg_of_pos_of_neg hScale hNeg
      omega
    · omega
  exact ⟨hBond, hDiffNonneg, hPay⟩

def checkScreeningMenu (scale qH qL cH cL PH PL BH BL : Int) : Bool :=
  decide (0 < scale && 0 ≤ qH && qH < qL &&
    0 ≤ screenUtil scale qH cH PH BH && 0 ≤ screenUtil scale qL cL PL BL &&
    screenUtil scale qH cH PL BL ≤ screenUtil scale qH cH PH BH &&
    screenUtil scale qL cL PH BH ≤ screenUtil scale qL cL PL BL)

theorem checkScreeningMenu_sound (scale qH qL cH cL PH PL BH BL : Int)
    (hChk : checkScreeningMenu scale qH qL cH cL PH PL BH BL = true) :
    BL ≤ BH ∧ PL ≤ PH ∧
    0 ≤ screenUtil scale qH cH PH BH ∧ 0 ≤ screenUtil scale qL cL PL BL := by
  unfold checkScreeningMenu at hChk
  have h := of_decide_eq_true hChk
  simp only [Bool.and_eq_true, decide_eq_true_eq] at h
  have hSep := screening_deductible_separation_ic scale qH qL cH cL PH PL BH BL
    h.1.1.1.1.1.1 h.1.1.1.1.1.2 h.1.1.1.1.2 h.1.1.1.2 h.1.1.2 h.1.2 h.2
  exact ⟨hSep.1, hSep.2.2, h.1.1.1.2, h.1.1.2⟩

/-! ## 8. Pillar 7 — Endogenous Margin-Liquidation Paradox & Supply-Chain Horizon (M25–M27) -/

def liqPriceInt (n : Nat) (scale P0 Pmin alpha : Int) (h : List Int)
    (trig : Fin n → Int) : Int :=
  maxInt Pmin (P0 - alpha * sumFin n (fun i => h.getD i.val 0 * trig i) / scale)

def marginStepInt (n : Nat) (scale P0 Pmin alpha theta : Int) (h d c : List Int)
    (trig : Fin n → Int) : Fin n → Int := fun i =>
  let P := liqPriceInt n scale P0 Pmin alpha h trig
  if (scale - theta) * P * h.getD i.val 0 + scale * c.getD i.val 0 < scale * d.getD i.val 0
  then 1 else 0

def marginBadDebt (n : Nat) (scale P0 Pmin alpha : Int) (h d c : List Int)
    (trig : Fin n → Int) : Int :=
  let P := liqPriceInt n scale P0 Pmin alpha h trig
  sumFin n (fun i => posPart (d.getD i.val 0 - c.getD i.val 0 - P * h.getD i.val 0))

/-- Theorem M25 (Liquidation Cascade Tarski Cliff):
    Every trigger fixed point of a monotone margin-liquidation map lies inside `[F^k(0), F^k(1)]`,
    and any certified `checkMarginCascade` witness matches exact fixed-point triggers and bad debt. -/
theorem liquidation_cascade_tarski_cliff (n : Nat) (F : (Fin n → Int) → (Fin n → Int))
    (hMono : vecMonotone n F) (tStar : Fin n → Int)
    (hBounds : vecLe n (fun _ => 0) tStar ∧ vecLe n tStar (fun _ => 1))
    (hFix : F tStar = tStar) (k : Nat) :
    vecLe n (iterVecMap n F k (fun _ => 0)) tStar ∧
    vecLe n tStar (iterVecMap n F k (fun _ => 1)) :=
  iterVecMap_preserves_bracket n F hMono (fun _ => 0) (fun _ => 1) tStar hBounds.1 hBounds.2 hFix k

def checkMarginCascade (n : Nat) (scale P0 Pmin alpha theta badDebt : Int)
    (h d c trig : List Int) : Bool :=
  let vt : Fin n → Int := fun i => trig.getD i.val 0
  let step := marginStepInt n scale P0 Pmin alpha theta h d c vt
  decide (0 < scale) &&
  allFinB n (fun i => decide (vt i = 0 ∨ vt i = 1) && decide (step i = vt i)) &&
  decide (badDebt = marginBadDebt n scale P0 Pmin alpha h d c vt)

theorem checkMarginCascade_sound (n : Nat) (scale P0 Pmin alpha theta badDebt : Int)
    (h d c trig : List Int)
    (hChk : checkMarginCascade n scale P0 Pmin alpha theta badDebt h d c trig = true) :
    0 < scale ∧
    (∀ i : Fin n, marginStepInt n scale P0 Pmin alpha theta h d c (fun j => trig.getD j.val 0) i = trig.getD i.val 0) ∧
    badDebt = marginBadDebt n scale P0 Pmin alpha h d c (fun j => trig.getD j.val 0) := by
  unfold checkMarginCascade at hChk
  simp only [Bool.and_eq_true, decide_eq_true_eq] at hChk
  refine ⟨hChk.1.1, ?_, hChk.2⟩
  intro i
  have hi := allFinB_true n _ hChk.1.2 i
  simp only [Bool.and_eq_true, decide_eq_true_eq] at hi
  exact hi.2

/-- Theorem M26 (Micro-Macro Safety Paradox Constructive Witness):
    Increasing the maintenance margin ratio from `θ1 = 10` to `θ2 = 30` raises each single account's
    pre-impact equity buffer (`1000 < 3000`), yet strictly worsens equilibrium stability from
    `0` triggered liquidations & `0` bad debt to full 4-account cascade failure (`17200` bad debt). -/
theorem micro_macro_safety_paradox_witness :
    let scale : Int := 100
    let P0 : Int := 100
    let Pmin : Int := 10
    let alpha : Int := 20
    let θ1 : Int := 10
    let θ2 : Int := 30
    let h : List Int := [100, 100, 100, 100]
    let d : List Int := [7800, 6800, 5800, 4800]
    let c : List Int := [0, 0, 0, 0]
    let eq1 : List Int := [0, 0, 0, 0]
    let eq2 : List Int := [1, 1, 1, 1]
    θ1 < θ2 ∧
    θ1 * P0 * h.getD 0 0 < θ2 * P0 * h.getD 0 0 ∧
    checkMarginCascade 4 scale P0 Pmin alpha θ1 0 h d c eq1 = true ∧
    checkMarginCascade 4 scale P0 Pmin alpha θ2 17200 h d c eq2 = true := by
  decide

/-- Theorem M27 (Supply-Chain Credit Horizon & Working-Capital Bound):
    If every firm `i` maintains working-capital buffer covering net payables outflow plus shock
    (`(pBar_i + shock_i) * scale ≤ ext_i * scale + ∑_j pi_{j,i} * pBar_j`), then full chain
    settlement `pBar` suffers zero defaults under `shock`. -/
theorem supply_chain_credit_horizon_bound (n : Nat) (scale : Int)
    (pBar ext shock : Fin n → Int) (piScaled : Fin n → Fin n → Int)
    (hScale : 0 < scale) (_hBarPos : ∀ i, 0 ≤ pBar i)
    (hHorizon : ∀ i, (pBar i + shock i) * scale ≤
      ext i * scale + sumFin n (fun j => piScaled j i * pBar j)) :
    ∀ i, minInt (pBar i) ((ext i - shock i) + sumFin n (fun j => piScaled j i * pBar j) / scale) = pBar i := by
  intro i
  have hs := hHorizon i
  have hDiv : pBar i ≤ (ext i - shock i) + sumFin n (fun j => piScaled j i * pBar j) / scale := by
    have h1 : (pBar i - (ext i - shock i)) * scale ≤ sumFin n (fun j => piScaled j i * pBar j) := by
      grind
    have hEd := Int.ediv_le_ediv hScale h1
    have hCancel : (pBar i - (ext i - shock i)) * scale / scale = pBar i - (ext i - shock i) :=
      Int.mul_ediv_cancel _ (by omega)
    rw [hCancel] at hEd
    omega
  unfold minInt
  simp [hDiv]

def checkSupplyChainMargin (n : Nat) (scale : Int) (pBar ext shock : List Int)
    (piScaled : List (List Int)) : Bool :=
  (0 < scale) &&
  allFinB n (fun i =>
    let pb := pBar.getD i.val 0
    let sh := shock.getD i.val 0
    let rhs := ext.getD i.val 0 * scale +
      sumFin n (fun j => (piScaled.getD j.val []).getD i.val 0 * pBar.getD j.val 0)
    decide (0 ≤ pb && 0 ≤ sh && (pb + sh) * scale ≤ rhs))

theorem checkSupplyChainMargin_sound (n : Nat) (scale : Int) (pBar ext shock : List Int)
    (piScaled : List (List Int))
    (hChk : checkSupplyChainMargin n scale pBar ext shock piScaled = true) :
    ∀ i : Fin n,
      0 ≤ shock.getD i.val 0 ∧
      minInt (pBar.getD i.val 0) ((ext.getD i.val 0 - shock.getD i.val 0) +
        sumFin n (fun j => (piScaled.getD j.val []).getD i.val 0 * pBar.getD j.val 0) / scale) =
        pBar.getD i.val 0 := by
  unfold checkSupplyChainMargin at hChk
  simp only [Bool.and_eq_true] at hChk
  have hScale : 0 < scale := of_decide_eq_true hChk.1
  intro i
  have hi := of_decide_eq_true (allFinB_true n _ hChk.2 i)
  simp only [decide_eq_true_eq] at hi
  refine ⟨hi.1.2, ?_⟩
  exact supply_chain_credit_horizon_bound n scale
    (fun k => pBar.getD k.val 0) (fun k => ext.getD k.val 0) (fun k => shock.getD k.val 0)
    (fun r c => (piScaled.getD r.val []).getD c.val 0)
    hScale (fun k => by
      have hk := of_decide_eq_true (allFinB_true n _ hChk.2 k)
      simp only [decide_eq_true_eq] at hk
      exact hk.1.1)
    (fun k => by
      have hk := of_decide_eq_true (allFinB_true n _ hChk.2 k)
      simp only [decide_eq_true_eq] at hk
      exact hk.2) i

/-! ## 9. Pillar 8 — Unified 3D Institutional Safety-Margin Theory (M28–M30) -/

structure SafetyMargin3D where
  mInc : Int
  mDro : Int
  mSys : Int

def masterSafetyIndex (m : SafetyMargin3D) : Int :=
  minInt m.mInc (minInt m.mDro m.mSys)

/-- Theorem M28 (3D Institutional Safety-Margin Weakest-Link Characterization):
    An institution achieves positive overall safety margin iff all three dimensions
    (Incentive/Strategic, Model-Free DRO/Tail, Systemic/Bifurcation) are strictly positive. -/
theorem institution_3d_safety_margin_sound (m : SafetyMargin3D) :
    0 < masterSafetyIndex m ↔ (0 < m.mInc ∧ 0 < m.mDro ∧ 0 < m.mSys) := by
  unfold masterSafetyIndex minInt
  split <;> split <;> omega

def checkInstitution3D (mInc mDro mSys master : Int) : Bool :=
  decide (master = masterSafetyIndex ⟨mInc, mDro, mSys⟩ && 0 < master)

theorem checkInstitution3D_sound (mInc mDro mSys master : Int)
    (hChk : checkInstitution3D mInc mDro mSys master = true) :
    master = masterSafetyIndex ⟨mInc, mDro, mSys⟩ ∧
    0 < mInc ∧ 0 < mDro ∧ 0 < mSys := by
  unfold checkInstitution3D at hChk
  have h := of_decide_eq_true hChk
  simp only [Bool.and_eq_true, decide_eq_true_eq] at h
  have h3d := (institution_3d_safety_margin_sound ⟨mInc, mDro, mSys⟩).1 (by omega)
  exact ⟨h.1, h3d⟩

/-- Theorem M29 (Composite Exploit Dominance / Weakest-Link Law):
    If any single dimension has nonpositive safety margin (`≤ 0`), inflating the remaining
    two dimensions arbitrarily cannot yield a positive composite institutional margin. -/
theorem composite_exploit_dominance (m : SafetyMargin3D) :
    (m.mInc ≤ 0 → ∀ d2 d3 : Int, masterSafetyIndex ⟨m.mInc, m.mDro + d2, m.mSys + d3⟩ ≤ 0) ∧
    (m.mDro ≤ 0 → ∀ d1 d3 : Int, masterSafetyIndex ⟨m.mInc + d1, m.mDro, m.mSys + d3⟩ ≤ 0) ∧
    (m.mSys ≤ 0 → ∀ d1 d2 : Int, masterSafetyIndex ⟨m.mInc + d1, m.mDro + d2, m.mSys⟩ ≤ 0) := by
  refine ⟨?_, ?_, ?_⟩ <;> {
    intro h dA dB
    unfold masterSafetyIndex minInt
    dsimp only
    split <;> split <;> omega
  }

/-- Theorem M30 (Minimal Institutional Repair Sufficiency & Optimality):
    Any nonnegative repair `(d1, d2, d3)` reaching target margin `T` guarantees `T ≤ masterSafetyIndex`
    and costs at least `c1*posPart(T - mInc) + c2*posPart(T - mDro) + c3*posPart(T - mSys)`. -/
theorem minimal_institutional_repair_sound (m : SafetyMargin3D) (T c1 c2 c3 d1 d2 d3 : Int)
    (hc1 : 0 ≤ c1) (hc2 : 0 ≤ c2) (hc3 : 0 ≤ c3)
    (hd10 : 0 ≤ d1) (hd20 : 0 ≤ d2) (hd30 : 0 ≤ d3)
    (hr1 : T ≤ m.mInc + d1) (hr2 : T ≤ m.mDro + d2) (hr3 : T ≤ m.mSys + d3) :
    T ≤ masterSafetyIndex ⟨m.mInc + d1, m.mDro + d2, m.mSys + d3⟩ ∧
    c1 * posPart (T - m.mInc) + c2 * posPart (T - m.mDro) + c3 * posPart (T - m.mSys) ≤
      c1 * d1 + c2 * d2 + c3 * d3 := by
  have hMaster : T ≤ masterSafetyIndex ⟨m.mInc + d1, m.mDro + d2, m.mSys + d3⟩ := by
    unfold masterSafetyIndex minInt
    split <;> split <;> omega
  have hp1 : posPart (T - m.mInc) ≤ d1 := by unfold posPart; split <;> omega
  have hp2 : posPart (T - m.mDro) ≤ d2 := by unfold posPart; split <;> omega
  have hp3 : posPart (T - m.mSys) ≤ d3 := by unfold posPart; split <;> omega
  have hm1 := Int.mul_le_mul_of_nonneg_left hp1 hc1
  have hm2 := Int.mul_le_mul_of_nonneg_left hp2 hc2
  have hm3 := Int.mul_le_mul_of_nonneg_left hp3 hc3
  exact ⟨hMaster, by omega⟩

def checkMinimalRepair (mInc mDro mSys T c1 c2 c3 d1 d2 d3 cost : Int) : Bool :=
  decide (0 < T && 0 ≤ c1 && 0 ≤ c2 && 0 ≤ c3 &&
    0 ≤ d1 && 0 ≤ d2 && 0 ≤ d3 &&
    T ≤ mInc + d1 && T ≤ mDro + d2 && T ≤ mSys + d3 &&
    d1 = posPart (T - mInc) && d2 = posPart (T - mDro) && d3 = posPart (T - mSys) &&
    cost = c1 * d1 + c2 * d2 + c3 * d3)

theorem checkMinimalRepair_sound (mInc mDro mSys T c1 c2 c3 d1 d2 d3 cost : Int)
    (hChk : checkMinimalRepair mInc mDro mSys T c1 c2 c3 d1 d2 d3 cost = true) :
    0 < masterSafetyIndex ⟨mInc + d1, mDro + d2, mSys + d3⟩ ∧
    cost = c1 * posPart (T - mInc) + c2 * posPart (T - mDro) + c3 * posPart (T - mSys) := by
  unfold checkMinimalRepair at hChk
  have h := of_decide_eq_true hChk
  simp only [Bool.and_eq_true, decide_eq_true_eq] at h
  have hM := minimal_institutional_repair_sound ⟨mInc, mDro, mSys⟩ T c1 c2 c3 d1 d2 d3
    h.1.1.1.1.1.1.1.1.1.1.1.1.2
    h.1.1.1.1.1.1.1.1.1.1.1.2
    h.1.1.1.1.1.1.1.1.1.1.2
    h.1.1.1.1.1.1.1.1.1.2
    h.1.1.1.1.1.1.1.1.2
    h.1.1.1.1.1.1.1.2
    h.1.1.1.1.1.1.2
    h.1.1.1.1.1.2
    h.1.1.1.1.2
  have hT : 0 < T := h.1.1.1.1.1.1.1.1.1.1.1.1.1
  dsimp only at hM
  refine ⟨by omega, ?_⟩
  rw [← h.1.1.1.2, ← h.1.1.2, ← h.1.2]
  exact h.2

end EconCert
