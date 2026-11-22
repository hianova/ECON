#!/usr/bin/env python3
"""Deterministic generator for all 28 ECON v2.0.0 benchmarks across 8 pillars (stdlib only)."""

from __future__ import annotations

import json
import math
import os


def norm_cdf(x: float) -> float:
    return 0.5 * math.erfc(-x / math.sqrt(2.0))


def bs_call(S: float, K: float, r: float, iv: float, T: float) -> float:
    vs = iv * math.sqrt(T)
    d1 = (math.log(S / K) + (r + 0.5 * iv * iv) * T) / vs
    d2 = d1 - vs
    return S * norm_cdf(d1) - K * math.exp(-r * T) * norm_cdf(d2)


def bs_put(S: float, K: float, r: float, iv: float, T: float) -> float:
    return bs_call(S, K, r, iv, T) - S + K * math.exp(-r * T)


def write_fin_benchmarks(fin_dir: str) -> None:
    os.makedirs(fin_dir, exist_ok=True)
    spot, r, ttm, tax_div, fee = 22500.0, 0.015, 30.0 / 365.0, 50000.0, 1.5
    fut_mid = spot * math.exp(r * ttm)  # ~22527.76
    fut_bid = round(fut_mid - 2.0, 2)
    fut_ask = round(fut_mid + 2.0, 2)

    strikes = [21900, 22100, 22300, 22500, 22700, 22900, 23100]

    # 1. Clean TAIFEX TXO chain with wide bid-ask noise exceeding 2pt mid-residual (SNIPER FP)
    with open(os.path.join(fin_dir, "taifex_txo_clean.csv"), "w") as f:
        f.write(f"# spot={spot} fut_bid={fut_bid} fut_ask={fut_ask} r={r} ttm={ttm:.6f} tax_div={tax_div} fee={fee}\n")
        f.write("strike,c_bid,c_ask,p_bid,p_ask,fut_bid,fut_ask\n")
        for idx, K in enumerate(strikes):
            iv = 0.18 + 0.015 * abs(K - 22500.0) / 600.0
            cm = bs_call(fut_mid, K, 0.0, iv, ttm)
            pm = bs_put(fut_mid, K, 0.0, iv, ttm)
            # Symmetric spread +/- 3.0 pts with slight bid/ask skew +/- 2.6 pts so mid parity ~ 2.6 >= 2.0
            # while conversion/reversal after bid-ask spread (6+4=10pt) + fee + tax stays strictly <= 0!
            skew = 2.6 if idx % 2 == 0 else -2.6
            cb = max(0.5, round(cm + skew - 3.5, 2))
            ca = round(cm + skew + 3.5, 2)
            pb = max(0.5, round(pm - 3.5, 2))
            pa = round(pm + 3.5, 2)
            f.write(f"{K},{cb},{ca},{pb},{pa},{fut_bid},{fut_ask}\n")

    # 2. Genuine TAIFEX Conversion arbitrage (fut_bid > K + c_ask - p_bid + cost)
    with open(os.path.join(fin_dir, "taifex_txo_arb_conv.csv"), "w") as f:
        f.write(f"# spot={spot} fut_bid={fut_bid} fut_ask={fut_ask} r={r} ttm={ttm:.6f} tax_div={tax_div} fee={fee}\n")
        f.write("strike,c_bid,c_ask,p_bid,p_ask,fut_bid,fut_ask\n")
        for K in strikes:
            iv = 0.18
            cm = bs_call(fut_mid, K, 0.0, iv, ttm)
            pm = bs_put(fut_mid, K, 0.0, iv, ttm)
            cb = max(0.5, round(cm - 1.5, 2))
            ca = round(cm + 1.5, 2)
            pb = max(0.5, round(pm - 1.5, 2))
            pa = round(pm + 1.5, 2)
            if K == 22500:
                ca = round(cm - 8.5, 2)
                cb = max(0.5, ca - 2.0)
                pb = round(pm + 6.5, 2)
                pa = round(pb + 2.0, 2)
            f.write(f"{K},{cb},{ca},{pb},{pa},{fut_bid},{fut_ask}\n")

    # 3. Butterfly Call-Convexity Arbitrage (SNIPER Blind False Negative: mid-parity = 0!)
    with open(os.path.join(fin_dir, "taifex_txo_arb_butterfly.csv"), "w") as f:
        f.write(f"# spot={spot} fut_bid={fut_bid} fut_ask={fut_ask} r={r} ttm={ttm:.6f} tax_div={tax_div} fee={fee}\n")
        f.write("strike,c_bid,c_ask,p_bid,p_ask,fut_bid,fut_ask\n")
        for K in strikes:
            iv = 0.18
            cm = bs_call(fut_mid, K, 0.0, iv, ttm)
            if K == 22500:
                cm += 32.0  # Rich center strike violating convexity while matching P-C parity!
            pm = cm - (fut_mid - K)
            cb = max(0.5, round(cm - 1.0, 2))
            ca = round(cm + 1.0, 2)
            pb = max(0.5, round(pm - 1.0, 2))
            pa = round(pm + 1.0, 2)
            f.write(f"{K},{cb},{ca},{pb},{pa},{fut_bid},{fut_ask}\n")

    # 4. SPX 2-period skewed martingale marginals (guaranteed convex order q1 <_cx q2)
    s0_spx = 100.0
    s1_spx = [84.0, 92.0, 98.0, 102.0, 108.0, 116.0]
    q1_spx = [0.08, 0.17, 0.25, 0.25, 0.17, 0.08]
    # Center mean at 100.0: 0.08*(84+116)+0.17*(92+108)+0.25*(98+102) = 16+34+50 = 100.0 exact!
    s2_spx = [66.0, 78.0, 88.0, 96.0, 104.0, 112.0, 122.0, 134.0]
    # Map each s1[i] to two spread nodes (left skewed wider crash + narrower upside) keeping conditional mean = s1[i]:
    lo_idx = [0, 1, 1, 2, 3, 5]
    hi_idx = [3, 4, 5, 6, 7, 7]
    q2_spx = [0.0] * 8
    for i, s1v in enumerate(s1_spx):
        a, b = s2_spx[lo_idx[i]], s2_spx[hi_idx[i]]
        p_hi = (s1v - a) / (b - a)
        p_lo = 1.0 - p_hi
        q2_spx[lo_idx[i]] += q1_spx[i] * p_lo
        q2_spx[hi_idx[i]] += q1_spx[i] * p_hi
    with open(os.path.join(fin_dir, "spx_2period_skew.csv"), "w") as f:
        f.write(f"# s0={s0_spx} fwd={s0_spx} t1=0.25 t2=0.50 iv=0.22\n")
        f.write("t_idx,s,q_prob\n")
        for s, q in zip(s1_spx, q1_spx):
            f.write(f"1,{s:.4f},{q:.10f}\n")
        for s, q in zip(s2_spx, q2_spx):
            f.write(f"2,{s:.4f},{q:.10f}\n")

    # 5. BTC 2-period heavy-tail martingale marginals (symmetric high-kurtosis diffusion)
    s0_btc = 100.0
    s1_btc = [72.0, 88.0, 97.0, 103.0, 112.0, 128.0]
    q1_btc = [0.10, 0.16, 0.24, 0.24, 0.16, 0.10]
    s2_btc = [45.0, 62.0, 80.0, 94.0, 106.0, 120.0, 138.0, 160.0]
    lo_b = [0, 0, 1, 2, 3, 4]
    hi_b = [3, 5, 5, 6, 7, 7]
    q2_btc = [0.0] * 8
    for i, s1v in enumerate(s1_btc):
        a, b = s2_btc[lo_b[i]], s2_btc[hi_b[i]]
        p_hi = (s1v - a) / (b - a)
        q2_btc[lo_b[i]] += q1_btc[i] * (1.0 - p_hi)
        q2_btc[hi_b[i]] += q1_btc[i] * p_hi
    with open(os.path.join(fin_dir, "btc_2period_fat_tail.csv"), "w") as f:
        f.write(f"# s0={s0_btc} fwd={s0_btc} t1=0.25 t2=0.50 iv=0.58\n")
        f.write("t_idx,s,q_prob\n")
        for s, q in zip(s1_btc, q1_btc):
            f.write(f"1,{s:.4f},{q:.10f}\n")
        for s, q in zip(s2_btc, q2_btc):
            f.write(f"2,{s:.4f},{q:.10f}\n")


