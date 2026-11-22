#!/usr/bin/env python3
"""Experiment 8 — Unified 3D Institutional DRC/LVS Audit & Minimal Repair Contrast."""

from __future__ import annotations

import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "oracles"))
from econ_sdk import EconEngine


def main() -> None:
    eng = EconEngine("build/econ_cli")
    pairs = [
        ("HedgeFund_2and20", "inst_fund_kpi_raw.json", "inst_fund_kpi_repaired.json"),
        ("Marketplace_Platform", "inst_platform_raw.json", "inst_platform_repaired.json"),
        ("Perp_Exchange_MM", "inst_exchange_margin_raw.json", "inst_exchange_margin_repaired.json"),
        ("SupplyChain_Payables", "inst_supply_chain_raw.json", "inst_supply_chain_repaired.json"),
    ]

    comparisons = []
    certs_ok = 0
    certs_tot = 0
    with tempfile.TemporaryDirectory() as tmp:
        for label, f_raw, f_rep in pairs:
            p_raw = os.path.join("benchmarks", "inst", f_raw)
            p_rep = os.path.join("benchmarks", "inst", f_rep)
            cp_raw = os.path.join(tmp, f"{f_raw}.econcert.json")
            cp_rep = os.path.join(tmp, f"{f_rep}.econcert.json")
            r_raw = eng.audit_inst(p_raw, cert_path=cp_raw)
            r_rep = eng.audit_inst(p_rep, cert_path=cp_rep)
            certs_tot += 2
            if eng.cert_verify(cp_raw):
                certs_ok += 1
            if eng.cert_verify(cp_rep):
                certs_ok += 1
            assert r_raw["certified"] == 0 and r_raw["verdict"] == "VETOED"
            assert r_rep["certified"] == 1 and r_rep["verdict"] == "CERTIFIED"
            comparisons.append({
                "institution": label,
                "raw": r_raw,
                "repaired": r_rep,
                "master_delta_bps": r_rep["master_margin_bps"] - r_raw["master_margin_bps"],
            })

    assert certs_ok == certs_tot
    out = {
        "comparisons": comparisons,
        "certs_verified": certs_ok,
        "certs_total": certs_tot,
    }
    out_path = os.path.join(os.path.dirname(__file__), "exp8_institution_drc_audit.json")
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
        f.write("\n")
    print(f"  [EXP8] Institutions={len(comparisons) * 2}, Certs={certs_ok}/{certs_tot} -> {out_path}")


if __name__ == "__main__":
    main()
