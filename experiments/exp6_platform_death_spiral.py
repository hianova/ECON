#!/usr/bin/env python3
"""Experiment 6 — Platform Take-Rate Death Spiral Phase Transition vs Screening Expansion."""

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

    rates = [0.00, 0.05, 0.10, 0.15, 0.20, 0.22, 0.25, 0.26, 0.28, 0.30, 0.35, 0.40, 0.45, 0.50]
    plat_sweep = []
    spiral_sweep = []

    p_plat = "benchmarks/adverse/platform_marketplace.json"
    p_spir = "benchmarks/adverse/insurance_death_spiral.json"

    for r in rates:
        mp = sess.eval_adverse(p_plat, take_rate=r)
        ms = sess.eval_adverse(p_spir, take_rate=r)
        plat_sweep.append({"take_rate": r, **mp})
        spiral_sweep.append({"take_rate": r, **ms})

    sess.close()

    cert_cases = []
    certs_ok = 0
    certs_tot = 0
    with tempfile.TemporaryDirectory() as tmp:
        for r in [0.10, 0.15, 0.20, 0.25, 0.26, 0.35]:
            cp = os.path.join(tmp, f"adv_r{int(round(r * 100))}.econcert.json")
            res = eng.adverse_audit(p_plat, take_rate=r, cert_path=cp)
            certs_tot += 1
            if eng.cert_verify(cp):
                certs_ok += 1
            cert_cases.append({"scenario": "platform_marketplace", **res})

        cp_sp = os.path.join(tmp, "adv_spiral.econcert.json")
        res_sp = eng.adverse_audit(p_spir, cert_path=cp_sp)
        certs_tot += 1
        if eng.cert_verify(cp_sp):
            certs_ok += 1
        cert_cases.append({"scenario": "insurance_death_spiral", **res_sp})

    assert certs_ok == certs_tot
    out = {
        "platform_sweep": plat_sweep,
        "spiral_sweep": spiral_sweep,
        "cert_cases": cert_cases,
        "certs_verified": certs_ok,
        "certs_total": certs_tot,
    }
    out_path = os.path.join(os.path.dirname(__file__), "exp6_platform_death_spiral.json")
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
        f.write("\n")
    print(f"  [EXP6] Sweeps={len(plat_sweep) * 2}, Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
