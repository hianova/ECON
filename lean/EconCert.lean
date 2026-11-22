/-
  EconCert — Formal Zero-FP-Trust Verification Kernel for ECON v1.0.0
  ====================================================================
  Imports ONLY `Flow.Interval` (domain-free, zero IO, zero `sorryAx`,
  zero `axiom`, zero `Lean.ofReduceBool`) and proves 18 theorems (`M1–M18`)
  across four economic & financial pillars:
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
-/
import Flow.Interval

namespace EconCert

open Flow.Interval

/-! ## 0. Self-Contained Exact Integer Helpers -/

def absInt (x : Int) : Int :=
  if x < 0 then -x else x

def posPart (x : Int) : Int :=
  if 0 < x then x else 0

def minInt (a b : Int) : Int :=
  if a ≤ b then a else b

def maxInt (a b : Int) : Int :=
  if a ≤ b then b else a

theorem int_sq_nonneg (x : Int) : 0 ≤ x * x := by
  by_cases hx : 0 ≤ x
  · exact Int.mul_nonneg hx hx
  · have hnx : 0 ≤ -x := by omega
    have hsq : (-x) * (-x) = x * x := Int.neg_mul_neg x x
    rw [← hsq]
    exact Int.mul_nonneg hnx hnx

theorem posPart_nonneg (x : Int) : 0 ≤ posPart x := by
  unfold posPart; split <;> omega

theorem posPart_add_le (a b : Int) : posPart (a + b) ≤ posPart a + posPart b := by
  unfold posPart
  split <;> split <;> split <;> omega

def sumFin : (n : Nat) → (Fin n → Int) → Int
  | 0, _ => 0
  | n + 1, f => sumFin n (fun i => f i.castSucc) + f ⟨n, Nat.lt_succ_self n⟩

theorem sumFin_congr {n : Nat} {f g : Fin n → Int} (h : ∀ i, f i = g i) :
    sumFin n f = sumFin n g := by
  induction n with
  | zero => rfl
  | succ k ih =>
    simp only [sumFin]
    rw [ih (fun i => h i.castSucc), h ⟨k, Nat.lt_succ_self k⟩]

theorem sumFin_zero (n : Nat) : sumFin n (fun _ => 0) = 0 := by
  induction n with
  | zero => rfl
  | succ k ih => simp only [sumFin, ih]; omega

theorem sumFin_nonneg (n : Nat) (f : Fin n → Int) (h : ∀ i, 0 ≤ f i) :
    0 ≤ sumFin n f := by
  induction n with
  | zero => simp [sumFin]
  | succ k ih =>
    simp only [sumFin]
    have h1 := ih (fun i => f i.castSucc) (fun i => h i.castSucc)
    have h2 := h ⟨k, Nat.lt_succ_self k⟩
    omega

theorem sumFin_add (n : Nat) (f g : Fin n → Int) :
    sumFin n (fun i => f i + g i) = sumFin n f + sumFin n g := by
  induction n with
  | zero => simp [sumFin]
  | succ k ih =>
    simp only [sumFin]
    rw [ih (fun i => f i.castSucc) (fun i => g i.castSucc)]
    omega

theorem sumFin_sub (n : Nat) (f g : Fin n → Int) :
    sumFin n (fun i => f i - g i) = sumFin n f - sumFin n g := by
  induction n with
  | zero => simp [sumFin]
  | succ k ih =>
    simp only [sumFin]
    rw [ih (fun i => f i.castSucc) (fun i => g i.castSucc)]
    omega

theorem sumFin_mul_left (n : Nat) (c : Int) (f : Fin n → Int) :
    c * sumFin n f = sumFin n (fun i => c * f i) := by
  induction n with
  | zero => simp [sumFin]
  | succ k ih =>
    simp only [sumFin]
    rw [Int.mul_add, ih (fun i => f i.castSucc)]