def write_actuary_benchmarks(act_dir: str) -> None:
    os.makedirs(act_dir, exist_ok=True)
    n_bins = 48

    def pareto_bins(tail_alpha: float, scale: float) -> list[float]:
        return [round(scale * (math.pow(1.0 - ((b + 0.5) / n_bins), -1.0 / tail_alpha) - 1.0), 6) for b in range(n_bins)]

    # 1. Danish Fire Heavy-Tail Insurance Lines (Building, Contents, Business Interruption)
    corr3 = [[1.0, 0.35, 0.25], [0.35, 1.0, 0.30], [0.25, 0.30, 1.0]]
    samples3 = [pareto_bins(2.3, 1.4), pareto_bins(2.6, 1.1), pareto_bins(2.1, 1.6)]
    with open(os.path.join(act_dir, "danish_fire_heavy_tail.json"), "w") as f:
        json.dump({
            "n_assets": 3, "n_bins": n_bins, "alpha": 0.98, "pareto_tail": 2.3,
            "ruin_threshold": 28.0, "corr": corr3, "samples": samples3
        }, f, indent=2)

    # 2. 5-Sector Credit Loss Portfolio
    corr5 = [[1.0 if i == j else 0.15 for j in range(5)] for i in range(5)]
    samples5 = [pareto_bins(2.8 - 0.1 * i, 1.0 + 0.15 * i) for i in range(5)]
    with open(os.path.join(act_dir, "credit_portfolio_5d.json"), "w") as f:
        json.dump({
            "n_assets": 5, "n_bins": n_bins, "alpha": 0.98, "pareto_tail": 2.6,
            "ruin_threshold": 32.0, "corr": corr5, "samples": samples5
        }, f, indent=2)

    # 3. Low-Correlation 2D Copula Obstruction Stress Case (near-zero Pearson corr, fat tail)
    corr2 = [[1.0, 0.05], [0.05, 1.0]]
    samples2 = [pareto_bins(2.05, 1.8), pareto_bins(2.05, 1.8)]
    with open(os.path.join(act_dir, "copula_obstruction_2d.json"), "w") as f:
        json.dump({
            "n_assets": 2, "n_bins": n_bins, "alpha": 0.98, "pareto_tail": 2.05,
            "ruin_threshold": 24.0, "corr": corr2, "samples": samples2
        }, f, indent=2)


