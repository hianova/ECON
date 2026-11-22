#!/usr/bin/env python3
"""Experiment 4 — Fire-Sale Contagion Bifurcation (EconFastSession) & Exact Discrete Myerson-Satterthwaite."""

from __future__ import annotations

import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "oracles"))
from econ_sdk import EconEngine, EconFastSession


def main() -> None:
    eng = EconEngine("build/econ_cli")
    phase_ecb = []
    phase_stress = []
    ms_sweep = []
    searu_cases = []
    certs_ok = 0
    certs_tot = 0

    # 4A. Fast in-process fire-sale sweep via EconFastSession
    s_ecb = EconFastSession("benchmarks/clearing/ecb_core_periphery_12.json")
    s_str = EconFastSession("benchmarks/clearing/stress_cascade_24.json")
    for s_scale in [1.0, 0.85, 0.70]:
        for af in [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0]:
            r_e = s_ecb.sweep_clearing(alpha_fire=af, beta_def=0.72, shock_scale=s_scale)
            r_s = s_str.sweep_clearing(alpha_fire=af, beta_def=0.68, shock_scale=s_scale)
            phase_ecb.append({"shock_scale": s_scale, "alpha_fire": af, **r_e})
            phase_stress.append({"shock_scale": s_scale, "alpha_fire": af, **r_s})
    s_ecb.close()
    s_str.close()

    # 4B. SEARU Adversarial Shock Hunting + Certified Tarski & Bailout across all 3 clearing benchmarks
    with tempfile.TemporaryDirectory() as tmp:
        for fname in ["ring_network_6.json", "ecb_core_periphery_12.json", "stress_cascade_24.json"]:
            p = os.path.join("benchmarks", "clearing", fname)
            cp = os.path.join(tmp, f"{fname}.econcert.json")
            r = eng.clear_net(p, searu=True, cert_path=cp)
            certs_tot += 1
            if eng.cert_verify(cp):
                certs_ok += 1
            searu_cases.append({"name": fname, **r})

        # 4C. Exact Discrete Myerson-Satterthwaite Phase Transition K=2..8
        s_ms = EconFastSession()
        for K in [2, 3, 4, 5, 6, 7, 8]:
            fast_r = s_ms.eval_ms(K, second_best=True)
            cp = os.path.join(tmp, f"ms_k{K}.econcert.json")
            cli_r = eng.bilateral_ms(K, second_best=True, cert_path=cp)
            certs_tot += 1
            if eng.cert_verify(cp, check_hash=False):
                certs_ok += 1
            ms_sweep.append({
                "K": K,
                "efficient_welfare": cli_r["efficient_welfare"],
                "min_rent_required": cli_r["min_rent_required"],
                "max_budget_surplus": cli_r["max_budget_surplus"],
                "norm_surplus_ratio": cli_r["norm_surplus_ratio"],
                "theoretical_formula": cli_r["theoretical_formula"],
                "second_best_welfare": cli_r["second_best_welfare"],
                "second_best_eff_pct": cli_r["second_best_eff_pct"],
                "impossibility_holds": cli_r["impossibility_holds"],
                "fast_agree": abs(fast_r["max_budget_surplus"] - cli_r["max_budget_surplus"]) < 1e-6,
            })
        s_ms.close()

    out = {
        "phase_ecb": phase_ecb,
        "phase_stress": phase_stress,
        "searu_cases": searu_cases,
        "ms_sweep": ms_sweep,
        "certs_verified": f"{certs_ok}/{certs_tot}",
    }
    out_path = os.path.join(os.path.dirname(__file__), "exp4_firesale_tipping_and_ms.json")
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
    print(f"  [EXP4] Sweeps={len(phase_ecb) + len(phase_stress)} clearing + {len(ms_sweep)} MS, Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
