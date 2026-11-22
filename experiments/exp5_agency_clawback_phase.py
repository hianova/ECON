#!/usr/bin/env python3
"""Experiment 5 — Agency 2D Phase Boundary (alpha x beta) & Cantelli Tail Risk Amplification."""

from __future__ import annotations

import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "oracles"))
from econ_sdk import EconEngine, EconFastSession


def main() -> None:
    eng = EconEngine("build/econ_cli")
    sess = EconFastSession()
    src = "benchmarks/agency/fund_2and20_raw.json"

    alphas = [0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.40]
    betas = [0.00, 0.05, 0.10, 0.15, 0.20, 0.25, 0.30, 0.40, 0.50]
    phase_grid = []
    alpha_thresholds = []

    for al in alphas:
        b_star = None
        for bt in betas:
            m = sess.eval_agency(src, alpha=al, beta=bt)
            if b_star is None:
                b_star = m["beta_star"]
            phase_grid.append({
                "alpha": al,
                "beta": bt,
                "ic_margin": m["ic_margin"],
                "ic_satisfied": 1 if m["ic_margin"] > 0.0 else 0,
                "beta_star": m["beta_star"],
                "sys_ev_gap": m["sys_ev_gap"],
                "cantelli_safe": m["cantelli_safe"],
                "cantelli_risk": m["cantelli_risk"],
                "amp_ratio": m["amp_ratio"],
            })
        alpha_thresholds.append({"alpha": al, "beta_star": b_star})

    sess.close()

    cert_cases = []
    certs_ok = 0
    certs_tot = 0
    with tempfile.TemporaryDirectory() as tmp:
        for bt in [0.00, 0.10, 0.15, 0.25, 0.35, 0.50]:
            cp = os.path.join(tmp, f"ag_b{int(round(bt * 100))}.econcert.json")
            res = eng.agency_audit(src, beta=bt, cert_path=cp)
            certs_tot += 1
            if eng.cert_verify(cp):
                certs_ok += 1
            cert_cases.append({"beta": bt, **res})

        for fname in ["fund_2and20_raw.json", "fund_2and20_clawback.json"]:
            p = os.path.join("benchmarks", "agency", fname)
            cp = os.path.join(tmp, f"{fname}.econcert.json")
            res = eng.agency_audit(p, cert_path=cp)
            certs_tot += 1
            if eng.cert_verify(cp):
                certs_ok += 1
            cert_cases.append({"benchmark": fname, **res})

    assert certs_ok == certs_tot
    out = {
        "alpha_thresholds": alpha_thresholds,
        "phase_grid": phase_grid,
        "cert_cases": cert_cases,
        "certs_verified": certs_ok,
        "certs_total": certs_tot,
    }
    out_path = os.path.join(os.path.dirname(__file__), "exp5_agency_clawback_phase.json")
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
        f.write("\n")
    print(f"  [EXP5] Grid={len(phase_grid)} solves, Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