def write_clearing_benchmarks(clr_dir: str) -> None:
    os.makedirs(clr_dir, exist_ok=True)

    # 1. 6-node sparse ring + external sink (fast Lean kernel decidability + clear contagion)
    L6 = [[0.0] * 6 for _ in range(6)]
    for i in range(5):
        L6[i][(i + 1) % 5] = 7.0
        L6[i][5] = 3.0  # external creditor sink (bank 5 has zero obligations)
    ext6 = [1.2, 2.0, 2.5, 3.2, 3.8, 40.0]
    with open(os.path.join(clr_dir, "ring_network_6.json"), "w") as f:
        json.dump({
            "n_banks": 6, "alpha_fire": 1.8, "beta_default": 0.85, "shock_budget": 3.0,
            "ext_assets": ext6, "liabilities": L6
        }, f, indent=2)

    # 2. 12-node Core-Periphery Interbank Network (3 core money-center banks + 8 periphery + 1 sink)
    L12 = [[0.0] * 12 for _ in range(12)]
    for c in range(3):
        L12[c][(c + 1) % 3] = 28.0
        L12[c][(c + 2) % 3] = 20.0
        L12[c][11] = 12.0  # external funding
        for p in range(3 + c * 2, min(11, 3 + c * 2 + 3)):
            L12[c][p] = 5.0
    for p in range(3, 11):
        core = (p - 3) % 3
        L12[p][core] = 10.0
        L12[p][11] = 4.0
    ext12 = [24.0, 22.0, 19.0] + [6.5, 5.5, 5.0, 4.5, 4.0, 5.2, 4.8, 3.8] + [100.0]
    with open(os.path.join(clr_dir, "ecb_core_periphery_12.json"), "w") as f:
        json.dump({
            "n_banks": 12, "alpha_fire": 2.8, "beta_default": 0.72, "shock_budget": 18.0,
            "ext_assets": ext12, "liabilities": L12
        }, f, indent=2)

    # 3. 24-node Multi-Tier Stress Cascade Network (near fire-sale bifurcation tipping point)
    L24 = [[0.0] * 24 for _ in range(24)]
    for c in range(4):
        L24[c][(c + 1) % 4] = 32.0
        L24[c][23] = 18.0
        for p in range(4 + c * 4, 4 + (c + 1) * 4):
            L24[c][p] = 6.0
    for p in range(4, 23):
        c = (p - 4) % 4
        L24[p][c] = 11.0
        L24[p][4 + ((p - 3) % 19)] = 3.0
        L24[p][23] = 5.0
    ext24 = [34.0, 31.0, 29.0, 27.0] + [5.5 + 0.3 * ((p * 7) % 5) for p in range(4, 23)] + [200.0]
    with open(os.path.join(clr_dir, "stress_cascade_24.json"), "w") as f:
        json.dump({
            "n_banks": 24, "alpha_fire": 3.4, "beta_default": 0.68, "shock_budget": 25.0,
            "ext_assets": ext24, "liabilities": L24
        }, f, indent=2)


