# ECON v1.0.0 Scientific Discovery & Certification Report

All numerical results below are measured directly by executing the four pure-stdlib experiment scripts in `experiments/` against `build/econ_cli` and `EconFastSession` (`ctypes` persistent session over `build/libecon.dylib`), with zero external numerical Python dependencies. Every emitted `.econcert.json` certificate passed exact 128-bit integer `cert-verify`.

---

## 1. Experiment 1 — Legacy `SNIPER` Mid-Price Parity vs `ECON` Bid-Ask + Tax + Convexity Audit

Script: [exp1_sniper_vs_econ_arb.py](exp1_sniper_vs_econ_arb.py) -> [exp1_sniper_vs_econ_arb.json](exp1_sniper_vs_econ_arb.json)  
Total certificates verified: **9 / 9 `cert-verify` PASS**.

### 1A. Clean Spread-Noise Chains (False-Positive Rate vs Bid-Ask Half-Spread)

| Half-Spread (pts) | Legacy `SNIPER` Alerts | `SNIPER` False Positives | `ECON` Arbitrage Alerts | `ECON` False Positives |
| ---: | ---: | ---: | ---: | ---: |
| 1.0 | 0 / 7 | 0 | 0 / 7 | **0** |
| 2.0 | **7 / 7** | **7** | 0 / 7 | **0** |
| 3.0 | **7 / 7** | **7** | 0 / 7 | **0** |
| 4.0 | **7 / 7** | **7** | 0 / 7 | **0** |
| 5.0 | **7 / 7** | **7** | 0 / 7 | **0** |

### 1B. Benchmark & Embedded Arbitrage Contrast

| Benchmark Scenario | `arb_found` | Best Mode | Verified Edge (pts) | `SNIPER` Flags | `SNIPER` FP | `SNIPER` Blind FN | Butterfly Violations |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| `taifex_txo_clean.csv` | 0 | `NO_ARB` | 0.0000 | **7** | **7** | 0 | 0 |
| `taifex_txo_arb_conv.csv` | 1 | `CONVERSION` (`M9`) | **+11.0494** | 1 | 0 | 0 | 0 |
| `synthetic_reversal.csv` | 1 | `REVERSAL` (`M9`) | **+12.0494** | 1 | 0 | 0 | 0 |
| `taifex_txo_arb_butterfly.csv` | 1 | `BUTTERFLY` (`M10`) | **+20.1500** | **0** | 0 | **1** | **5** |

### Key Discovery
- **Bid-Ask & Convexity Gap Quantified**: Naive mid-price parity triggers **100% false positives (7/7)** on noisy wide-spread chains whenever half-spread reaches `>= 2.0` pts (`CertEconSniperUpgrade.lean`), while simultaneously suffering **100% blindness (`0` flags)** on multi-strike Butterfly Convexity arbitrage where Put-Call parity holds at every individual strike (`taifex_txo_arb_butterfly.csv`, `+20.15` pts guaranteed pointwise across all $S \in \mathbb{Z}$, proved in `CertEconArbBidAsk.lean` via Theorem `M10`).
- **Ring-0 Silicon Parity**: `Econ.lean` upgrades the 8-op mid-price `.fir` into the 18-op Pure-GPR Bid-Ask + Fee circuit (`econ_bidask_arb.fir`, `0 br/blr/div/FPU/SIMD`), projecting cleanly to `124,242 B` `sky130` GDSII (`DRC=0, LVS=0`) via `../ELE/build/ele_bootstrap`.

---

## 2. Experiment 2 — Model-Free Pricing Hierarchy (`Fréchet` vs `2-Period MOT LP` vs `Moment-SOS` vs `BS`)

Script: [exp2_mot_sos_hierarchy.py](exp2_mot_sos_hierarchy.py) -> [exp2_mot_sos_hierarchy.json](exp2_mot_sos_hierarchy.json)  
Total certificates verified: **14 / 14 `cert-verify` PASS** (`M11` FTAP duals + `M12` Hankel SDD).

