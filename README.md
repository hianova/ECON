# ECON (`v1.0.0`)

**Formal Zero-Floating-Point-Trust Economics, Model-Free Finance, Actuarial DRO, Systemic Clearing & Ring-0 Market Engine**

`ECON v1.0.0` pairs a formally verified Lean 4 mathematical kernel ([`lean/EconCert.lean`](lean/EconCert.lean), **18 theorems `M1–M18`**, zero `sorryAx`, zero `Lean.ofReduceBool`, audited by `leanchecker`) with a pure C17 numerical solver core (`src/econ_*.c`), deterministic `__int128` verification (`cert-verify`), an independent pure-stdlib Python golden oracle (`oracles/`), and a Ring-0 Pure-GPR TAIFEX hardware layer ([`Econ.lean`](Econ.lean)) that **absorbs and upgrades `../SNIPER`**.

---

## 1. Architecture Overview

| Layer | Module | Role & Guarantees |
| :--- | :--- | :--- |
| **Formal Kernel (SSOT)** | [`lean/EconCert.lean`](lean/EconCert.lean) (`import Flow.Interval` only) | Proves 18 theorems (`M1–M18`) with standard axioms only: Cholesky-SDD PSD (`M1–M3`), exact integer LP Weak Duality (`M4`), VCG DSIC & IR (`M5–M6`), Discrete Myerson-Satterthwaite Phase Transition (`M7`), Fisher Walras & clearance (`M8`), Bid-Ask Conversion/Reversal & Butterfly Arbitrage (`M9–M10`), 2-Period FTAP Super-Hedge (`M11`), SOS Nonnegativity (`M12`), Cantelli One-Sided Inequality & Copula-Free CVaR DRO (`M13–M14`), Constructive Covariance Obstruction (`M15`), Eisenberg-Noe Tarski Lattice Bracket, Fire-Sale Dominance & Bailout Sufficiency (`M16–M18`). |
| **Ring-0 Upgrade (`../SNIPER` Absorbed)** | [`Econ.lean`](Econ.lean) (`128` lines, `0` local theorems, `import Flow`) | Replaces `SNIPER`'s naive 8-op mid-price residual with an 18-op Pure-GPR Bid-Ask + Tax/Fee Conversion & Reversal circuit (`econ_bidask_arb.fir`), `320B` `econ_taifex.flow`, `64B` `econ_ring0_abi.h`, zero `br/blr/div/FPU/SIMD`, and direct `sky130` GDSII synthesis via `../ELE/build/ele_bootstrap`. |
| **Zero-FP-Trust Core** | [`include/econ_cert.h`](include/econ_cert.h), [`src/econ_cert.c`](src/econ_cert.c) | FIPS-180-4 SHA-256, Jacobi eigensolver, Two-Phase Simplex LP with dual multipliers, `__int128` Cholesky-SDD certifier, canonical `.econcert.json` writer/verifier, and 9 standalone `CertEcon*.lean` theorem emitters. |
| **4 Pillar Engines** | [`src/econ_fin.c`](src/econ_fin.c), [`src/econ_actuary.c`](src/econ_actuary.c), [`src/econ_clearing.c`](src/econ_clearing.c), [`src/econ_mech.c`](src/econ_mech.c) | Arbitrage + 3-Tier MOT/SOS Pricing (`fin`), Copula-Free CVaR/Cantelli DRO (`actuary`), Tarski Clearing + Fire-Sale Bifurcation + `../FLOW/core/searu.h` Adversarial Shock Search (`clearing`), Combinatorial VCG + MS LP + Proportional-Response Fisher (`mech`). |
| **Python SDK & Oracles** | [`oracles/`](oracles/), [`experiments/`](experiments/) | Pure-stdlib `EconEngine` & persistent `ctypes` `EconFastSession` (`libecon`), independent golden verifier ([`ECON_VERIFICATION_REPORT.md`](ECON_VERIFICATION_REPORT.md)), 14/14 tamper-rejection tests, and 4 scientific discovery experiments ([`experiments/ECON_DISCOVERY_REPORT.md`](experiments/ECON_DISCOVERY_REPORT.md)). |

---

## 2. Quick Verification

```bash
make clean && make verify      # Hygiene + Ring-0 GPR/Silicon + LeanCert (M1-M18) + 9 CertEcon*.lean + Oracle + 14/14 Tamper
make experiments               # Runs exp1..exp4 against econ_cli & EconFastSession -> experiments/*.json
```

### CLI Examples

```bash
./build/econ_cli selftest
./build/econ_cli arb-scan benchmarks/fin/taifex_txo_arb_butterfly.csv --cert build/bfly.econcert.json --emit-lean build/CertBfly.lean --name CertBfly
./build/econ_cli mot-bound benchmarks/fin/spx_2period_skew.csv --payoff fwd_start --param 1.0
./build/econ_cli risk-dro benchmarks/actuary/danish_fire_heavy_tail.json
./build/econ_cli ruin-bound benchmarks/actuary/copula_obstruction_2d.json --target-ruin 0.005
./build/econ_cli clear-net benchmarks/clearing/ecb_core_periphery_12.json --searu
./build/econ_cli mech-verify benchmarks/mech/spectrum_comb_auction.json
./build/econ_cli bilateral-ms --K 6 --second-best
./build/econ_cli fisher-eq benchmarks/mech/fisher_4agent_3good.json
```

---

## 3. Verified Highlights

- **Verification Summary**: See [`ECON_VERIFICATION_REPORT.md`](ECON_VERIFICATION_REPORT.md) (`FIN(5) + ACTUARY(3) + CLEARING(3) + MECH(4) + CERTS(18) + LEAN(9)` + `14/14` tamper rejections + `124,242 B` `sky130` GDSII `DRC=0, LVS=0`).
- **Discovery Highlights**: See [`experiments/ECON_DISCOVERY_REPORT.md`](experiments/ECON_DISCOVERY_REPORT.md):
  1. Legacy `SNIPER` fires 7/7 false positives under wide bid-ask noise (`>= 2.0` pts) while missing `taifex_txo_arb_butterfly.csv` (`+20.15` pts guaranteed profit across all $S \in \mathbb{Z}$).
  2. 2-period MOT LP collapses Fréchet pricing width to **0.123x** on SPX Variance Swaps and exposes constant-IV Black-Scholes violations below the skew no-arbitrage floor (`4.386 < 5.082`).
  3. Gaussian copula CVaR underestimates copula-free DRO tail risk up to **3.04x** at `d=10`, while Normal ruin probabilities fall $1.75 \times 10^6$× short of the exact Cantelli bound on heavy-tail fire losses.
  4. Interbank fire-sale clearing exhibits a sharp discontinuous collapse cliff (`301.84 -> 24.50` paid obligations across `alpha_fire in (2.0, 2.5]` on `ecb12`), and discrete bilateral trade proves an exact phase transition from `K=4 (surplus = 0)` to `K >= 5 (strict impossibility -5, -14, -28, -48)`.
