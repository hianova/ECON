# ECON v2.0.0 Institutional Safety-Margin Discovery & DRC Audit Report

All numerical findings below are measured directly by running `experiments/exp5..exp8` (pure Python stdlib + `EconEngine` / `EconFastSession` over `build/libecon.{dylib,so}` and `build/econ_cli`), covering Pillars 5–8 (`M19–M30`). For Pillars 1–4 Classical Model-Free Finance, DRO, Clearing & Mechanisms (`exp1–exp4`, `M1–M18`), see [ECON_DISCOVERY_REPORT.md](ECON_DISCOVERY_REPORT.md). Every emitted `.econcert.json` passed independent 128-bit integer `cert-verify`.

---

## 1. Experiment 5 — Agency 2D Phase Boundary ($\alpha \times \beta$) & Cantelli Tail Risk Amplification

Script: [exp5_agency_clawback_phase.py](exp5_agency_clawback_phase.py) -> [exp5_agency_clawback_phase.json](exp5_agency_clawback_phase.json)  
Total solves: **63 grid evaluations, 8 / 8 `cert-verify` PASS** (`M19–M21`).

### 1A. Critical Clawback Threshold $\beta^*(\alpha)$ vs Upside Share $\alpha$

| Profit Share $\alpha$ | Exact Critical Clawback $\beta^*(\alpha)$ | Limited-Liability IC Margin ($\beta=0$) | Certified IC at $\beta=0.35$ | Systemic Expected Return Gap ($\text{Safe} - \text{Risk}$) |
| ---: | ---: | ---: | ---: | ---: |
| `0.05` | `0.0380` | `-0.4450` (`VETOED`) | `+3.6500` (`IC=1`) | `+5.4750` |
| `0.10` | `0.0675` | `-0.7900` (`VETOED`) | `+3.3050` (`IC=1`) | `+5.4750` |
| `0.15` | `0.0970` | `-1.1350` (`VETOED`) | `+2.9600` (`IC=1`) | `+5.4750` |
| **`0.20` (2-and-20)** | **`0.1265`** | **`-1.4800` (`VETOED`)** | **`+2.6150` (`CERTIFIED`)** | **`+5.4750`** |
| `0.25` | `0.1560` | `-1.8250` (`VETOED`) | `+2.2700` (`IC=1`) | `+5.4750` |
| `0.30` | `0.1855` | `-2.1700` (`VETOED`) | `+1.9250` (`IC=1`) | `+5.4750` |
| `0.40` | `0.2444` | `-2.8600` (`VETOED`) | `+1.2350` (`IC=1`) | `+5.4750` |

### 1B. Model-Free Cantelli Ruin Amplification (`alpha = 0.20`, `fund_2and20`)

| Clawback Rate $\beta$ | $u_{\text{safe}}$ | $u_{\text{risk}}$ | IC Margin | Agent Choice | Cantelli Ruin UB (`M21`) | Ruin Amplification |
| ---: | ---: | ---: | ---: | --- | ---: | ---: |
| `0.00` (`raw`) | `+0.320` | `+1.800` | **`-1.480`** | **Risky (Tail-Seeking)** | **`35.83%`** (`0.3583`) | **`24.26x`** |
| `0.10` | `+0.290` | `+0.600` | **`-0.310`** | Risky (Tail-Seeking) | `35.83%` | `24.26x` |
| `0.15` | `+0.275` | `0.000` | **`+0.275`** | Safe | `1.48%` (`0.0148`) | Baseline (`1.0x`) |
| **`0.35` (`repaired`)** | `+0.215` | `-2.400` | **`+2.615`** | **Safe (`CERTIFIED`)** | **`1.48%`** | Baseline (`1.0x`) |

- **Discovery (`M19–M21`)**: Under zero-clawback limited liability ($\beta = 0$), standard 2-and-20 KPI convexity induces managers to reject a strategy with `+5.475` higher expected institutional return (`IC = -1.480`), inflating worst-case model-free Cantelli ruin probability by **24.26x** (`1.48% -> 35.83%`). Enforcing escrow/clawback beyond the exact linear boundary $\beta^*(\alpha)$ (`0.1265` at $\alpha = 0.20$) eliminates the convexity exploit with zero loss to safe participation.

---