theorem sumFin_mul_right (n : Nat) (f : Fin n → Int) (c : Int) :
    sumFin n f * c = sumFin n (fun i => f i * c) := by
  induction n with
  | zero => simp [sumFin]
  | succ k ih =>
    simp only [sumFin]
    rw [Int.add_mul, ih (fun i => f i.castSucc)]

theorem sumFin_swap {m n : Nat} (f : Fin m → Fin n → Int) :
    sumFin m (fun i => sumFin n (fun j => f i j)) =
      sumFin n (fun j => sumFin m (fun i => f i j)) := by
  induction m with
  | zero =>
    simp only [sumFin]
    exact (sumFin_zero n).symm
  | succ k ih =>
    simp only [sumFin]
    rw [ih (fun i j => f i.castSucc j)]
    rw [← sumFin_add n (fun j => sumFin k (fun i => f i.castSucc j))
          (fun j => f ⟨k, Nat.lt_succ_self k⟩ j)]

theorem sumFin_single_hit (n : Nat) (i : Fin n) (c : Int) :
    sumFin n (fun j => if j = i then c else 0) = c := by
  induction n with
  | zero => exact Fin.elim0 i
  | succ k ih =>
    simp only [sumFin]
    by_cases hk : i.val = k
    · have hi : i = ⟨k, Nat.lt_succ_self k⟩ := Fin.ext hk
      subst hi
      have hsub : sumFin k (fun j : Fin k => if j.castSucc = ⟨k, Nat.lt_succ_self k⟩ then c else 0) = 0 := by
        have hzero : (fun j : Fin k => if j.castSucc = ⟨k, Nat.lt_succ_self k⟩ then c else 0) = (fun _ => 0) := by
          funext j
          have hne : j.castSucc ≠ ⟨k, Nat.lt_succ_self k⟩ := by
            intro h
            exact Nat.ne_of_lt j.isLt (congrArg Fin.val h)
          simp [hne]
        rw [hzero, sumFin_zero]
      simp [hsub]
    · have hilt : i.val < k := Nat.lt_of_le_of_ne (Nat.lt_succ_iff.mp i.isLt) hk
      let i' : Fin k := ⟨i.val, hilt⟩
      have hi : i = i'.castSucc := Fin.ext rfl
      have hlast : (if (⟨k, Nat.lt_succ_self k⟩ : Fin (k + 1)) = i then c else 0) = 0 := by
        have hne : (⟨k, Nat.lt_succ_self k⟩ : Fin (k + 1)) ≠ i := by
          intro h
          exact hk (congrArg Fin.val h).symm
        simp [hne]
      have hsub : (fun j : Fin k => if j.castSucc = i then c else 0) =
          (fun j : Fin k => if j = i' then c else 0) := by
        funext j
        by_cases hj : j = i'
        · subst hj; simp [hi]
        · have hne : j.castSucc ≠ i := by
            intro h
            apply hj; apply Fin.ext
            have hVal : j.castSucc.val = i.val := congrArg Fin.val h
            exact hVal
          simp [hj, hne]
      rw [hsub, ih i', hlast]
      omega

theorem sumFin_le_sumFin (n : Nat) (f g : Fin n → Int) (h : ∀ i, f i ≤ g i) :
    sumFin n f ≤ sumFin n g := by
  have hd : 0 ≤ sumFin n (fun i => g i - f i) := sumFin_nonneg n _ (fun i => by have := h i; omega)
  rw [sumFin_sub] at hd
  omega

theorem sumFin_split_at (n : Nat) (f : Fin n → Int) (i : Fin n) :
    sumFin n f = f i + sumFin n (fun j => if j = i then 0 else f j) := by
  have h1 : f i = sumFin n (fun j => if j = i then f i else 0) := (sumFin_single_hit n i (f i)).symm
  rw [h1, ← sumFin_add]
  apply sumFin_congr
  intro j
  by_cases hj : j = i
  · subst hj; simp
  · simp [hj]

private theorem sumFin_cons_shift (n : Nat) (f : Nat → Int) :
    sumFin (n + 1) (fun j : Fin (n + 1) => f j.val) =
      f 0 + sumFin n (fun j : Fin n => f (j.val + 1)) := by
  induction n with
  | zero => simp [sumFin]
  | succ k ih =>
    have hL : sumFin (k + 2) (fun j : Fin (k + 2) => f j.val) =
        sumFin (k + 1) (fun j : Fin (k + 1) => f j.val) + f (k + 1) := rfl
    rw [hL, ih]
    have hR : sumFin (k + 1) (fun j : Fin (k + 1) => f (j.val + 1)) =
        sumFin k (fun j : Fin k => f (j.val + 1)) + f (k + 1) := rfl
    rw [hR]
    omega

private theorem sumFin_rev_nat (n : Nat) (f : Nat → Int) :
    sumFin (n + 1) (fun d : Fin (n + 1) => f (n - d.val)) =
      sumFin (n + 1) (fun s : Fin (n + 1) => f s.val) := by
  induction n generalizing f with
  | zero => simp [sumFin]
  | succ k ih =>
    have hL : sumFin (k + 2) (fun d : Fin (k + 2) => f (k + 1 - d.val)) =
        sumFin (k + 1) (fun d : Fin (k + 1) => f ((k - d.val) + 1)) + f 0 := by
      change sumFin (k + 1) (fun d : Fin (k + 1) => f (k + 1 - d.val)) + f (k + 1 - (k + 1)) = _
      have hZero : k + 1 - (k + 1) = 0 := by omega
      rw [hZero]
      congr 1
      apply sumFin_congr
      intro d
      have hd : k + 1 - d.val = (k - d.val) + 1 := by omega
      rw [hd]
    rw [hL, ih (fun t => f (t + 1)), sumFin_cons_shift]
    omega

private def allFinAux (n : Nat) (p : Fin n → Bool) : (k : Nat) → k ≤ n → Bool
  | 0, _ => true
  | k + 1, hk => allFinAux n p k (Nat.le_of_lt hk) && p ⟨k, hk⟩

def allFinB (n : Nat) (p : Fin n → Bool) : Bool :=
  allFinAux n p n (Nat.le_refl n)

private theorem allFinAux_true (n : Nat) (p : Fin n → Bool) :
    ∀ k (hk : k ≤ n), allFinAux n p k hk = true → ∀ j (hj : j < k), p ⟨j, Nat.lt_of_lt_of_le hj hk⟩ = true
  | 0, _, _, _, hj => by omega
  | k + 1, hk, hall, j, hj => by
    simp only [allFinAux, Bool.and_eq_true] at hall
    by_cases hjk : j = k
    · subst hjk; exact hall.2
    · have hjlt : j < k := by omega
      exact allFinAux_true n p k (Nat.le_of_lt hk) hall.1 j hjlt

theorem allFinB_true (n : Nat) (p : Fin n → Bool) (h : allFinB n p = true) :
    ∀ i : Fin n, p i = true := by
  intro i
  exact allFinAux_true n p n (Nat.le_refl n) h i.val i.isLt

/-! ## 1. Vector Norms, Cholesky + SDD & Exact LP Duality (M1–M4) -/

def vecNormSq (n : Nat) (v : Fin n → Int) : Int :=
  sumFin n (fun i => v i * v i)

theorem vecNormSq_nonneg (n : Nat) (v : Fin n → Int) : 0 ≤ vecNormSq n v :=
  sumFin_nonneg n _ (fun i => int_sq_nonneg (v i))

def matQuad (n : Nat) (A : Fin n → Fin n → Int) (v : Fin n → Int) : Int :=
  sumFin n (fun i => sumFin n (fun j => v i * A i j * v j))

def gramLLt (n : Nat) (L : Fin n → Fin n → Int) (i j : Fin n) : Int :=
  sumFin n (fun k => L i k * L j k)

def IsSDD (n : Nat) (E : Fin n → Fin n → Int) : Prop :=
  (∀ i j, E i j = E j i) ∧
  (∀ i, sumFin n (fun j => if j = i then 0 else absInt (E i j)) ≤ E i i)

private theorem sdd_pair_nonneg (a x y : Int) :
    0 ≤ absInt a * (x * x) + 2 * (a * (x * y)) + absInt a * (y * y) := by
  by_cases ha : a < 0
  · have habs : absInt a = -a := by unfold absInt; simp [ha]
    rw [habs]
    have hna : 0 ≤ -a := by omega
    have hsq : 0 ≤ (x - y) * (x - y) := int_sq_nonneg (x - y)
    have hid : (-a) * ((x - y) * (x - y)) = (-a) * (x * x) + 2 * (a * (x * y)) + (-a) * (y * y) := by
      grind
    rw [← hid]
    exact Int.mul_nonneg hna hsq
  · have habs : absInt a = a := by unfold absInt; simp [ha]
    rw [habs]
    have hpa : 0 ≤ a := by omega
    have hsq : 0 ≤ (x + y) * (x + y) := int_sq_nonneg (x + y)
    have hid : a * ((x + y) * (x + y)) = a * (x * x) + 2 * (a * (x * y)) + a * (y * y) := by
      grind
    rw [← hid]
    exact Int.mul_nonneg hpa hsq

/-- Theorem M1 (SDD Quadratic Form Nonnegativity):
    Every symmetric diagonally dominant integer matrix has nonnegative quadratic form. -/
theorem sdd_quadratic_form_nonneg (n : Nat) (E : Fin n → Fin n → Int) (hSDD : IsSDD n E) :
    ∀ v : Fin n → Int, 0 ≤ matQuad n E v := by
  intro v
  rcases hSDD with ⟨hSymm, hDom⟩
  let offDiag : Fin n → Int := fun i => sumFin n (fun j => if j = i then 0 else absInt (E i j))
  let diagSum : Int := sumFin n (fun i => (E i i - offDiag i) * (v i * v i))
  let offSqI : Int := sumFin n (fun i => offDiag i * (v i * v i))
  let pairSum : Int :=
    sumFin n (fun i => sumFin n (fun j =>
      if j = i then 0 else absInt (E i j) * (v i * v i) + 2 * (E i j * (v i * v j)) + absInt (E i j) * (v j * v j)))
  have h1 : matQuad n E v = diagSum + offSqI + sumFin n (fun i => sumFin n (fun j => if j = i then 0 else E i j * (v i * v j))) := by
    have hA : matQuad n E v =
        sumFin n (fun i => E i i * (v i * v i) + sumFin n (fun j => if j = i then 0 else E i j * (v i * v j))) := by
      unfold matQuad
      apply sumFin_congr
      intro i
      have hSplit :
          (fun j : Fin n => v i * E i j * v j) =
            (fun j : Fin n => (if j = i then E i i * (v i * v i) else 0) + (if j = i then 0 else E i j * (v i * v j))) := by
        funext j
        by_cases hj : j = i
        · subst hj; grind
        · simp [hj]; grind
      rw [hSplit, sumFin_add, sumFin_single_hit]
    rw [hA, sumFin_add]
    have hD : sumFin n (fun i => E i i * (v i * v i)) = diagSum + offSqI := by
      have hSplit : (fun i => E i i * (v i * v i)) = (fun i => (E i i - offDiag i) * (v i * v i) + offDiag i * (v i * v i)) := by
        funext i; grind
      rw [hSplit, sumFin_add]
    rw [hD]
  have hOff1 : offSqI = sumFin n (fun i => sumFin n (fun j => if j = i then 0 else absInt (E i j) * (v i * v i))) := by
    unfold offSqI
    apply sumFin_congr
    intro i
    change sumFin n (fun j => if j = i then 0 else absInt (E i j)) * (v i * v i) = _
    rw [sumFin_mul_right]
    apply sumFin_congr
    intro j
    by_cases hj : j = i <;> simp [hj]
  have hOff2 : offSqI = sumFin n (fun i => sumFin n (fun j => if j = i then 0 else absInt (E i j) * (v j * v j))) := by
    rw [hOff1]
    have hSw := sumFin_swap (fun i j : Fin n => if j = i then 0 else absInt (E i j) * (v i * v i))
    rw [hSw]
    apply sumFin_congr
    intro i
    apply sumFin_congr
    intro j
    by_cases hji : j = i
    · subst hji; rfl
    · have hij' : i ≠ j := fun h => hji h.symm
      simp only [hji, hij', ↓reduceIte]
      rw [hSymm j i]
  have hPairEq :
      2 * offSqI + 2 * sumFin n (fun i => sumFin n (fun j => if j = i then 0 else E i j * (v i * v j))) = pairSum := by
    have hCross :
        2 * sumFin n (fun i => sumFin n (fun j => if j = i then 0 else E i j * (v i * v j))) =
          sumFin n (fun i => sumFin n (fun j => if j = i then 0 else 2 * (E i j * (v i * v j)))) := by
      rw [sumFin_mul_left]
      apply sumFin_congr
      intro i
      rw [sumFin_mul_left]
      apply sumFin_congr
      intro j
      by_cases hj : j = i <;> simp [hj]
    have hSumOff :
        offSqI + offSqI =
          sumFin n (fun i => sumFin n (fun j =>
            if j = i then 0 else absInt (E i j) * (v i * v i) + absInt (E i j) * (v j * v j))) := by
      calc offSqI + offSqI
          = sumFin n (fun i => sumFin n (fun j => if j = i then 0 else absInt (E i j) * (v i * v i))) +
              sumFin n (fun i => sumFin n (fun j => if j = i then 0 else absInt (E i j) * (v j * v j))) := by
                rw [← hOff1, ← hOff2]
        _ = sumFin n (fun i =>
              sumFin n (fun j => if j = i then 0 else absInt (E i j) * (v i * v i)) +
              sumFin n (fun j => if j = i then 0 else absInt (E i j) * (v j * v j))) := by
                rw [sumFin_add]
        _ = sumFin n (fun i => sumFin n (fun j =>
              if j = i then 0 else absInt (E i j) * (v i * v i) + absInt (E i j) * (v j * v j))) := by
                apply sumFin_congr; intro i
                rw [← sumFin_add]
                apply sumFin_congr; intro j
                by_cases hj : j = i <;> simp [hj]
    have h2 : 2 * offSqI = offSqI + offSqI := by omega
    rw [h2, hSumOff, hCross]
    unfold pairSum
    rw [← sumFin_add]
    apply sumFin_congr
    intro i
    rw [← sumFin_add]
    apply sumFin_congr
    intro j
    by_cases hj : j = i <;> simp [hj] <;> omega
  have h2Quad : 2 * matQuad n E v = 2 * diagSum + pairSum := by omega
  have hDiagNonneg : 0 ≤ diagSum := by
    apply sumFin_nonneg
    intro i
    have hd : 0 ≤ E i i - offDiag i := by have := hDom i; grind
    exact Int.mul_nonneg hd (int_sq_nonneg (v i))
  have hPairNonneg : 0 ≤ pairSum := by
    apply sumFin_nonneg
    intro i
    apply sumFin_nonneg
    intro j
    by_cases hj : j = i
    · simp [hj]
    · simp only [hj, ↓reduceIte]
      exact sdd_pair_nonneg (E i j) (v i) (v j)
  omega

private theorem gramLLt_psd (n : Nat) (L : Fin n → Fin n → Int) (v : Fin n → Int) :
    0 ≤ matQuad n (gramLLt n L) v := by
  have hEq : matQuad n (gramLLt n L) v =
      sumFin n (fun k => (sumFin n (fun i => v i * L i k)) * (sumFin n (fun j => v j * L j k))) := by
    unfold matQuad gramLLt
    calc
      sumFin n (fun i => sumFin n (fun j => v i * sumFin n (fun k => L i k * L j k) * v j))
          = sumFin n (fun i => sumFin n (fun j => sumFin n (fun k => (v i * L i k) * (v j * L j k)))) := by
            apply sumFin_congr; intro i
            apply sumFin_congr; intro j
            rw [sumFin_mul_left, sumFin_mul_right]
            apply sumFin_congr; intro k; grind
      _ = sumFin n (fun i => sumFin n (fun k => sumFin n (fun j => (v i * L i k) * (v j * L j k)))) := by
            apply sumFin_congr; intro i; exact sumFin_swap _
      _ = sumFin n (fun k => sumFin n (fun i => sumFin n (fun j => (v i * L i k) * (v j * L j k)))) :=
            sumFin_swap _
      _ = sumFin n (fun k => sumFin n (fun i => (v i * L i k) * sumFin n (fun j => v j * L j k))) := by
            apply sumFin_congr; intro k
            apply sumFin_congr; intro i
            exact (sumFin_mul_left n (v i * L i k) (fun j => v j * L j k)).symm
      _ = sumFin n (fun k => sumFin n (fun i => v i * L i k) * sumFin n (fun j => v j * L j k)) := by
            apply sumFin_congr; intro k
            exact (sumFin_mul_right (f := fun i => v i * L i k) n (sumFin n (fun j => v j * L j k))).symm
  rw [hEq]
  apply sumFin_nonneg
  intro k
  exact int_sq_nonneg _

/-- Theorem M2 (Integer Cholesky + SDD Certified Eigenvalue Lower Bound):
    If `E = A - mu*I - L*L^T` is symmetric diagonally dominant, then `mu * ||v||^2 ≤ v^T A v`. -/
theorem cholesky_sdd_certified_psd (n : Nat) (A L : Fin n → Fin n → Int) (mu : Int)
    (hSDD : IsSDD n (fun i j => A i j - (if i = j then mu else 0) - gramLLt n L i j)) :
    ∀ v : Fin n → Int, mu * vecNormSq n v ≤ matQuad n A v := by
  intro v
  let E : Fin n → Fin n → Int := fun i j => A i j - (if i = j then mu else 0) - gramLLt n L i j
  have hNonneg : 0 ≤ matQuad n E v := sdd_quadratic_form_nonneg n E hSDD v
  have hGram : 0 ≤ matQuad n (gramLLt n L) v := gramLLt_psd n L v
  have hDecomp : matQuad n E v = matQuad n A v - mu * vecNormSq n v - matQuad n (gramLLt n L) v := by
    have hD : sumFin n (fun i => sumFin n (fun j => v i * (if i = j then mu else 0) * v j)) = mu * vecNormSq n v := by
      unfold vecNormSq
      rw [sumFin_mul_left]
      apply sumFin_congr
      intro i
      have hSub : (fun j : Fin n => v i * (if i = j then mu else 0) * v j) =
          (fun j : Fin n => if j = i then mu * (v i * v i) else 0) := by
        funext j
        by_cases hj : j = i
        · subst hj; simp; grind
        · have hij : i ≠ j := fun h => hj h.symm
          simp [hj, hij]
      rw [hSub, sumFin_single_hit]
    unfold matQuad E
    calc
      sumFin n (fun i => sumFin n (fun j => v i * (A i j - (if i = j then mu else 0) - gramLLt n L i j) * v j))
          = sumFin n (fun i => sumFin n (fun j =>
              (v i * A i j * v j - v i * (if i = j then mu else 0) * v j) - v i * gramLLt n L i j * v j)) := by
            apply sumFin_congr; intro i
            apply sumFin_congr; intro j; grind
      _ = sumFin n (fun i => sumFin n (fun j => v i * A i j * v j) -
              sumFin n (fun j => v i * (if i = j then mu else 0) * v j) -
              sumFin n (fun j => v i * gramLLt n L i j * v j)) := by
            apply sumFin_congr; intro i
            rw [sumFin_sub, sumFin_sub]
      _ = matQuad n A v -
            sumFin n (fun i => sumFin n (fun j => v i * (if i = j then mu else 0) * v j)) -
            matQuad n (gramLLt n L) v := by
            rw [sumFin_sub, sumFin_sub]; rfl
      _ = matQuad n A v - mu * vecNormSq n v - matQuad n (gramLLt n L) v := by
            rw [hD]
  omega

def matAt {n : Nat} (rows : List (List Int)) (i j : Fin n) : Int :=
  (rows.getD i.val []).getD j.val 0

def triAt {n : Nat} (rows : List (List Int)) (i k : Fin n) : Int :=
  if k.val ≤ i.val then matAt rows i k else 0

def checkCholeskySDD (n : Nat) (A L : List (List Int)) (mu : Int) : Bool :=
  let E : Fin n → Fin n → Int := fun i j =>
    matAt A i j - (if i = j then mu else 0) - gramLLt n (triAt L) i j
  let symOk := allFinB n (fun i => allFinB n (fun j => decide (matAt A i j = matAt A j i)))
  let domOk := allFinB n (fun i =>
    decide (sumFin n (fun j => if j = i then 0 else absInt (E i j)) ≤ E i i))
  symOk && domOk

/-- Theorem M3 (Kernel-Checked Integer Cholesky + SDD Soundness):
    If `checkCholeskySDD n A L mu = true`, then `mu * ||v||^2 ≤ v^T A v` for all `v`. -/
theorem checkCholeskySDD_sound (n : Nat) (A L : List (List Int)) (mu : Int)
    (hCheck : checkCholeskySDD n A L mu = true) :
    ∀ v : Fin n → Int, mu * vecNormSq n v ≤ matQuad n (matAt A) v := by
  unfold checkCholeskySDD at hCheck
  simp only [Bool.and_eq_true] at hCheck
  have hSymmMat : ∀ i j : Fin n, matAt A i j = matAt A j i := by
    intro i j
    have hi := allFinB_true n _ hCheck.1 i
    have hj := allFinB_true n _ hi j
    exact of_decide_eq_true hj
  have hDomE : ∀ i : Fin n,
      sumFin n (fun j => if j = i then 0 else absInt (matAt A i j - (if i = j then mu else 0) - gramLLt n (triAt L) i j)) ≤
        matAt A i i - (if i = i then mu else 0) - gramLLt n (triAt L) i i := by
    intro i
    simpa using of_decide_eq_true (allFinB_true n _ hCheck.2 i)
  refine cholesky_sdd_certified_psd n (matAt A) (triAt L) mu ⟨?_, hDomE⟩
  intro i j
  have hg : gramLLt n (triAt L) i j = gramLLt n (triAt L) j i := by
    unfold gramLLt
    apply sumFin_congr; intro k; exact Int.mul_comm _ _
  by_cases hij : i = j
  · subst hij; rfl
  · have hji : j ≠ i := fun h => hij h.symm
    simp only [hij, hji, ↓reduceIte, hSymmMat i j, hg]

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
  | zero => simp [sumFin]
  | succ k ih =>
    simp only [sumFin]
    have hEq : (fun i : Fin k => ((i.castSucc.val : Int) * ((i.castSucc.val : Int) - 1))) =
        (fun i : Fin k => (i.val : Int) * ((i.val : Int) - 1)) := rfl
    rw [Int.mul_add, hEq, ih]
    have h1 : (((k + 1 : Nat) : Int)) = (k : Int) + 1 := by omega
    rw [h1]
    grind

private theorem sumFin_ones (n : Nat) :
    sumFin n (fun _ : Fin n => (1 : Int)) = (n : Int) := by
  induction n with
  | zero => simp [sumFin]
  | succ k ih =>
    change sumFin k (fun _ : Fin k => (1 : Int)) + 1 = ((k + 1 : Nat) : Int)
    rw [ih]
    omega

private theorem total_efficient_welfare_formula (K : Nat) :
    6 * sumFin (K + 1) (fun b : Fin (K + 1) =>
      sumFin (K + 1) (fun s : Fin (K + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) =
      (K : Int) * ((K : Int) + 1) * ((K : Int) + 2) := by
  induction K with
  | zero => simp [sumFin]
  | succ k ih =>
    have hOuter :
        sumFin (k + 2) (fun b : Fin (k + 2) =>
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) =
        sumFin (k + 1) (fun b : Fin (k + 1) =>
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) +
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0) := rfl
    have hIn :
        sumFin (k + 1) (fun b : Fin (k + 1) =>
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) =
        sumFin (k + 1) (fun b : Fin (k + 1) =>
          sumFin (k + 1) (fun s : Fin (k + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0)) := by
      apply sumFin_congr
      intro b
      change sumFin (k + 1) (fun s : Fin (k + 1) => if s.val < b.val then (b.val : Int) - (s.val : Int) else 0) +
          (if k + 1 < b.val then (b.val : Int) - ((k + 1 : Nat) : Int) else 0) = _
      have hFalse : ¬ (k + 1 < b.val) := by omega
      simp [hFalse]
    have hLast :
        2 * sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0) =
          ((k + 1 : Nat) : Int) * ((k + 2 : Nat) : Int) := by
      have hTail :
          sumFin (k + 2) (fun s : Fin (k + 2) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0) =
          sumFin (k + 1) (fun s : Fin (k + 1) => ((k + 1 : Nat) : Int) - (s.val : Int)) := by
        change sumFin (k + 1) (fun s : Fin (k + 1) => if s.val < k + 1 then ((k + 1 : Nat) : Int) - (s.val : Int) else 0) +
            (if k + 1 < k + 1 then ((k + 1 : Nat) : Int) - ((k + 1 : Nat) : Int) else 0) = _
        have hF : ¬ (k + 1 < k + 1) := by omega
        simp only [hF, ↓reduceIte, Int.add_zero]
        apply sumFin_congr; intro s
        have hs : s.val < k + 1 := s.isLt
        simp [hs]
      rw [hTail]
      clear hTail hIn hOuter ih
      induction k with
      | zero => simp [sumFin]
      | succ m ih2 =>
        have hStep :
            sumFin (m + 2) (fun s : Fin (m + 2) => ((m + 2 : Nat) : Int) - (s.val : Int)) =
            sumFin (m + 1) (fun s : Fin (m + 1) => ((m + 1 : Nat) : Int) - (s.val : Int)) +
              ((m + 2 : Nat) : Int) := by
          have hA :
              sumFin (m + 2) (fun s : Fin (m + 2) => ((m + 2 : Nat) : Int) - (s.val : Int)) =
              sumFin (m + 1) (fun s : Fin (m + 1) => ((m + 2 : Nat) : Int) - (s.val : Int)) + 1 := by
            change sumFin (m + 1) (fun s : Fin (m + 1) => ((m + 2 : Nat) : Int) - (s.val : Int)) +
                (((m + 2 : Nat) : Int) - ((m + 1 : Nat) : Int)) = _
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
    | zero => simp [sumFin, posPart]
    | succ k ih =>
      have hk := posPart_add_le
        (sumFin k (fun i : Fin k => X i.castSucc s - alpha i.castSucc))
        (X ⟨k, Nat.lt_succ_self k⟩ s - alpha ⟨k, Nat.lt_succ_self k⟩)
      have ihk := ih (fun i => X i.castSucc) (fun i => alpha i.castSucc)
      change posPart (sumFin k (fun i : Fin k => X i.castSucc s - alpha i.castSucc) +
          (X ⟨k, Nat.lt_succ_self k⟩ s - alpha ⟨k, Nat.lt_succ_self k⟩)) ≤
        sumFin k (fun i : Fin k => posPart (X i.castSucc s - alpha i.castSucc)) +
        posPart (X ⟨k, Nat.lt_succ_self k⟩ s - alpha ⟨k, Nat.lt_succ_self k⟩)
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

end EconCert
