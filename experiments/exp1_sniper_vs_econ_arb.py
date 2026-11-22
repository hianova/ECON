#!/usr/bin/env python3
"""Experiment 1 — Legacy SNIPER Mid-Price Parity vs ECON Bid-Ask+Tax+Convexity Arbitrage Audit."""

from __future__ import annotations

import json
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "oracles"))
from econ_sdk import EconEngine


def norm_cdf(x: float) -> float:
    return 0.5 * math.erfc(-x / math.sqrt(2.0))


def bs_call(S: float, K: float, iv: float, T: float) -> float:
    vs = iv * math.sqrt(T)
    d1 = (math.log(S / K) + 0.5 * iv * iv * T) / vs
    return S * norm_cdf(d1) - K * norm_cdf(d1 - vs)


def write_synthetic_chain(path: str, half_spread: float, mid_skew: float, conv_bump: float = 0.0, bfly_bump: float = 0.0) -> None:
    fut_mid = 22527.76
    fut_bid = round(fut_mid - 2.0, 2)
    fut_ask = round(fut_mid + 2.0, 2)
    strikes = [21900, 22100, 22300, 22500, 22700, 22900, 23100]
    with open(path, "w") as f:
        f.write(f"# spot=22500.0 fut_bid={fut_bid} fut_ask={fut_ask} r=0.015 ttm=0.082192 tax_div=50000 fee=1.5\n")
        f.write("strike,c_bid,c_ask,p_bid,p_ask,fut_bid,fut_ask\n")
        for idx, K in enumerate(strikes):
            cm = bs_call(fut_mid, K, 0.18, 30.0 / 365.0)
            if K == 22500:
                cm += bfly_bump
            pm = cm - (fut_mid - K)
            sk = mid_skew if (idx % 2 == 0) else -mid_skew
            cb = max(0.5, round(cm + sk - half_spread, 2))
            ca = round(cm + sk + half_spread, 2)
            pb = max(0.5, round(pm - half_spread, 2))
            pa = round(pm + half_spread, 2)
            if K == 22500 and conv_bump > 0.0:
                ca = round(cm - conv_bump, 2)
                cb = max(0.5, ca - 2.0)
                pb = round(pm + conv_bump, 2)
                pa = round(pb + 2.0, 2)
            f.write(f"{K},{cb},{ca},{pb},{pa},{fut_bid},{fut_ask}\n")


def main() -> None:
    eng = EconEngine("build/econ_cli")
    results: dict = {"spread_sweep": [], "benchmark_scenarios": []}
    certs_ok = 0
    certs_tot = 0

    with tempfile.TemporaryDirectory() as tmp:
        # 1A. Bid-Ask Spread & Mid-Price Noise False-Positive Sweep (clean no-arb markets)
        for hw in [1.0, 2.0, 3.0, 4.0, 5.0]:
            csv_p = os.path.join(tmp, f"clean_hw{hw:.1f}.csv")
            cert_p = os.path.join(tmp, f"clean_hw{hw:.1f}.econcert.json")
            mid_skew = max(2.4, hw + 0.2) if hw >= 2.5 else 2.4
            # Keep total round-trip > skew so net conversion/reversal edge stays <= 0 when hw >= 2.0
            write_synthetic_chain(csv_p, half_spread=hw, mid_skew=2.4 if hw >= 2.0 else 0.4)
            r = eng.arb_scan(csv_p, cert_path=cert_p)
            certs_tot += 1
            if eng.cert_verify(cert_p):
                certs_ok += 1
            results["spread_sweep"].append({
                "half_spread": hw,
                "sniper_flags": r["sniper_flags"],
                "sniper_false_pos": r["sniper_false_pos"],
                "econ_arb_found": r["arb_found"],
                "econ_false_pos": 0 if r["arb_found"] == 0 else r["sniper_flags"],
            })

        # 1B. Benchmark & Embedded Arbitrage Contrast (Clean vs Conversion vs Butterfly)
        cases = [
            ("taifex_txo_clean.csv", "benchmarks/fin/taifex_txo_clean.csv"),
            ("taifex_txo_arb_conv.csv", "benchmarks/fin/taifex_txo_arb_conv.csv"),
            ("taifex_txo_arb_butterfly.csv", "benchmarks/fin/taifex_txo_arb_butterfly.csv"),
        ]
        # Add synthetic Reversal case
        rev_csv = os.path.join(tmp, "syn_reversal.csv")
        write_synthetic_chain(rev_csv, half_spread=1.0, mid_skew=0.0)
        # Reversal: inject overpriced call bid / cheap put ask at K=22500
        with open(rev_csv, "r") as rf:
            lines = rf.readlines()
        with open(rev_csv, "w") as wf:
            for ln in lines:
                if ln.startswith("22500,"):
                    parts = ln.strip().split(",")
                    parts[1] = f"{float(parts[1]) + 10.0:.2f}"
                    parts[2] = f"{float(parts[2]) + 10.0:.2f}"
                    parts[4] = f"{max(0.5, float(parts[4]) - 8.0):.2f}"
                    parts[3] = f"{max(0.5, float(parts[4]) - 2.0):.2f}"
                    wf.write(",".join(parts) + "\n")
                else:
                    wf.write(ln)
        cases.append(("synthetic_reversal.csv", rev_csv))

        for label, path in cases:
            cert_p = os.path.join(tmp, f"{label}.econcert.json")
            r = eng.arb_scan(path, cert_path=cert_p)
            certs_tot += 1
            if eng.cert_verify(cert_p):
                certs_ok += 1
            results["benchmark_scenarios"].append({
                "name": label,
                "n_strikes": r["n_strikes"],
                "arb_found": r["arb_found"],
                "best_mode": r["best_mode"],
                "best_edge": r["best_edge"],
                "sniper_flags": r["sniper_flags"],
                "sniper_false_pos": r["sniper_false_pos"],
                "sniper_false_neg": r["sniper_false_neg"],
                "butterfly_count": r["econ_butterfly_count"],
            })

    results["certs_verified"] = f"{certs_ok}/{certs_tot}"
    out_path = os.path.join(os.path.dirname(__file__), "exp1_sniper_vs_econ_arb.json")
    with open(out_path, "w") as f:
        json.dump(results, f, indent=2)
    print(f"  [EXP1] Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
