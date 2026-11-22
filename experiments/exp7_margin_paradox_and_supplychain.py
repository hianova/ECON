#!/usr/bin/env python3
"""Experiment 7 — Inverted-U Micro-Macro Margin Paradox & Supply-Chain Horizon Contagion."""

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

    p_mgn = "benchmarks/margin/exchange_perp_8acct.json"
    p_sup = "benchmarks/margin/supply_chain_tier3_10firm.json"

    thetas = [0.02, 0.04, 0.06, 0.08, 0.10, 0.12, 0.15, 0.18, 0.20, 0.25, 0.30, 0.35, 0.40, 0.45]
    margin_sweep = []
    for sh in [0.00, 0.05, 0.10]:
        for th in thetas:
            res = sess.eval_margin(p_mgn, theta_mm=th, alpha_impact=0.15, shock_pct=sh)
            margin_sweep.append({"shock_pct": sh, "theta_mm": th, **res})

    taus = [0.50, 0.75, 1.00, 1.25, 1.50, 1.75, 2.00, 2.50, 3.00, 3.50]
    supply_sweep = []
    for tau in taus:
        res = sess.eval_supply(p_sup, tau=tau)
        supply_sweep.append({"tau": tau, **res})

    sess.close()

    cert_cases = []
    certs_ok = 0
    certs_tot = 0
    with tempfile.TemporaryDirectory() as tmp:
        for th in [0.04, 0.12, 0.20, 0.30, 0.40]:
            cp = os.path.join(tmp, f"mgn_t{int(round(th * 100))}.econcert.json")
            res = eng.margin_audit(p_mgn, theta=th, cert_path=cp)
            certs_tot += 1
            if eng.cert_verify(cp):
                certs_ok += 1
            cert_cases.append({"kind": "margin", **res})

        for tau in [0.80, 1.50, 2.50]:
            cp = os.path.join(tmp, f"sup_t{int(round(tau * 100))}.econcert.json")
            res = eng.supply_audit(p_sup, tau=tau, searu=True, cert_path=cp)
            certs_tot += 1
            if eng.cert_verify(cp):
                certs_ok += 1
            cert_cases.append({"kind": "supply", **res})

    assert certs_ok == certs_tot
    out = {
        "margin_sweep": margin_sweep,
        "supply_sweep": supply_sweep,
        "cert_cases": cert_cases,
        "certs_verified": certs_ok,
        "certs_total": certs_tot,
    }
    out_path = os.path.join(os.path.dirname(__file__), "exp7_margin_paradox_and_supplychain.json")
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
        f.write("\n")
    print(f"  [EXP7] Margin={len(margin_sweep)} + Supply={len(supply_sweep)}, Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