## 2. Experiment 6 — Platform Take-Rate Death Spiral vs Screening Expansion (`M22–M24`)

Script: [exp6_platform_death_spiral.py](exp6_platform_death_spiral.py) -> [exp6_platform_death_spiral.json](exp6_platform_death_spiral.json)  
Total sweeps: **28 evaluations, 7 / 7 `cert-verify` PASS**.

| Take Rate $r$ | Surviving Tiers $k_{\text{eq}} / 10$ | Market Mass | Pool Price | Platform Revenue | Safety Margin $r^* - r$ | Market Regime |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `0.00` | `8 / 10` | `0.80` | `55.0` | `0.00` | `+0.25` | Zero-Fee Pool |
| `0.05` | `6 / 10` | `0.60` | `45.0` | `13.50` | `+0.20` | Healthy Multi-Tier |
| `0.10` | `5 / 10` | `0.50` | `40.0` | `20.00` | `+0.15` | Core Pool |
| **`0.15`** | **`5 / 10`** | **`0.50`** | `40.0` | **`30.00`** | `+0.10` | **Revenue Peak** |
| `0.20` | `4 / 10` | `0.40` | `35.0` | `28.00` | `+0.05` | Pre-Cliff Erosion |
| `0.22` | `3 / 10` | `0.30` | `30.0` | `19.80` | `+0.03` | Marginal Squeeze |
| **`0.25` ($r^*$)** | `1 / 10` | `0.10` | `20.0` | `5.00` | **`0.00`** | **Exact Critical Boundary** |
| **`0.26`** | **`0 / 10`** | **`0.00`** | **`0.0`** | **`0.00`** | **`-0.01`** | **DISCONTINUOUS DEATH SPIRAL (`M22`)** |
| `0.35–0.50` | `0 / 10` | `0.00` | `0.0` | `0.00` | `< 0` | Complete Market Extinction |

- **Screening Expansion (`M24`)**: Without quality separation (`insurance_death_spiral.json`, $r = 0.42$), pooled pricing unravels completely to `k_eq = 0` (`OBSTRUCTION`). Introducing a verifiable bond/deductible separating menu (`checkScreeningMenu_sound`) expands the certified take-rate ceiling from **`r* = 0.250`** to **`r*_screened = 0.400`** (`+1500 bps`).

---

## 3. Experiment 7 — Inverted-U Micro-Macro Margin Paradox & Supply-Chain Contagion (`M25–M27`)

Script: [exp7_margin_paradox_and_supplychain.py](exp7_margin_paradox_and_supplychain.py) -> [exp7_margin_paradox_and_supplychain.json](exp7_margin_paradox_and_supplychain.json)  
Total sweeps: **42 margin points + 10 supply-chain points, 8 / 8 `cert-verify` PASS**.

### 3A. Endogenous Maintenance Margin $\theta_{\text{mm}}$ Inverted-U Phase Diagram (`exchange_perp_8acct.json`)

| Shock | Maintenance Margin $\theta_{\text{mm}}$ | Equilibrium Price $P_{\text{eq}}$ | Liquidated Accounts | System Bad Debt | Macro Safety Margin | Micro-Macro Regime |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `0%` | `0.02` | `100.0` | `0 / 8` | `0.0` | `+0.0865` | Undermargined Jump Risk |
| `0%` | `0.06` | `100.0` | `0 / 8` | `0.0` | `+0.1465` | Narrow Cushion |
| **`0%`** | **`0.08 – 0.15`** | **`100.0`** | **`0 / 8`** | **`0.0`** | **`+0.1765` (Peak)** | **Certified Optimal Interior Band** |
| **`0%`** | **`0.18 – 0.45`** | **`10.0`** | **`8 / 8`** | **`34,000.0`** | **`-1.9739`** | **PARADOX CASCADE COLLAPSE (`M26`)** |
| `5%` | `0.02` | `95.0` | `0 / 8` | `0.0` | `+0.0276` | Undermargined |
| **`5%`** | **`0.08 – 0.10`** | **`95.0`** | **`0 / 8`** | **`0.0`** | **`+0.1176` (Peak)** | **Stressed Optimal Band Shrinks** |
| `5%` | `>= 0.12` | `10.0` | `8 / 8` | `34,000.0` | `-1.9739` | Earlier Paradox Cliff |

