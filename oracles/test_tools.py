#!/usr/bin/env python3
"""End-to-End CLI, EconFastSession ctypes SDK & 12/12 Tamper-Rejection Test Suite."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile

from econ_sdk import EconEngine, EconFastSession


def expect_verify_fail(cli_path: str, cert_path: str, tag: str) -> None:
    res = subprocess.run([cli_path, "cert-verify", cert_path], capture_output=True, text=True)
    assert res.returncode != 0, f"Tamper test FAILED to reject corrupted certificate ({tag}): {cert_path}"


def main() -> None:
    eng = EconEngine("build/econ_cli")
    assert eng.selftest() == 0, "selftest failed"

    # CLI & SDK smoke tests
    a1 = eng.arb_scan("benchmarks/fin/taifex_txo_clean.csv")
    assert a1["arb_found"] == 0 and a1["sniper_false_pos"] >= 3
    a2 = eng.arb_scan("benchmarks/fin/taifex_txo_arb_butterfly.csv")
    assert a2["arb_found"] == 1 and a2["best_mode"] == 3 and a2["sniper_false_neg"] >= 1

    m1 = eng.mot_bound("benchmarks/fin/spx_2period_skew.csv", payoff="fwd_start")
    assert m1["certified"] == 1 and m1["mot_lb"] <= m1["mot_ub"]
    m2 = eng.mot_bound("benchmarks/fin/btc_2period_fat_tail.csv", payoff="var_swap")
    assert m2["certified"] == 1 and m2["mot_lb"] <= m2["mot_ub"]

    d1 = eng.risk_dro("benchmarks/actuary/danish_fire_heavy_tail.json")
    assert d1["corr_psd_ok"] == 1 and d1["amp_moment_vs_gauss"] > 1.2
    r1 = eng.ruin_bound("benchmarks/actuary/danish_fire_heavy_tail.json")
    assert r1["cantelli_ruin_ub"] >= r1["gauss_ruin_prob"]

    c1 = eng.clear_net("benchmarks/clearing/ecb_core_periphery_12.json", searu=True)
    assert c1["multi_eq_gap"] >= 0.0 and c1["adv_shock_shortfall"] >= c1["systemic_shortfall"] - 1e-6

    v1 = eng.mech_verify("benchmarks/mech/spectrum_comb_auction.json")
    assert v1["dsic_verified"] == 1 and v1["ir_verified"] == 1
    ms5 = eng.bilateral_ms(5, second_best=True)
    assert ms5["impossibility_holds"] == 1 and abs(ms5["max_budget_surplus"] + 5.0) < 1e-4
    f1 = eng.fisher_eq("benchmarks/mech/fisher_4agent_3good.json")
    assert f1["certified"] == 1

    # EconFastSession ctypes persistent FFI test
    sess = EconFastSession("benchmarks/clearing/ring_network_6.json")
    s_lo = sess.sweep_clearing(alpha_fire=0.2, beta_def=0.95, shock_scale=1.0)
    s_hi = sess.sweep_clearing(alpha_fire=4.0, beta_def=0.60, shock_scale=0.8)
    assert s_hi["systemic_shortfall"] >= s_lo["systemic_shortfall"]
    ms_fast = sess.eval_ms(6, second_best=True)
    assert abs(ms_fast["max_budget_surplus"] + 14.0) < 1e-4
    sess.close()

    # 12/12 Zero-FP-Trust Tamper-Rejection Suite
    cert_dir = "build/certs"
    with tempfile.TemporaryDirectory() as tmp:
        def load(name: str) -> dict:
            with open(os.path.join(cert_dir, name), "r") as f:
                return json.load(f)

        def save_and_fail(obj: dict, fname: str, tag: str) -> None:
            p = os.path.join(tmp, fname)
            with open(p, "w") as f:
                json.dump(obj, f)
            expect_verify_fail("build/econ_cli", p, tag)

        # 1. Corrupt input SHA-256
        r = load("fin_txo_clean.econcert.json")
        r["input_sha256"] = "deadbeef" + r["input_sha256"][8:]
        save_and_fail(r, "t01_sha.json", "T01_SHA256")

        # 2. Flip arb_edge_int in clean NO_ARB cert
        r = load("fin_txo_clean.econcert.json")
        r["arb_mode"] = 1
        r["arb_edge_int"] = 500
        save_and_fail(r, "t02_noarb.json", "T02_NO_ARB_FORGE")

        # 3. Forge conversion edge in fin_txo_conv.econcert.json
        r = load("fin_txo_conv.econcert.json")
        r["arb_edge_int"] += 123
        save_and_fail(r, "t03_conv.json", "T03_CONV_EDGE")

        # 4. Break butterfly convexity k1 < k2 < k3 / credit
        r = load("fin_txo_butterfly.econcert.json")
        r["c_ask1"] += 500000
        save_and_fail(r, "t04_bfly.json", "T04_BUTTERFLY")

        # 5. Break MOT dual super-hedge pointwise dominance in fin_spx_mot.econcert.json
        r = load("fin_spx_mot.econcert.json")
        r["phi2_up_int"][-1] -= 50000
        save_and_fail(r, "t05_mot.json", "T05_MOT_SUPERHEDGE")

        # 6. Corrupt SOS Hankel A_int entry in fin_btc_mot.econcert.json
        r = load("fin_btc_mot.econcert.json")
        r["A_int"][0][0] = -1000
        save_and_fail(r, "t06_psd_a.json", "T06_SOS_PSD")

        # 7. Inflate mu_psd & mu_cert consistently in act_danish_dro.econcert.json -> Cholesky+SDD rejects
        r = load("act_danish_dro.econcert.json")
        r["mu_psd"] += 20000
        r["mu_cert"] += 20000
        save_and_fail(r, "t07_dro_mu.json", "T07_CORR_MU_SDD")

        # 8. Forge DRO upper bound below Gaussian baseline in act_credit_dro.econcert.json
        r = load("act_credit_dro.econcert.json")
        r["dro_cvar_int"] = r["gauss_cvar_int"] - 100
        save_and_fail(r, "t08_dro_bound.json", "T08_DRO_INVERT")

        # 9. Corrupt Cantelli S2 witness in act_copula_ruin.econcert.json (ptail >= 1)
        r = load("act_copula_ruin.econcert.json")
        r["cantelli_S2"] = 1
        save_and_fail(r, "t09_cantelli.json", "T09_CANTELLI_INEQ")

        # 10. Raise clear_p_lo_int above Tarski pre-fixed-point in clr_ring6.econcert.json
        r = load("clr_ring6.econcert.json")
        r["clear_p_lo_int"][0] = r["clear_p_bar_int"][0]
        save_and_fail(r, "t10_tarski.json", "T10_TARSKI_BRACKET")

        # 11. Slash bailout z[0] in clr_ecb12.econcert.json -> M18 bailout sufficiency rejects
        r = load("clr_ecb12.econcert.json")
        for idx in range(len(r["clear_z_int"])):
            r["clear_z_int"][idx] = 0
        save_and_fail(r, "t11_bailout.json", "T11_BAILOUT_M18")

        # 12. Inflate MS deficit into positive territory OR break VCG pivot in mech_spectrum_vcg.econcert.json
        r = load("mech_ms_k6.econcert.json")
        r["mech_ms_surplus_int"] = 1000
        save_and_fail(r, "t12_ms.json", "T12_MS_OBSTRUCTION")

        r13 = load("mech_spectrum_vcg.econcert.json")
        r13["mech_pivots_int"][0] += 10000000
        save_and_fail(r13, "t13_vcg.json", "T13_VCG_PIVOT")

        r14 = load("mech_fisher4.econcert.json")
        r14["mech_fisher_p_int"][0] = 0
        save_and_fail(r14, "t14_fisher.json", "T14_FISHER_POS_PRICE")

    print("  [TEST_TOOLS PASS] CLI + EconEngine + EconFastSession + 14/14 Tamper-Rejection checks PASS")


if __name__ == "__main__":
    main()