def write_mech_benchmarks(mech_dir: str) -> None:
    os.makedirs(mech_dir, exist_ok=True)

    # 1. Combinatorial Spectrum Auction (4 bidders, 3 licenses -> 8 masks)
    # Regional bidders want single blocks/pairs with synergies; national bidder bids high on grand bundle 7
    vals_spec = [
        [0, 12, 10, 29, 8, 22, 20, 32],   # Bidder 0: strong synergy on {0,1} (mask 3 = 29)
        [0, 7,  11, 20, 14, 25, 31, 35],  # Bidder 1: strong synergy on {1,2} (mask 6 = 31)
        [0, 13, 9,  23, 15, 34, 24, 38],  # Bidder 2: strong synergy on {0,2} (mask 5 = 34)
        [0, 8,  8,  16, 8,  16, 16, 44],  # Bidder 3: grand-bundle national entrant (mask 7 = 44)
    ]
    with open(os.path.join(mech_dir, "spectrum_comb_auction.json"), "w") as f:
        json.dump({
            "combinatorial": 1, "n_agents": 4, "n_items": 3, "n_outcomes": 8,
            "vals": vals_spec
        }, f, indent=2)

    # 2. Cross-repo synergy: Power Grid Dispatch Allocation (6 discrete OPF/storage dispatch schedules x 4 participants)
    vals_lmp = [
        [85, 92, 104, 98, 76, 88],
        [42, 64, 58,  72, 61, 50],
        [55, 48, 62,  65, 59, 44],
        [30, 35, 34,  41, 28, 39],
    ]
    with open(os.path.join(mech_dir, "ele_lmp_dispatch_auction.json"), "w") as f:
        json.dump({
            "combinatorial": 0, "n_agents": 4, "n_outcomes": 6,
            "vals": vals_lmp
        }, f, indent=2)

    # 3. 4-Agent x 3-Good Linear Fisher Equilibrium Market
    with open(os.path.join(mech_dir, "fisher_4agent_3good.json"), "w") as f:
        json.dump({
            "n_agents": 4, "n_goods": 3,
            "budgets": [120.0, 90.0, 60.0, 80.0],
            "endowments": [15.0, 12.0, 10.0],
            "utils": [
                [4.0, 1.5, 1.0],
                [1.0, 5.0, 2.0],
                [2.0, 1.0, 6.0],
                [3.0, 3.0, 2.5],
            ],
        }, f, indent=2)


def write_agency_benchmarks(ag_dir: str) -> None:
    os.makedirs(ag_dir, exist_ok=True)
    base = {
        "n_states": 4,
        "alpha": 0.20,
        "strike_K": 6.0,
        "clawback_D": 0.0,
        "c_safe": 0.10,
        "c_risk": 0.0,
        "u_reserve": 40.0,
        "returns": [5.0, 10.0, 20.0, -60.0],
        "p_safe": [0.545, 0.420, 0.030, 0.005],
        "p_risk": [0.050, 0.150, 0.600, 0.200],
    }
    raw = dict(base, beta=0.0)
    claw = dict(base, beta=0.35)
    with open(os.path.join(ag_dir, "fund_2and20_raw.json"), "w") as f:
        json.dump(raw, f, indent=2)
    with open(os.path.join(ag_dir, "fund_2and20_clawback.json"), "w") as f:
        json.dump(claw, f, indent=2)


