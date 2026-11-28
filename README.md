# ECON (`v2.0.0`)

[![CI](https://github.com/hianova/ECON/actions/workflows/ci.yml/badge.svg)](https://github.com/hianova/ECON/actions/workflows/ci.yml)

**Institutional Safety-Margin Theory & Formal Commercial DRC/LVS Sign-Off Engine**

`ECON v2.0.0` unifies formal economics, model-free finance, actuarial tail DRO, macro-prudential systemic clearing, contract/agency theory, adverse-selection phase transitions, and Ring-0 Pure-GPR execution around one central thesis:

$$\mathbf{M}(\mathcal{I}) = (m_{\text{inc}},\, m_{\text{dro}},\, m_{\text{sys}}), \qquad M^*(\mathcal{I}) = \min(m_{\text{inc}},\, m_{\text{dro}},\, m_{\text{sys}})$$

> Commercial institutions rarely fail merely because of exogenous luck; they fail when institutional rules drive one of the three safety margins below zero — strategic/incentive exploitability ($m_{\text{inc}}$), distributional model misspecification ($m_{\text{dro}}$), or systemic bifurcation/contagion cliffs ($m_{\text{sys}}$).

`ECON` acts as a **Formal Commercial DRC / LVS Sign-Off & Red-Team Auditor**, pairing a **30-theorem Lean 4 kernel** ([`lean/EconCert.lean`](lean/EconCert.lean), `M1–M30`, zero `sorryAx`, zero `Lean.ofReduceBool`, audited by `leanchecker`) with a portable C17 Zero-FP-Trust `__int128` verification core, a Ring-0 Pure-GPR TAIFEX hardware layer ([`Econ.lean`](Econ.lean) absorbing and upgrading legacy `SNIPER`), and 8 end-to-end discovery experiments.

---

## 1. Architecture Overview (55% Formal Theory / 35% C17 Core / 10% Ring-0)

| Layer | Module | Role & Certified Guarantees |
| :--- | :--- | :--- |
| **Formal Kernel (`M1–M30`)** | [`lean/EconCert.lean`](lean/EconCert.lean) (`import Flow.Interval`, `Flow.CholeskySDD`) | Core Cholesky-SDD & LP Duality (`M1–M4`), VCG DSIC/IR & Discrete Myerson-Satterthwaite Phase Transition & Fisher Walras (`M5–M8`), Bid-Ask/Butterfly Arb & 2-Period MOT/SOS (`M9–M12`), Cantelli Ruin & Copula-Free CVaR DRO & Covariance Obstruction (`M13–M15`), Eisenberg-Noe Tarski Bracket & Fire-Sale Dominance & Bailout (`M16–M18`), Limited-Liability Convexity vs Clawback IC & Cantelli Amplification (`M19–M21`), Akerlof Death Spiral & Platform Take-Rate Margin & Screening Separation (`M22–M24`), Margin-Liquidation Paradox Witness & Supply-Chain Credit Horizon (`M25–M27`), 3D Weakest-Link Dominance & Minimal Institutional Repair (`M28–M30`). |
| **Ring-0 Execution (`SNIPER` Absorbed)** | [`Econ.lean`](Econ.lean) (`128` lines, `0` local theorems, `import Flow`) | Replaces legacy mid-price parity with an **18-op Pure-GPR Bid-Ask + Fee Conversion/Reversal circuit** (`econ_bidask_arb.fir`, zero `br/blr/div/FPU/SIMD`), `320B` `econ_taifex.flow`, `64B` `econ_ring0_abi.h`, projecting to `124,242 B` `sky130` GDSII (`DRC=0, LVS=0`) via `../ELE/build/ele_bootstrap`. |
| **Zero-FP-Trust Core** | [`include/econ_cert.h`](include/econ_cert.h), [`include/econ_wire.h`](include/econ_wire.h), [`src/econ_cert.c`](src/econ_cert.c) | Shared `FLOW` FIPS-180-4 SHA-256 (`flow_sha256_io.h`), `__int128` Cholesky-SDD certifier (`flow_cholesky_sdd.h`), Jacobi eigensolver, Two-Phase Simplex LP with duals, canonical `.econcert.json` verifier, **freestanding `"ECN2"` canonical ULEB128 + zigzag + FNV-1a64 packed binary wire codec** (`~70–79 B` vs `~770 B` JSON, `ir/econ_wire.fspec`), and 14 standalone `CertEcon*.lean` theorem emitters. |
| **8 Pillar Solvers** | [`src/econ_fin.c`](src/econ_fin.c), [`src/econ_actuary.c`](src/econ_actuary.c), [`src/econ_clearing.c`](src/econ_clearing.c), [`src/econ_mech.c`](src/econ_mech.c), [`src/econ_agency.c`](src/econ_agency.c), [`src/econ_adverse.c`](src/econ_adverse.c), [`src/econ_margin.c`](src/econ_margin.c), [`src/econ_inst.c`](src/econ_inst.c) | Arbitrage & 3-Tier MOT/SOS (`fin`), Copula-Free DRO & Cantelli (`actuary`), Tarski Clearing & `../FLOW/core/searu.h` Shock Search (`clearing`), VCG / MS / Fisher (`mech`), Clawback Phase Audit (`agency`), Take-Rate Cliff & Screening (`adverse`), Micro-Macro Margin Paradox & Supply-Chain Horizon (`margin`), Unified 3D DRC/LVS Auditor `audit-inst` (`inst`). |
| **Python SDK, Oracles & Experiments** | [`oracles/`](oracles/), [`experiments/`](experiments/), [`data/historical/`](data/historical/) | Pure-stdlib `EconEngine` & persistent `ctypes` `EconFastSession` (`build/libecon.{dylib,so}`), independent golden verifier ([`ECON_VERIFICATION_REPORT.md`](ECON_VERIFICATION_REPORT.md)), `22/22` tamper rejections, `exp1–exp8` scientific discovery reports, and `exp9` historical stress backtest ([`ECON_HISTORICAL_BACKTEST_REPORT.md`](experiments/ECON_HISTORICAL_BACKTEST_REPORT.md)). |

---

## 2. Quick Start & Verification

```bash
make clean && make verify      # Gate 1 Hygiene + Gate 2 Ring-0/sky130 + Gate 3 Lean (M1-M30), 24/24 selftest, 14 CertEcon*, Oracle, 22/22 Tamper
make experiments               # Runs exp1..exp9 against econ_cli & EconFastSession -> experiments/*.json & reports
```

### Commercial Institutional DRC & Pillar CLI Examples

```bash
./build/econ_cli selftest
./build/econ_cli audit-inst benchmarks/inst/inst_exchange_margin_raw.json --cert build/exch_raw.econcert.json --emit-lean build/CertExchRaw.lean --name CertExchRaw
./build/econ_cli audit-inst benchmarks/inst/inst_exchange_margin_repaired.json --cert build/exch_rep.econcert.json
./build/econ_cli pack-wire benchmarks/inst/inst_exchange_margin_raw.json build/exch_raw.ecn2
./build/econ_cli unpack-wire build/exch_raw.ecn2 --json build/exch_raw_wire.json
./build/econ_cli agency-audit benchmarks/agency/fund_2and20_raw.json --beta 0.35
./build/econ_cli adverse-audit benchmarks/adverse/platform_marketplace.json --take-rate 0.25
./build/econ_cli margin-audit benchmarks/margin/exchange_perp_8acct.json --theta 0.12
./build/econ_cli supply-audit benchmarks/margin/supply_chain_tier3_10firm.json --tau 2.5 --searu
./build/econ_cli arb-scan benchmarks/fin/taifex_txo_arb_butterfly.csv
./build/econ_cli mot-bound benchmarks/fin/spx_2period_skew.csv --payoff var_swap
./build/econ_cli risk-dro benchmarks/actuary/danish_fire_heavy_tail.json
./build/econ_cli clear-net benchmarks/clearing/ecb_core_periphery_12.json --searu
```

---

## 3. Verified Highlights & Scientific Discoveries

- **Formal Verification ([`ECON_VERIFICATION_REPORT.md`](ECON_VERIFICATION_REPORT.md))**:
  - `M1–M30` Lean 4 kernel (`leanchecker` PASS) + `14 / 14` standalone `CertEcon*.lean` modules (`0 sorryAx`, `0 ofReduceBool`).
  - `28 / 28` C17 selftests, `32 / 32` canonical `.econcert.json` `__int128` certificates, `"ECN2"` canonical binary wire codec (`61–192 B`, strict truncation/tamper/non-canonical varint rejection), independent pure-stdlib Python golden oracle (`FIN(5) + ACT(3) + CLR(3) + MECH(4) + AG(2) + ADV(2) + MGN(2) + INST(8) + ECN2(8)`), `22 / 22` tamper rejections, and `124,242 B` `sky130` GDSII (`DRC=0, LVS=0`).
- **Classical Model-Free & Systemic Findings (`exp1–exp4`, [`experiments/ECON_DISCOVERY_REPORT.md`](experiments/ECON_DISCOVERY_REPORT.md))**:
  1. Legacy mid-price parity fires `7/7` false positives under `>= 2.0` pt spreads while missing `taifex_txo_arb_butterfly.csv` (`+20.15` pts guaranteed profit across all $S \in \mathbb{Z}$).
  2. 2-period MOT LP collapses SPX Variance Swap pricing ambiguity to **0.1234x** and catches constant-IV Black-Scholes breaching skew no-arbitrage floors.
  3. Gaussian copula CVaR underestimates copula-free tail risk up to **3.04x** (`d=10`), while Normal approximations understate exact Cantelli fire ruin by $1.75 \times 10^6\times$.
  4. Fire-sale clearing shows a sharp discontinuous cliff (`301.84 -> 24.50` across `alpha_fire in (2.0, 2.5]`), and discrete Myerson-Satterthwaite hits exact transition $K=4$ (`S=0`) to $K \ge 5$ (`S<0`).
- **Institutional Engineering & Paradox Findings (`exp5–exp8`, [`experiments/ECON_INSTITUTIONAL_REPORT.md`](experiments/ECON_INSTITUTIONAL_REPORT.md))**:
  5. **Limited-Liability Convexity**: Standard 2-and-20 (`beta=0`) induces tail-seeking (`IC = -1.48`), multiplying Cantelli ruin risk by **24.26x** (`1.48% -> 35.83%`); exact clawback threshold $\beta^*(0.20) = 0.1265$ restores `IC > 0`.
  6. **Platform Death Spiral**: Take-rate revenue peaks at `30.00` (`r=15%`), vanishes across `r* = 25% -> 26%` via discontinuous unraveling (`k_eq = 1 -> 0`), and expands to `40%` under verifiable screening (`M24`).
  7. **Inverted-U Margin Paradox & Supply-Chain Contagion**: Tightening maintenance margin beyond the interior optimal band (`theta in [0.08, 0.15]`, `0` bad debt) triggers a full 8-account cascade (`theta >= 0.18`, `34,000` bad debt, `P_eq: 100 -> 10`); stretching supply-chain payables ($\tau: 0.5 \to 2.5$) inflates core working capital (`+54.0`) while flipping `M27` horizon headroom negative (`-14.0`) and exploding SEARU shortfall to `272.84`.
  8. **End-to-End 3D DRC Audits**: All 4 commercial institutions flip from `VETOED` (`M* = -34785, -4187, -19739, -17354` bps) to `CERTIFIED` (`M* = +7000, +2500, +600, +800` bps) via minimal institutional repair (`M30`).
- **Historical Stress Calibration & Empirical Backtests (`exp9`, [`data/historical/`](data/historical/), [`experiments/ECON_HISTORICAL_BACKTEST_REPORT.md`](experiments/ECON_HISTORICAL_BACKTEST_REPORT.md))**:
  - Audits 4 offline stress episodes (`8/8` `__int128` `.econcert.json` + live `CertHistBankRaw.lean` / `CertHistBankRepaired.lean` `leanchecker` PASS): `taifex_txo_20180206_stress.csv` (`+92.04` pt butterfly arb across `11` triples) vs. `taifex_txo_2024_normal.csv` (`0` arb / `7` legacy FP suppressed); `perp_20210519_cascade.json` (`theta = 0.25`: `8/8` liquidated, `bad_debt = 4,270.0` vs. `theta = 0.12`: `0/8` liquidated, `0.0` bad debt); `danish_fire_empirical_1980_1990.json` (`1.98x` moment-DRO CVaR amplification, `277,552.8x` Cantelli ruin ratio); `inst_2023_bank_duration_raw.json` (`VETOED`, `M* = -44,597` bps) -> `inst_2023_bank_duration_repaired.json` (`CERTIFIED`, `M* = +7,000` bps).
