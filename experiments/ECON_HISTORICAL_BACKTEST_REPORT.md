# ECON v2.0.0 — Historical Market Stress Calibration & Backtest Report (`exp9`)

- **Exact `__int128` Certificates Verified**: `8/8` (`econ_cli cert-verify` + `lean -DwarningAsError=true` + `leanchecker`, `0 sorryAx`, `0 Lean.ofReduceBool`)

## 1. Episode 1 — TAIFEX TXO 2018-02-06 Dislocation vs. 2024 Wide-Spread Regime

| Regime | Strikes | Bid-Ask Arb Found | Best Edge (pts) | Butterfly Count | Legacy SNIPER False Positives |
| --- | ---: | ---: | ---: | ---: | ---: |
| `taifex_txo_20180206_stress.csv` | 9 | 1 | `92.04` | 11 | 0 |
| `taifex_txo_2024_normal.csv` | 7 | 0 | `0.00` | 0 | 7 |

## 2. Episode 2 — 2021-05-19 BTC Perpetual Deleveraging Spiral

- **Safe Interior Band (`theta_mm = 0.12`)**: `P_eq = 39.56 kUSD`, `liquidated = 0/8`, `bad_debt = 0.0`, `macro_safety_margin = 0.1943`, `paradox_detected = 0`.
- **Over-Tight Trigger (`theta_mm = 0.25`)**: `P_eq = 18.00 kUSD`, `liquidated = 8/8`, `bad_debt = 4270.0`, `macro_safety_margin = -0.9803`, `paradox_detected = 1` (interior optimum `theta_opt = 0.080`).

## 3. Episode 3 — 1980–1990 Empirical Danish Fire Heavy-Tail Portfolio (`alpha = 0.99`, `tail = 1.85`)

- **Gaussian VaR / CVaR vs. Copula/Moment DRO**: `gauss_VaR = 27.556`, `gauss_CVaR = 33.161`, `comonotonic_CVaR_UB = 47.482` (`1.43x`), `moment_DRO_CVaR_UB = 65.505` (`1.98x`), `corr_psd_ok = 1`.
- **Cantelli Ruin Amplification**: `gauss_ruin_prob = 1.310e-07` vs. `cantelli_ruin_ub = 0.0363` (`277552.8x`), `min_capital_cantelli = 82.66` vs. `min_capital_gauss = 22.86`.

## 4. Episode 4 — 2023 Regional-Bank HTM Duration Mismatch (Raw vs. Repaired DRC Sign-Off)

| Configuration | Verdict | `m_inc` (bps) | `m_dro` (bps) | `m_sys` (bps) | `M*` Master (bps) | Weakest |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| `RegionalBank_2023_HTM_Raw` | **VETOED** | -6325 | -44597 | -6211 | **-44597** | `DRO` |
| `RegionalBank_2023_ClawbackHedged_Repaired` | **CERTIFIED** | 19269 | 8880 | 7000 | **7000** | `SYS` |

- **Lean 4 Replay**: `CertHistBankRaw.lean` (3 thm: `inst_weakest_exploit_veto, inst_repair_check, inst_minimal_repair_sound`) and `CertHistBankRepaired.lean` (2 thm: `inst_safety_check, inst_3d_certified`) compiled with `-DwarningAsError=true` and verified by `leanchecker` (`0 sorryAx`, `0 Lean.ofReduceBool`).