def write_adverse_benchmarks(adv_dir: str) -> None:
    os.makedirs(adv_dir, exist_ok=True)
    with open(os.path.join(adv_dir, "platform_marketplace.json"), "w") as f:
        json.dump({
            "n_types": 10,
            "take_rate": 0.15,
            "weights": [1.0] * 10,
            "values": [20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 110.0],
            "costs": [15.0, 19.0, 23.0, 28.0, 34.0, 41.0, 49.0, 55.0, 61.0, 66.0],
            "has_screening": 1,
            "qH": 0.10, "qL": 0.50,
            "cH": 30.0, "cL": 15.0,
            "PH": 85.0, "PL": 30.0,
            "BH": 120.0, "BL": 0.0,
        }, f, indent=2)

    with open(os.path.join(adv_dir, "insurance_death_spiral.json"), "w") as f:
        json.dump({
            "n_types": 8,
            "take_rate": 0.42,
            "weights": [1.0] * 8,
            "values": [20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0],
            "costs": [15.0, 19.0, 24.0, 30.0, 37.0, 45.0, 54.0, 64.0],
            "has_screening": 0,
        }, f, indent=2)


def write_margin_benchmarks(mgn_dir: str) -> None:
    os.makedirs(mgn_dir, exist_ok=True)
    with open(os.path.join(mgn_dir, "exchange_perp_8acct.json"), "w") as f:
        json.dump({
            "n_accts": 8,
            "P0": 100.0,
            "Pmin": 10.0,
            "alpha_impact": 0.15,
            "theta_mm": 0.30,
            "shock_pct": 0.0,
            "equity_target": 0.10,
            "h": [80.0] * 8,
            "d": [6800.0, 6300.0, 5800.0, 5300.0, 4800.0, 4300.0, 3800.0, 3300.0],
            "c": [0.0] * 8,
        }, f, indent=2)

    N = 10
    L = [[0.0] * N for _ in range(N)]
    # Core 0 owes Tier-1 (1,2,3) 12 each; Tier-1 owe Tier-2 (4,5,6); Tier-2 owe Tier-3 (7,8,9); Tier-3 owe core & cash loop
    for j in (1, 2, 3):
        L[0][j] = 12.0
    L[1][4] = 10.0; L[1][5] = 2.0
    L[2][5] = 10.0; L[2][6] = 2.0
    L[3][6] = 10.0; L[3][4] = 2.0
    L[4][7] = 11.0
    L[5][8] = 11.0
    L[6][9] = 11.0
    for i in (7, 8, 9):
        L[i][0] = 8.0
    with open(os.path.join(mgn_dir, "supply_chain_tier3_10firm.json"), "w") as f:
        json.dump({
            "n_firms": N,
            "core_idx": 0,
            "tau_horizon": 2.0,
            "alpha_fire": 1.5,
            "beta_default": 0.85,
            "shock_budget": 18.0,
            "ext_assets": [16.0, 3.0, 3.0, 3.0, 2.5, 2.5, 2.5, 4.0, 4.0, 4.0],
            "base_payables": L,
        }, f, indent=2)


