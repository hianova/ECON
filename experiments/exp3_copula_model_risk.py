#!/usr/bin/env python3
"""Experiment 3 — Gaussian Copula Underestimation Multiplier vs Copula-Free DRO & Cantelli Ruin."""

from __future__ import annotations

import json
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "oracles"))
from econ_sdk import EconEngine


def build_risk_json(path: str, n_assets: int, pareto_alpha: float, corr_rho: float, alpha_conf: float = 0.98) -> None:
    n_bins = 48
    corr = [[1.0 if i == j else corr_rho for j in range(n_assets)] for i in range(n_assets)]
    samples = []
    for _ in range(n_assets):
        row = [round(math.pow(1.0 - (b + 0.5) / n_bins, -1.0 / pareto_alpha) - 1.0, 6) for b in range(n_bins)]
        samples.append(row)
    mu_approx = sum(samples[0]) / n_bins * n_assets
    var_approx = sum((v - sum(samples[0]) / n_bins) ** 2 for v in samples[0]) / n_bins * n_assets * (1.0 + (n_assets - 1) * corr_rho)
    ruin_u = mu_approx + 2.8 * math.sqrt(max(1e-6, var_approx))
    with open(path, "w") as f:
        json.dump({
            "n_assets": n_assets, "n_bins": n_bins, "alpha": alpha_conf,
            "pareto_tail": pareto_alpha, "ruin_threshold": ruin_u,
            "corr": corr, "samples": samples
        }, f)


def main() -> None:
    eng = EconEngine("build/econ_cli")
    tail_sweep = []
    dim_sweep = []
    bench_cases = []
    certs_ok = 0
    certs_tot = 0

    with tempfile.TemporaryDirectory() as tmp:
        # 3A. Tail Heaviness Sweep (d=4, rho=0.25, alpha_conf=0.98, pareto_alpha in {1.8..10.0})
        for pa in [1.8, 2.1, 2.5, 3.2, 5.0, 10.0]:
            jp = os.path.join(tmp, f"tail_{pa}.json")
            cd = os.path.join(tmp, f"tail_{pa}_dro.econcert.json")
            cr = os.path.join(tmp, f"tail_{pa}_ruin.econcert.json")
            build_risk_json(jp, n_assets=4, pareto_alpha=pa, corr_rho=0.25)
            d = eng.risk_dro(jp, cert_path=cd)
            r = eng.ruin_bound(jp, target_ruin=0.005, cert_path=cr)
            certs_tot += 2
            if eng.cert_verify(cd):
                certs_ok += 1
            if eng.cert_verify(cr):
                certs_ok += 1
            tail_sweep.append({
                "pareto_tail": pa,
                "gauss_cvar": d["gauss_cvar"],
                "comon_cvar_ub": d["comonotonic_cvar_ub"],
                "mom_dro_cvar_ub": d["moment_dro_cvar_ub"],
                "amp_copula": d["amp_copula_vs_gauss"],
                "amp_moment": d["amp_moment_vs_gauss"],
                "gauss_ruin_prob": r["gauss_ruin_prob"],
                "cantelli_ruin_ub": r["cantelli_ruin_ub"],
                "min_cap_gauss": r["min_capital_gauss"],
                "min_cap_cantelli": r["min_capital_cantelli"],
            })

        # 3B. Portfolio Dimension Sweep (pa=2.3, rho=0.15, d in {2, 3, 5, 8, 10})
        for dim in [2, 3, 5, 8, 10]:
            jp = os.path.join(tmp, f"dim_{dim}.json")
            cd = os.path.join(tmp, f"dim_{dim}_dro.econcert.json")
            build_risk_json(jp, n_assets=dim, pareto_alpha=2.3, corr_rho=0.15)
            d = eng.risk_dro(jp, cert_path=cd)
            certs_tot += 1
            if eng.cert_verify(cd):
                certs_ok += 1
            dim_sweep.append({
                "n_assets": dim,
                "gauss_cvar": d["gauss_cvar"],
                "comon_cvar_ub": d["comonotonic_cvar_ub"],
                "mom_dro_cvar_ub": d["moment_dro_cvar_ub"],
                "amp_copula": d["amp_copula_vs_gauss"],
                "amp_moment": d["amp_moment_vs_gauss"],
            })

        # 3C. Benchmark Tables
        for name in ["danish_fire_heavy_tail.json", "credit_portfolio_5d.json", "copula_obstruction_2d.json"]:
            p = os.path.join("benchmarks", "actuary", name)
            cd = os.path.join(tmp, f"{name}.dro.econcert.json")
            cr = os.path.join(tmp, f"{name}.ruin.econcert.json")
            d = eng.risk_dro(p, cert_path=cd)
            r = eng.ruin_bound(p, target_ruin=0.005, cert_path=cr)
            certs_tot += 2
            if eng.cert_verify(cd):
                certs_ok += 1
            if eng.cert_verify(cr):
                certs_ok += 1
            bench_cases.append({"name": name, "dro": d, "ruin": r})

    out = {
        "tail_sweep": tail_sweep,
        "dim_sweep": dim_sweep,
        "bench_cases": bench_cases,
        "certs_verified": f"{certs_ok}/{certs_tot}",
    }
    out_path = os.path.join(os.path.dirname(__file__), "exp3_copula_model_risk.json")
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
    print(f"  [EXP3] Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