| Market Regime | Payoff (`param`) | Fréchet Bracket (`Tier 1`) | 2-Period MOT Bracket (`Tier 2`) | Width Ratio (`MOT / Fréchet`) | Black-Scholes Ref | Structural Discovery |
| --- | --- | --- | --- | ---: | ---: | --- |
| `SPX Skew` | `fwd_start` (0.90) | `[10.970, 15.814]` | **`[11.166, 13.687]`** | **0.5203** | 10.941 | BS `< MOT LB` (`-0.225` skew floor breach) |
| `SPX Skew` | `fwd_start` (1.00) | `[4.410, 10.591]` | **`[5.082, 7.607]`** | **0.4085** | 4.386 | BS `< MOT LB` (`-0.696` skew floor breach) |
| `SPX Skew` | `fwd_start` (1.10) | `[0.728, 6.496]` | **`[0.979, 3.846]`** | **0.4970** | 1.231 | MOT strictly encloses OTM BS inside `[0.979, 3.846]` |
| `SPX Skew` | `var_swap` (1.00) | `[172.73, 747.65]` | **`[278.98, 349.93]`** | **0.1234** | 242.00 | **87.7% width collapse**; BS below MOT floor |
| `SPX Skew` | `lookback` (1.00) | `[7.550, 10.641]` | **`[7.882, 9.253]`** | **0.4438** | 11.470 | Flat-vol BS overshoots MOT ceiling (`9.253`) |
| `BTC Fat-Tail` | `fwd_start` (1.00) | `[8.400, 19.132]` | **`[9.585, 14.231]`** | **0.4329** | 11.529 | `-56.7%` ambiguity cut under fat-tail symmetry |
| `BTC Fat-Tail` | `var_swap` (1.00) | `[642.20, 2816.09]` | **`[919.16, 1415.73]`** | **0.2284** | 1682.00 | Constant-vol BS exceeds MOT ceiling (`1415.73`) |
| `BTC Fat-Tail` | `lookback` (1.00) | `[13.840, 19.206]` | **`[14.444, 16.940]`** | **0.4652** | 30.058 | Discrete jump path ceiling far below continuous BS |

- **Key Discovery**: Enforcing dynamic martingale no-arbitrage (`Tier 2 MOT`) cuts model-free pricing ambiguity by **48%–88%** across forward-start, lookback, and variance swap contracts (`0.1234x` on SPX Variance Swap) without making any parametric stochastic-volatility assumption. On skewed SPX marginals, constant-IV Black-Scholes (`4.386` ATM Forward-Start; `242.0` Var Swap) falls strictly below the model-free `MOT` arbitrage lower bound (`5.082`; `278.98`), exposing exact parametric model mispricing.

---

## 3. Experiment 3 — Gaussian Copula Underestimation Multiplier vs Copula-Free DRO & Cantelli Ruin

Script: [exp3_copula_model_risk.py](exp3_copula_model_risk.py) -> [exp3_copula_model_risk.json](exp3_copula_model_risk.json)  
Total certificates verified: **23 / 23 `cert-verify` PASS** (`M13b` Cantelli + `M14` Comonotonic CVaR + `M15` Copula Obstruction).

### 3A. Portfolio Dimension Scaling (`Pareto tail = 2.3`, `corr = 0.15`, `alpha = 0.98`)

| Assets (`d`) | Gaussian Copula CVaR | Comonotonic CVaR UB (`M14`) | Moment-DRO CVaR UB (`M13`) | Copula Underestimation Multiplier | Moment / Gauss Ratio |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | 7.930 | 12.551 | 12.888 | **1.58x** | 1.63x |
| 3 | 9.872 | 18.826 | 17.067 | **1.91x** | 1.73x |
| 5 | 12.883 | 31.377 | 24.951 | **2.44x** | 1.94x |
| 8 | 17.469 | 50.203 | 36.330 | **2.87x** | 2.08x |
| 10 | 20.620 | **62.754** | 43.794 | **3.04x** | 2.12x |

### 3B. Benchmark Ruin & Reserve Capital Contrast (`target_ruin = 0.005`)

| Benchmark | Gauss CVaR | Comon / Mom-DRO UB | Gauss Ruin Prob | Cantelli Ruin UB (`M13b`) | Ruin Probability Gap | Reserve Capital (`Gauss` vs `Cantelli`) |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `danish_fire_heavy_tail.json` | 16.042 | **26.543** (`1.65x`) | $1.824 \times 10^{-8}$ | **$3.192 \times 10^{-2}$** | **$1.75 \times 10^6\times$** | `14.695` -> **`67.033`** (**4.56x**) |
| `credit_portfolio_5d.json` | 13.861 | **32.080** (`2.31x`) | $2.827 \times 10^{-7}$ | **$3.842 \times 10^{-2}$** | **$1.36 \times 10^5\times$** | `18.388` -> **`83.062`** (**4.52x**) |
| `copula_obstruction_2d.json` | 17.700 | **29.763** (`1.68x`) | $1.590 \times 10^{-5}$ | **$5.462 \times 10^{-2}$** | **$3.44 \times 10^3\times$** | `16.058` -> **`73.857`** (**4.60x**) |