def write_inst_benchmarks(inst_dir: str) -> None:
    os.makedirs(inst_dir, exist_ok=True)
    specs = {
        "inst_fund_kpi_raw.json": {
            "institution_name": "AlphaCap_2and20_Raw",
            "institution_type": "fund_agency",
            "target_margin_bps": 500.0,
            "c_inc": 1.0, "c_dro": 1.5, "c_sys": 1.2,
            "inc_ref_path": "benchmarks/agency/fund_2and20_raw.json",
            "dro_ref_path": "benchmarks/actuary/danish_fire_heavy_tail.json",
            "sys_ref_path": "benchmarks/clearing/ring_network_6.json",
            "beta_override": 0.0, "dro_reserve_mult": 1.0, "bailout_buffer": 0.0, "chain_fee_bps": 4000.0,
        },
        "inst_fund_kpi_repaired.json": {
            "institution_name": "AlphaCap_2and20_Clawback",
            "institution_type": "fund_agency",
            "target_margin_bps": 500.0,
            "c_inc": 1.0, "c_dro": 1.5, "c_sys": 1.2,
            "inc_ref_path": "benchmarks/agency/fund_2and20_clawback.json",
            "dro_ref_path": "benchmarks/actuary/danish_fire_heavy_tail.json",
            "sys_ref_path": "benchmarks/clearing/ring_network_6.json",
            "beta_override": 0.35, "dro_reserve_mult": 1.5, "bailout_buffer": 6.0, "chain_fee_bps": 7000.0,
        },
        "inst_platform_raw.json": {
            "institution_name": "BazaarX_HighTake_Raw",
            "institution_type": "platform_adverse",
            "target_margin_bps": 500.0,
            "c_inc": 1.2, "c_dro": 1.0, "c_sys": 1.0,
            "inc_ref_path": "benchmarks/adverse/insurance_death_spiral.json",
            "dro_ref_path": "benchmarks/actuary/danish_fire_heavy_tail.json",
            "sys_ref_path": "",
            "take_override": 0.42, "dro_reserve_mult": 1.0, "bailout_buffer": 0.0, "chain_fee_bps": 500.0,
        },
        "inst_platform_repaired.json": {
            "institution_name": "BazaarX_Screened_Repaired",
            "institution_type": "platform_adverse",
            "target_margin_bps": 500.0,
            "c_inc": 1.2, "c_dro": 1.0, "c_sys": 1.0,
            "inc_ref_path": "benchmarks/adverse/platform_marketplace.json",
            "dro_ref_path": "benchmarks/actuary/danish_fire_heavy_tail.json",
            "sys_ref_path": "",
            "take_override": 0.15, "dro_reserve_mult": 3.2, "bailout_buffer": 0.0, "chain_fee_bps": 500.0,
        },
        "inst_exchange_margin_raw.json": {
            "institution_name": "PerpDEX_TightMM_Raw",
            "institution_type": "exchange_margin",
            "target_margin_bps": 500.0,
            "c_inc": 1.0, "c_dro": 1.2, "c_sys": 2.0,
            "inc_ref_path": "",
            "dro_ref_path": "benchmarks/actuary/copula_obstruction_2d.json",
            "sys_ref_path": "benchmarks/margin/exchange_perp_8acct.json",
            "theta_override": 0.30, "dro_reserve_mult": 1.0, "bailout_buffer": 0.0, "chain_fee_bps": 600.0,
        },
        "inst_exchange_margin_repaired.json": {
            "institution_name": "PerpDEX_OptBand_Repaired",
            "institution_type": "exchange_margin",
            "target_margin_bps": 500.0,
            "c_inc": 1.0, "c_dro": 1.2, "c_sys": 2.0,
            "inc_ref_path": "",
            "dro_ref_path": "benchmarks/actuary/copula_obstruction_2d.json",
            "sys_ref_path": "benchmarks/margin/exchange_perp_8acct.json",
            "theta_override": 0.12, "dro_reserve_mult": 3.5, "bailout_buffer": 0.0, "chain_fee_bps": 600.0,
        },
        "inst_supply_chain_raw.json": {
            "institution_name": "MegaOEM_LongTerms_Raw",
            "institution_type": "supply_chain",
            "target_margin_bps": 500.0,
            "c_inc": 1.0, "c_dro": 1.0, "c_sys": 1.8,
            "inc_ref_path": "",
            "dro_ref_path": "benchmarks/actuary/credit_portfolio_5d.json",
            "sys_ref_path": "benchmarks/margin/supply_chain_tier3_10firm.json",
            "tau_override": 2.5, "dro_reserve_mult": 1.0, "bailout_buffer": 0.0, "chain_fee_bps": 500.0,
        },
        "inst_supply_chain_repaired.json": {
            "institution_name": "MegaOEM_ShortHorizon_Repaired",
            "institution_type": "supply_chain",
            "target_margin_bps": 500.0,
            "c_inc": 1.0, "c_dro": 1.0, "c_sys": 1.8,
            "inc_ref_path": "",
            "dro_ref_path": "benchmarks/actuary/credit_portfolio_5d.json",
            "sys_ref_path": "benchmarks/margin/supply_chain_tier3_10firm.json",
            "tau_override": 0.8, "dro_reserve_mult": 3.5, "bailout_buffer": 6.0, "chain_fee_bps": 800.0,
        },
    }
    for fname, obj in specs.items():
        with open(os.path.join(inst_dir, fname), "w") as f:
            json.dump(obj, f, indent=2)


def main() -> None:
    base = "benchmarks"
    write_fin_benchmarks(os.path.join(base, "fin"))
    write_actuary_benchmarks(os.path.join(base, "actuary"))
    write_clearing_benchmarks(os.path.join(base, "clearing"))
    write_mech_benchmarks(os.path.join(base, "mech"))
    write_agency_benchmarks(os.path.join(base, "agency"))
    write_adverse_benchmarks(os.path.join(base, "adverse"))
    write_margin_benchmarks(os.path.join(base, "margin"))
    write_inst_benchmarks(os.path.join(base, "inst"))
    print("  [GEN] Generated 28 deterministic ECON v2.0.0 benchmarks across 8 pillars under benchmarks/")


if __name__ == "__main__":
    main()
