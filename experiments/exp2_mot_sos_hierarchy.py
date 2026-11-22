#!/usr/bin/env python3
"""Experiment 2 — 3-Tier Model-Free Pricing Hierarchy (Frechet vs 2-Period MOT LP vs Moment-SOS vs BS)."""

from __future__ import annotations

import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "oracles"))
from econ_sdk import EconEngine


def main() -> None:
    eng = EconEngine("build/econ_cli")
    markets = [
        ("SPX Skew", "benchmarks/fin/spx_2period_skew.csv"),
        ("BTC Fat-Tail", "benchmarks/fin/btc_2period_fat_tail.csv"),
    ]
    payoffs = [
        ("fwd_start", 0.90),
        ("fwd_start", 0.95),
        ("fwd_start", 1.00),
        ("fwd_start", 1.05),
        ("fwd_start", 1.10),
        ("var_swap", 1.00),
        ("lookback", 1.00),
    ]
    rows = []
    certs_ok = 0
    certs_tot = 0

    with tempfile.TemporaryDirectory() as tmp:
        for mname, mpath in markets:
            for pkind, param in payoffs:
                cp = os.path.join(tmp, f"{mname.replace(' ', '_')}_{pkind}_{param:.2f}.econcert.json")
                r = eng.mot_bound(mpath, payoff=pkind, param=param, cert_path=cp)
                certs_tot += 1
                ok = eng.cert_verify(cp)
                if ok:
                    certs_ok += 1
                rows.append({
                    "market": mname,
                    "payoff": pkind,
                    "param": param,
                    "frechet_lb": r["frechet_lb"],
                    "mot_lb": r["mot_lb"],
                    "sos_lb": r["sos_lb"],
                    "bs_price": r["bs_price"],
                    "sos_ub": r["sos_ub"],
                    "mot_ub": r["mot_ub"],
                    "frechet_ub": r["frechet_ub"],
                    "frechet_width": r["frechet_ub"] - r["frechet_lb"],
                    "mot_width": r["mot_ub"] - r["mot_lb"],
                    "tightening_ratio": r["tightening_ratio"],
                    "cert_ok": ok,
                })

    out = {"rows": rows, "certs_verified": f"{certs_ok}/{certs_tot}"}
    out_path = os.path.join(os.path.dirname(__file__), "exp2_mot_sos_hierarchy.json")
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
    print(f"  [EXP2] {len(rows)} solves, Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
