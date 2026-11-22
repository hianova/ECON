#!/usr/bin/env python3
"""Experiment 9 — Offline Historical Real-Market Empirical Calibration & Backtest Suite."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "oracles"))
from econ_sdk import EconEngine, EconFastSession


def _find_tool(name: str) -> str:
    which_p = shutil.which(name)
    if which_p:
        return which_p
    home_p = Path.home() / ".elan" / "bin" / name
    if home_p.exists():
        return str(home_p)
    return str(Path("/Users/kuangtalin/.elan/bin") / name)


def _verify_lean_cert(lean_path: Path, mod_name: str, build_dir: Path, flow_olean_dir: Path, tmp_dir: Path) -> list[str]:
    src = lean_path.read_text(encoding="utf-8")
    stripped = re.sub(r"/-.*?-/", "", src, flags=re.DOTALL)
    assert not re.search(r"\b(sorry|admit)\b", stripped), f"sorry/admit found in {lean_path}"
    thms = re.findall(r"^\s*theorem\s+(\S+)", src, re.MULTILINE)
    assert len(thms) >= 1, f"Expected >=1 theorem in {lean_path}"

    lean_bin = _find_tool("lean")
    leanchk_bin = _find_tool("leanchecker")
    env = dict(os.environ)
    env["LEAN_PATH"] = f"{flow_olean_dir}:{build_dir}:{tmp_dir}"
    olean = tmp_dir / f"{mod_name}.olean"

    c_res = subprocess.run(
        [lean_bin, "-DwarningAsError=true", "--root", str(tmp_dir), "-o", str(olean), str(lean_path)],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert c_res.returncode == 0, f"lean compile failed for {mod_name}:\n{c_res.stdout}\n{c_res.stderr}"

    chk_res = subprocess.run(
        [leanchk_bin, mod_name],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert chk_res.returncode == 0, f"leanchecker failed for {mod_name}:\n{chk_res.stdout}\n{chk_res.stderr}"

    ax_script = f"import {mod_name}\n" + "".join(f"#print axioms {mod_name}.{t}\n" for t in thms)
    ax_res = subprocess.run(
        [lean_bin, "-DwarningAsError=true", "--stdin"],
        env=env,
        input=ax_script,
        capture_output=True,
        text=True,
        check=False,
    )
    assert ax_res.returncode == 0, f"axiom audit failed for {mod_name}:\n{ax_res.stderr}"
    assert "sorryAx" not in ax_res.stdout, f"sorryAx leaked in {mod_name}:\n{ax_res.stdout}"
    assert "Lean.ofReduceBool" not in ax_res.stdout, f"Lean.ofReduceBool leaked in {mod_name}:\n{ax_res.stdout}"
    return thms


def main() -> None:
    root = Path(__file__).resolve().parent.parent
    os.chdir(root)
    eng = EconEngine("build/econ_cli")
    sess = EconFastSession()

    hist_dir = root / "data" / "historical"
    flow_olean_dir = root.parent / "FLOW" / "build" / "lean"
    build_dir = root / "build"

    certs_ok = 0
    certs_tot = 0

    with tempfile.TemporaryDirectory(prefix="econ_hist_") as td:
        tmp_dir = Path(td)

        # Episode 1: TAIFEX TXO 2018-02-06 Liquidity Dislocation vs. 2024 Normal Wide-Spread Regime
        cp_stress = str(tmp_dir / "taifex_20180206_stress.econcert.json")
        cp_norm = str(tmp_dir / "taifex_2024_normal.econcert.json")
        txo_stress = eng.arb_scan(str(hist_dir / "taifex_txo_20180206_stress.csv"), cert_path=cp_stress)
        txo_normal = eng.arb_scan(str(hist_dir / "taifex_txo_2024_normal.csv"), cert_path=cp_norm)
        certs_tot += 2
        if eng.cert_verify(cp_stress):
            certs_ok += 1
        if eng.cert_verify(cp_norm):
            certs_ok += 1
        assert txo_stress["arb_found"] == 1 and txo_stress["best_edge"] > 50.0, f"Expected 2018-02-06 arb: {txo_stress}"
        assert txo_normal["arb_found"] == 0 and txo_normal["sniper_false_pos"] > 0, f"Expected 2024 normal FP suppression: {txo_normal}"

        # Episode 2: 2021-05-19 Crypto Perpetual Deleveraging Cascade
        p_perp = str(hist_dir / "perp_20210519_cascade.json")
        thetas = [0.06, 0.08, 0.12, 0.16, 0.20, 0.25, 0.30]
        perp_sweep = []
        for th in thetas:
            r = sess.eval_margin(p_perp, theta_mm=th, alpha_impact=0.045, shock_pct=0.08)
            perp_sweep.append({"theta_mm": th, **r})

        cp_perp_safe = str(tmp_dir / "perp_safe.econcert.json")
        cp_perp_tight = str(tmp_dir / "perp_tight.econcert.json")
        perp_safe = eng.margin_audit(p_perp, theta=0.12, cert_path=cp_perp_safe)
        perp_tight = eng.margin_audit(p_perp, theta=0.25, cert_path=cp_perp_tight)
        certs_tot += 2
        if eng.cert_verify(cp_perp_safe):
            certs_ok += 1
        if eng.cert_verify(cp_perp_tight):
            certs_ok += 1
        assert perp_safe["bad_debt"] == 0.0 and perp_safe["paradox_detected"] == 0, f"Perp safe check failed: {perp_safe}"
        assert perp_tight["paradox_detected"] == 1 and perp_tight["bad_debt"] > 0.0, f"Perp paradox check failed: {perp_tight}"

        # Episode 3: 1980-1990 Empirical Danish Fire Heavy-Tail Calibration
        p_df = str(hist_dir / "danish_fire_empirical_1980_1990.json")
        cp_dro = str(tmp_dir / "df_dro.econcert.json")
        cp_ruin = str(tmp_dir / "df_ruin.econcert.json")
        df_dro = eng.risk_dro(p_df, cert_path=cp_dro)
        df_ruin = eng.ruin_bound(p_df, target_ruin=0.01, cert_path=cp_ruin)
        certs_tot += 2
        if eng.cert_verify(cp_dro):
            certs_ok += 1
        if eng.cert_verify(cp_ruin):
            certs_ok += 1
        assert df_dro["amp_moment_vs_gauss"] > 1.5 and df_dro["corr_psd_ok"] == 1, f"Danish fire DRO mismatch: {df_dro}"
        assert df_ruin["ruin_ratio"] > 10.0, f"Danish fire Cantelli ratio too low: {df_ruin}"

        # Episode 4: 2023 Regional-Bank Duration Mismatch Raw vs. Repaired DRC Sign-Off + Lean 4 Certs
        p_bank_raw = str(hist_dir / "inst_2023_bank_duration_raw.json")
        p_bank_rep = str(hist_dir / "inst_2023_bank_duration_repaired.json")
        cp_bank_raw = str(tmp_dir / "bank_raw.econcert.json")
        cp_bank_rep = str(tmp_dir / "bank_rep.econcert.json")
        lp_bank_raw = tmp_dir / "CertHistBankRaw.lean"
        lp_bank_rep = tmp_dir / "CertHistBankRepaired.lean"
        bank_raw = eng.audit_inst(p_bank_raw, cert_path=cp_bank_raw, lean_path=str(lp_bank_raw), mod_name="CertHistBankRaw")
        bank_rep = eng.audit_inst(p_bank_rep, cert_path=cp_bank_rep, lean_path=str(lp_bank_rep), mod_name="CertHistBankRepaired")
        certs_tot += 2
        if eng.cert_verify(cp_bank_raw):
            certs_ok += 1
        if eng.cert_verify(cp_bank_rep):
            certs_ok += 1
        assert bank_raw["certified"] == 0 and bank_raw["verdict"] == "VETOED", f"Bank raw should be VETOED: {bank_raw}"
        assert bank_rep["certified"] == 1 and bank_rep["verdict"] == "CERTIFIED", f"Bank rep should be CERTIFIED: {bank_rep}"

        thms_raw = _verify_lean_cert(lp_bank_raw, "CertHistBankRaw", build_dir, flow_olean_dir, tmp_dir)
        thms_rep = _verify_lean_cert(lp_bank_rep, "CertHistBankRepaired", build_dir, flow_olean_dir, tmp_dir)

    sess.close()
    assert certs_ok == certs_tot, f"Cert verification failed: {certs_ok}/{certs_tot}"

    out = {
        "episode1_taifex": {"stress_20180206": txo_stress, "normal_2024": txo_normal},
        "episode2_perp_20210519": {"sweep": perp_sweep, "safe_012": perp_safe, "tight_025": perp_tight},
        "episode3_danish_fire_1980_1990": {"dro": df_dro, "ruin": df_ruin},
        "episode4_bank_duration_2023": {
            "raw": bank_raw,
            "repaired": bank_rep,
            "lean_raw_theorems": thms_raw,
            "lean_rep_theorems": thms_rep,
        },
        "certs_verified": certs_ok,
        "certs_total": certs_tot,
    }
    json_out = root / "experiments" / "exp9_historical_market_backtest.json"
    json_out.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")

    md = [
        "# ECON v2.0.0 — Historical Market Stress Calibration & Backtest Report (`exp9`)",
        "",
        f"- **Exact `__int128` Certificates Verified**: `{certs_ok}/{certs_tot}` (`econ_cli cert-verify` + `lean -DwarningAsError=true` + `leanchecker`, `0 sorryAx`, `0 Lean.ofReduceBool`)",
        "",
        "## 1. Episode 1 — TAIFEX TXO 2018-02-06 Dislocation vs. 2024 Wide-Spread Regime",
        "",
        "| Regime | Strikes | Bid-Ask Arb Found | Best Edge (pts) | Butterfly Count | Legacy SNIPER False Positives |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
        f"| `taifex_txo_20180206_stress.csv` | {txo_stress['n_strikes']} | {txo_stress['arb_found']} | `{txo_stress['best_edge']:.2f}` | {txo_stress['econ_butterfly_count']} | {txo_stress['sniper_false_pos']} |",
        f"| `taifex_txo_2024_normal.csv` | {txo_normal['n_strikes']} | {txo_normal['arb_found']} | `{txo_normal['best_edge']:.2f}` | {txo_normal['econ_butterfly_count']} | {txo_normal['sniper_false_pos']} |",
        "",
        "## 2. Episode 2 — 2021-05-19 BTC Perpetual Deleveraging Spiral",
        "",
        f"- **Safe Interior Band (`theta_mm = 0.12`)**: `P_eq = {perp_safe['P_eq']:.2f} kUSD`, `liquidated = {perp_safe['liquidated_count']}/8`, `bad_debt = {perp_safe['bad_debt']:.1f}`, `macro_safety_margin = {perp_safe['macro_safety_margin']:.4f}`, `paradox_detected = {perp_safe['paradox_detected']}`.",
        f"- **Over-Tight Trigger (`theta_mm = 0.25`)**: `P_eq = {perp_tight['P_eq']:.2f} kUSD`, `liquidated = {perp_tight['liquidated_count']}/8`, `bad_debt = {perp_tight['bad_debt']:.1f}`, `macro_safety_margin = {perp_tight['macro_safety_margin']:.4f}`, `paradox_detected = {perp_tight['paradox_detected']}` (interior optimum `theta_opt = {perp_tight['theta_opt']:.3f}`).",
        "",
        "## 3. Episode 3 — 1980–1990 Empirical Danish Fire Heavy-Tail Portfolio (`alpha = 0.99`, `tail = 1.85`)",
        "",
        f"- **Gaussian VaR / CVaR vs. Copula/Moment DRO**: `gauss_VaR = {df_dro['gauss_var']:.3f}`, `gauss_CVaR = {df_dro['gauss_cvar']:.3f}`, `comonotonic_CVaR_UB = {df_dro['comonotonic_cvar_ub']:.3f}` (`{df_dro['amp_copula_vs_gauss']:.2f}x`), `moment_DRO_CVaR_UB = {df_dro['moment_dro_cvar_ub']:.3f}` (`{df_dro['amp_moment_vs_gauss']:.2f}x`), `corr_psd_ok = {df_dro['corr_psd_ok']}`.",
        f"- **Cantelli Ruin Amplification**: `gauss_ruin_prob = {df_ruin['gauss_ruin_prob']:.3e}` vs. `cantelli_ruin_ub = {df_ruin['cantelli_ruin_ub']:.4f}` (`{df_ruin['ruin_ratio']:.1f}x`), `min_capital_cantelli = {df_ruin['min_capital_cantelli']:.2f}` vs. `min_capital_gauss = {df_ruin['min_capital_gauss']:.2f}`.",
        "",
        "## 4. Episode 4 — 2023 Regional-Bank HTM Duration Mismatch (Raw vs. Repaired DRC Sign-Off)",
        "",
        "| Configuration | Verdict | `m_inc` (bps) | `m_dro` (bps) | `m_sys` (bps) | `M*` Master (bps) | Weakest |",
        "| --- | --- | ---: | ---: | ---: | ---: | --- |",
        f"| `{bank_raw['institution_name']}` | **{bank_raw['verdict']}** | {bank_raw['m_inc_bps']:.0f} | {bank_raw['m_dro_bps']:.0f} | {bank_raw['m_sys_bps']:.0f} | **{bank_raw['master_margin_bps']:.0f}** | `{bank_raw['weakest_dim']}` |",
        f"| `{bank_rep['institution_name']}` | **{bank_rep['verdict']}** | {bank_rep['m_inc_bps']:.0f} | {bank_rep['m_dro_bps']:.0f} | {bank_rep['m_sys_bps']:.0f} | **{bank_rep['master_margin_bps']:.0f}** | `{bank_rep['weakest_dim']}` |",
        "",
        f"- **Lean 4 Replay**: `CertHistBankRaw.lean` ({len(thms_raw)} thm: `{', '.join(thms_raw)}`) and `CertHistBankRepaired.lean` ({len(thms_rep)} thm: `{', '.join(thms_rep)}`) compiled with `-DwarningAsError=true` and verified by `leanchecker` (`0 sorryAx`, `0 Lean.ofReduceBool`).",
        "",
    ]
    report_out = root / "experiments" / "ECON_HISTORICAL_BACKTEST_REPORT.md"
    report_out.write_text("\n".join(md), encoding="utf-8")

    print(
        f"  [EXP9] 4 Historical Episodes (TXO_2018 edge={txo_stress['best_edge']:.1f}, "
        f"Perp_2021 bad_debt={perp_tight['bad_debt']:.0f}->0, "
        f"DanishFire DRO_amp={df_dro['amp_moment_vs_gauss']:.2f}x, "
        f"Bank2023 {bank_raw['verdict']}->{bank_rep['verdict']}), "
        f"Certs={certs_ok}/{certs_tot} -> {report_out}"
    )


if __name__ == "__main__":
    main()