### 3B. Supply-Chain Payables Stretching $\tau$ (`supply_chain_tier3_10firm.json`)

| Horizon $\tau$ | Core Apparent WC Gain | Total Chain Payables | `M27` Shock Horizon Margin | Uniform Shortfall | SEARU Adversarial Shortfall | Systemic Safety Margin |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `0.50` | `-18.0` | `64.5` | **`+3.00`** | `0.00` | **`0.00`** | **`+0.0465` (SAFE)** |
| `0.75` | `-9.0` | `96.75` | **`+3.00`** | `0.00` | **`0.00`** | **`+0.0310` (SAFE)** |
| `1.00` | `0.0` | `129.0` | `+3.00` | `0.00` | `28.22` | `-0.4142` |
| `1.50` | `+18.0` | `193.5` | `-2.00` | `3.97` | `128.00` | `-1.3334` |
| **`2.50`** | **`+54.0`** | `322.5` | **`-14.00`** | `211.65` | **`272.84`** (`10/10 def`) | **`-1.7354`** |
| `3.50` | **`+90.0`** | `451.5` | **`-26.00`** | `362.54` | **`405.58`** | **`-1.8542`** |

- **Discovery**: Raising single-account maintenance margins (`0.12 -> 0.30`) or stretching core payables (`0.8 -> 2.5`) **improves local micro KPIs while plunging systemic macro safety margins sharply negative** (`+0.1765 -> -1.9739` on perp clearing; `+54.0` core WC illusion vs `272.84` SEARU supply-chain collapse).

---

## 4. Experiment 8 — Unified 3D Institutional DRC / LVS Audit (`M28–M30`)

Script: [exp8_institution_drc_audit.py](exp8_institution_drc_audit.py) -> [exp8_institution_drc_audit.json](exp8_institution_drc_audit.json)  
Total audits: **8 commercial institution specs (4 pairs), 8 / 8 `cert-verify` PASS**.

| Commercial Institution | Variant | $m_{\text{inc}}$ (bps) | $m_{\text{dro}}$ (bps) | $m_{\text{sys}}$ (bps) | Master $M^*$ (bps) | Verdict | Weakest Link | Min Repair Cost |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- | ---: |
| `AlphaCap_2and20` | Raw ($\beta=0$) | `-7400` | `-34785` | `-5063` | **`-34785`** | **`VETOED`** | `DRO` | `67503.10` |
| `AlphaCap_2and20` | Repaired ($\beta=0.35$, buf) | `+13075` | `+8770` | `+7000` | **`+7000`** | **`CERTIFIED`** | `SYS` | `0.00` |
| `BazaarX_Platform` | Raw (take `42%`) | `-1700` | `-4187` | `-1500` | **`-4187`** | **`VETOED`** | `DRO` | `9327.00` |
| `BazaarX_Platform` | Repaired (screen + `15%`) | `+2500` | `+8603` | `+3500` | **`+2500`** | **`CERTIFIED`** | `INC` | `0.00` |
| `PerpDEX_Margin` | Raw ($\theta=0.30$) | `+600` | `-5473` | `-19739` | **`-19739`** | **`VETOED`** | `SYS` | `47645.60` |
| `PerpDEX_Margin` | Repaired ($\theta=0.12$) | `+600` | `+5843` | `+1765` | **`+600`** | **`CERTIFIED`** | `INC` | `0.00` |
| `MegaOEM_Supply` | Raw ($\tau=2.5$) | `-100` | `-4644` | `-17354` | **`-17354`** | **`VETOED`** | `SYS` | `37881.20` |
| `MegaOEM_Supply` | Repaired ($\tau=0.8$, buf) | `+800` | `+8746` | `+872` | **`+800`** | **`CERTIFIED`** | `INC` | `0.00` |

- **Weakest-Link Law (`M29`) in Action**: `PerpDEX_TightMM_Raw` already had positive fee/incentive margin (`+600` bps), yet hair-trigger liquidation (`theta = 0.30`) collapsed $m_{\text{sys}}$ to `-19739` bps, forcing a `VETOED` DRC audit until `theta` moved inside the optimal band and DRO reserves expanded (`M^* = +600` bps, `CERTIFIED`).
