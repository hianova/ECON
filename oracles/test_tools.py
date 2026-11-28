#!/usr/bin/env python3
"""End-to-End CLI, EconFastSession ctypes SDK & 22/22 Tamper-Rejection Test Suite."""

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

    # v2.0.0 Pillars 5-8 CLI & SDK checks
    ag0 = eng.agency_audit("benchmarks/agency/fund_2and20_raw.json")
    assert ag0["ic_satisfied"] == 0 and ag0["cantelli_amp_ratio"] > 5.0
    ag1 = eng.agency_audit("benchmarks/agency/fund_2and20_clawback.json")
    assert ag1["ic_satisfied"] == 1 and ag1["ic_margin"] > 0.0

    av0 = eng.adverse_audit("benchmarks/adverse/platform_marketplace.json")
    assert av0["k_eq"] >= 4 and av0["screening_valid"] == 1 and av0["screened_r_star"] > av0["critical_r_star"]
    av1 = eng.adverse_audit("benchmarks/adverse/insurance_death_spiral.json")
    assert av1["k_eq"] == 0 and av1["full_collapse"] == 1

    mg0 = eng.margin_audit("benchmarks/margin/exchange_perp_8acct.json", theta=0.12)
    mg1 = eng.margin_audit("benchmarks/margin/exchange_perp_8acct.json", theta=0.30)
    assert mg0["bad_debt"] == 0.0 and mg1["bad_debt"] > 10000.0 and mg1["paradox_detected"] == 1

    sup0 = eng.supply_audit("benchmarks/margin/supply_chain_tier3_10firm.json", tau=0.8)
    sup1 = eng.supply_audit("benchmarks/margin/supply_chain_tier3_10firm.json", tau=2.5)
    assert sup1["shock_horizon_margin"] < sup0["shock_horizon_margin"] and sup1["searu_shortfall"] > sup0["searu_shortfall"]

    in0 = eng.audit_inst("benchmarks/inst/inst_fund_kpi_raw.json")
    in1 = eng.audit_inst("benchmarks/inst/inst_fund_kpi_repaired.json")
    assert in0["certified"] == 0 and in1["certified"] == 1

    # EconFastSession ctypes persistent FFI test
    sess = EconFastSession("benchmarks/clearing/ring_network_6.json")
    s_lo = sess.sweep_clearing(alpha_fire=0.2, beta_def=0.95, shock_scale=1.0)
    s_hi = sess.sweep_clearing(alpha_fire=4.0, beta_def=0.60, shock_scale=0.8)
    assert s_hi["systemic_shortfall"] >= s_lo["systemic_shortfall"]
    ms_fast = sess.eval_ms(6, second_best=True)
    assert abs(ms_fast["max_budget_surplus"] + 14.0) < 1e-4
    f_ag = sess.eval_agency("benchmarks/agency/fund_2and20_raw.json", alpha=0.20, beta=0.35)
    assert f_ag["ic_margin"] > 0.0
    f_av = sess.eval_adverse("benchmarks/adverse/platform_marketplace.json", take_rate=0.15)
    assert f_av["k_eq"] >= 4
    f_mg = sess.eval_margin("benchmarks/margin/exchange_perp_8acct.json", theta_mm=0.12, alpha_impact=0.15, shock_pct=0.0)
    assert f_mg["bad_debt"] == 0.0
    f_sp = sess.eval_supply("benchmarks/margin/supply_chain_tier3_10firm.json", tau=1.0)
    assert f_sp["searu_shortfall"] >= 0.0 and f_sp["shock_horizon_margin"] <= 1e6
    sess.close()

    # 22/22 Zero-FP-Trust Tamper-Rejection Suite
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

        # 15-22. Pillars 5-8 Zero-FP-Trust Tamper-Rejection checks
        r15 = load("ag_clawback.econcert.json")
        r15["ag_ic_margin"] += 500
        save_and_fail(r15, "t15_ag_ic.json", "T15_AGENCY_IC")

        r16 = load("ag_clawback.econcert.json")
        r16["ag_S2_risk"] = 1
        save_and_fail(r16, "t16_ag_cant.json", "T16_AGENCY_CANTELLI")

        r17 = load("adv_platform.econcert.json")
        r17["adv_r_star"] = r17["adv_scale"]
        save_and_fail(r17, "t17_adv_rstar.json", "T17_ADVERSE_RSTAR")

        r18 = load("adv_platform.econcert.json")
        r18["adv_BH"] = -1000
        save_and_fail(r18, "t18_adv_scr.json", "T18_SCREEN_IC")

        r19 = load("mgn_perp8.econcert.json")
        r19["mgn_trig_int"][0] = 0
        save_and_fail(r19, "t19_mgn_trig.json", "T19_MARGIN_TRIG")

        r20 = load("mgn_perp8.econcert.json")
        r20["mgn_bad_debt"] = 0
        save_and_fail(r20, "t20_mgn_bd.json", "T20_MARGIN_BADDEBT")

        r21 = load("inst_fund_raw.econcert.json")
        r21["inst_master"] = 999
        save_and_fail(r21, "t21_inst_mast.json", "T21_INST_MASTER")

        r22 = load("inst_fund_raw.econcert.json")
        r22["inst_repair_cost"] += 42
        save_and_fail(r22, "t22_inst_cost.json", "T22_INST_REPAIR_COST")

        # ECN2 Packed Wire CLI & SDK roundtrip + tamper/truncation/non-canonical rejection
        ecn2_bin = os.path.join(tmp, "inst_fund_raw.ecn2")
        out_msg = eng.pack_wire("benchmarks/inst/inst_fund_kpi_raw.json", ecn2_bin)
        assert "OK" in out_msg
        dec = eng.unpack_wire(ecn2_bin)
        r_ref = load("inst_fund_raw.econcert.json")
        assert dec["format"] == "ecn2.v1"
        assert dec["status"] == r_ref["status"]
        assert dec["m_inc"] == r_ref["inst_m_inc"]
        assert dec["m_dro"] == r_ref["inst_m_dro"]
        assert dec["m_sys"] == r_ref["inst_m_sys"]
        assert dec["m_star"] == r_ref["inst_master"]
        assert dec["repair_cost"] == r_ref["inst_repair_cost"]
        assert dec["sha256"] == r_ref["input_sha256"]

        raw_wire = open(ecn2_bin, "rb").read()
        for bad_tag, bad_bytes in [
            ("ECN2_TRUNC", raw_wire[:-1]),
            ("ECN2_TRAILING", raw_wire + b"\x00"),
            ("ECN2_TAMPER_BODY", raw_wire[:10] + bytes([raw_wire[10] ^ 0x01]) + raw_wire[11:]),
        ]:
            bp = os.path.join(tmp, f"{bad_tag}.ecn2")
            open(bp, "wb").write(bad_bytes)
            r_bad = subprocess.run(["build/econ_cli", "unpack-wire", bp], capture_output=True, text=True)
            assert r_bad.returncode != 0, f"unpack-wire failed to reject {bad_tag}"

    print("  [TEST_TOOLS PASS] CLI + EconEngine + EconFastSession + 22/22 Tamper-Rejection + ECN2 Wire checks PASS")


if __name__ == "__main__":
    main()