- **Key Discovery**: Even at low pairwise correlation ($\rho = 0.15$), Gaussian copula CVaR underestimates worst-case model-free portfolio tail loss by **1.58x (`d=2`) -> 3.04x (`d=10`)**, while Normal-approximation ruin models on Danish Fire heavy-tail risk understate certified Cantelli ruin probability by **six orders of magnitude** ($1.82 \times 10^{-8}$ vs $3.19 \times 10^{-2}$). Theorem `M15` (`copula_correlation_obstruction`) constructs an explicit 3×3 counterexample proving identical covariance gives `0` vs `1/12` joint crash probability.

---

## 4. Experiment 4 — Fire-Sale Contagion Bifurcation (`EconFastSession`) & Discrete Myerson-Satterthwaite Phase Transition

Script: [exp4_firesale_tipping_and_ms.py](exp4_firesale_tipping_and_ms.py) -> [exp4_firesale_tipping_and_ms.json](exp4_firesale_tipping_and_ms.json)  
Total solves: **54 clearing sweeps + 7 MS LPs (`10/10 cert-verify PASS`)**.

### 4A. Core-Periphery Fire-Sale Phase Cliff (`ecb_core_periphery_12.json`, `shock_scale = 1.0`)

| `alpha_fire` | Greatest Eq `p_upper` | Least Eq `p_lower` | Multi-Equilibrium Gap (`M16`) | Systemic Shortfall | Regime State |
| ---: | ---: | ---: | ---: | ---: | --- |
| 0.0 | 308.30 | 248.87 | 59.43 | 88.13 | Mild Multi-Equilibrium |
| 1.0 | 305.21 | 137.81 | 167.40 | 199.19 | Widening Fragility Wedge |
| **2.0** | **301.84** | 43.64 | **258.20** | 293.36 | **Pre-Cliff Solvency Basin Exists** |
| **2.5** | **24.50** | **24.50** | **0.00** | **312.50** | **DISCONTINUOUS COLLAPSE CLIFF** |
| 4.0 | 4.83 | 4.83 | 0.00 | 332.17 | Unique Insolvent Regime |

- Under a `15%` external asset haircut (`shock_scale = 0.85`), the discontinuous collapse cliff shifts downward from `alpha_fire in (2.0, 2.5]` to **`alpha_fire in (0.5, 1.0]`** (`p_upper` plunges from `301.67` to `106.86`). Across the full network suite, SEARU adversarial shock reallocation strictly dominates uniform shocks (`319.50 -> 323.03` shortfall on `ecb12`; `43.43 -> 45.31` on `ring6`), while Theorem `M18` certifies exact minimum bailouts restoring 100% solvency (`3.30`, `28.70`, `91.80`).

### 4B. Discrete Myerson-Satterthwaite Impossibility Phase Transition (`M7`, $K = 2 \dots 8$)

| Grid `K` | Efficient Welfare `W` | Min IC+IR Rent Required | Max Budget Surplus `S(K)` | Normalized Ratio `S / (K(K+1)^2)` | Exact Formula `(4-K)/(6(K+1))` | Second-Best Efficiency |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 2 | 4.0 | 2.0 | **+2.0** | +0.111111 | +0.111111 | 100.00% |
| 3 | 10.0 | 8.0 | **+2.0** | +0.041667 | +0.041667 | 100.00% |
| **4** | 20.0 | 20.0 | **0.0** | **0.000000** | **0.000000** | **100.00% (Boundary)** |
| **5** | 35.0 | 40.0 | **-5.0** | **-0.027778** | **-0.027778** | **95.24% (`Impossibility`)** |
| 6 | 56.0 | 70.0 | **-14.0** | **-0.047619** | **-0.047619** | **93.75%** |
| 7 | 84.0 | 112.0 | **-28.0** | **-0.062500** | **-0.062500** | **93.33%** |
| 8 | 120.0 | 168.0 | **-48.0** | **-0.074074** | **-0.074074** | **93.33%** |

- **Exact Discrete Obstruction Proof**: Both the C17 Two-Phase Simplex LP and Lean Theorem `M7` (`myerson_satterthwaite_bilateral_obstruction`) establish the exact discrete phase boundary **`K = 4 -> 0` vs `K >= 5 -> strictly negative deficit`**, matching $\frac{4 - K}{6(K+1)} \xrightarrow{K \to \infty} -\frac{1}{6}$ to 8+ digits, while the Second-Best budget-balanced LP settles at finite-grid optimal efficiency converging toward the continuous Myerson-Satterthwaite bound.
