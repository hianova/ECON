# ECON v2.0.0 Verification Report

Independent Pure-Stdlib Golden-Oracle Verification & Lean 4 `leanchecker` Audit across 8 Pillars (`M1–M30`).

## 1. Pillar 2 — Model-Free Finance (Bid-Ask Arb & 2-Period MOT/SOS)

| Benchmark | ECON Result | Independent Golden Oracle | Agreement | Status |
| --- | --- | --- | --- | --- |
| taifex_txo_clean.csv | mode=0, edge=0.0000, FP=7, FN=0 | Fraction: edge=0.0000, FP=7, BF=0 | diff=0.0e+00 | PASS |
| taifex_txo_arb_conv.csv | mode=1, edge=11.0494, FP=0, FN=0 | Fraction: edge=11.0494, FP=0, BF=0 | diff=2.0e-07 | PASS |
| taifex_txo_arb_butterfly.csv | mode=3, edge=20.1500, FP=0, FN=1 | Fraction: edge=20.1500, FP=0, BF=5 | diff=0.0e+00 | PASS |
| spx_2period_skew.csv | MOT=[5.082, 7.607], ratio=0.409 | BS=4.386, Fréchet=[4.410, 10.591] | BS diff=3.8e-07 | PASS |
| btc_2period_fat_tail.csv | MOT=[9.585, 14.231], ratio=0.433 | BS=11.529, Fréchet=[8.400, 19.132] | BS diff=1.8e-07 | PASS |

## 2. Pillar 3 — Actuarial Science (Copula-Free DRO & Cantelli Ruin)

| Benchmark | ECON DRO Result | Independent Golden Oracle | Agreement | Status |
| --- | --- | --- | --- | --- |
| danish_fire_heavy_tail.json | GaussCVaR=16.04, MomDRO=26.54 | ComonCVaR=26.51, CantelliRuin=3.1920e-02 | diff=0.0e+00 | PASS |
| credit_portfolio_5d.json | GaussCVaR=13.86, MomDRO=26.57 | ComonCVaR=32.08, CantelliRuin=3.8422e-02 | diff=0.0e+00 | PASS |
| copula_obstruction_2d.json | GaussCVaR=17.70, MomDRO=28.57 | ComonCVaR=29.76, CantelliRuin=5.4623e-02 | diff=0.0e+00 | PASS |

## 3. Pillar 4 — Macro-Prudential Clearing & Fire-Sale Tarski Bounds

| Network | ECON Clearing | Independent Tarski & Bailout | Agreement | Status |
| --- | --- | --- | --- | --- |
| ring_network_6.json | p_lo=6.57, p_up=6.57, z*=3.30 | Ref Tarski=[6.57, 6.57], z*=3.30 | err=2.8e-08 | PASS |
| ecb_core_periphery_12.json | p_lo=17.50, p_up=17.50, z*=28.70 | Ref Tarski=[17.50, 17.50], z*=28.70 | err=2.0e-07 | PASS |
| stress_cascade_24.json | p_lo=17.67, p_up=17.67, z*=91.80 | Ref Tarski=[17.67, 17.67], z*=91.80 | err=2.6e-07 | PASS |

## 4. Pillar 1 — Mechanisms (VCG Combinatorial, Bilateral MS, Fisher EG)

| Benchmark | ECON Result | Independent Oracle | Agreement | Status |
| --- | --- | --- | --- | --- |
| spectrum_comb_auction.json | W=45.00, rev=43.00 | Enum W=45.00, DSIC=1, IR=1 | diff=0.0e+00 | PASS |
| ele_lmp_dispatch_auction.json | W=276.00, rev=0.00 | Exact W=276.00, DSIC=1, IR=1 | diff=0.0e+00 | PASS |
| bilateral_ms_K6 | S=-14.0000, norm=-0.047619 | Formula S=-14.0000, 2ndBest=93.75% | diff=0.0e+00 | PASS |
| fisher_4agent_3good.json | clr=1.78e-15, kkt=1.23e-13 | Walras=1.76e-14, EG=1290.6560 | KKT<1e-4 | PASS |

## 5. Pillar 5 — Principal-Agent Convexity & Clawback Design (M19–M21)

| Benchmark | ECON Agency Audit | Independent Oracle | Agreement | Status |
| --- | --- | --- | --- | --- |
| fund_2and20_raw.json | ic=-1.4800, beta*=0.126, amp=24.26x | Ref ic=-1.4800, Cantelli=[0.0148, 0.3583] | diff=2.2e-16 | PASS |
| fund_2and20_clawback.json | ic=2.6150, beta*=0.126, amp=24.26x | Ref ic=2.6150, Cantelli=[0.0148, 0.3583] | diff=4.4e-16 | PASS |

## 6. Pillar 6 — Adverse Selection & Platform Death Spiral (M22–M24)

| Benchmark | ECON Adverse Audit | Independent Oracle | Agreement | Status |
| --- | --- | --- | --- | --- |
| platform_marketplace.json | k_eq=5/10, r*=0.250, scr_r*=0.400 | Ref k_eq=5, r*=0.250, margin=0.100 | diff=0.0e+00 | PASS |
| insurance_death_spiral.json | k_eq=0/8, r*=0.250, scr_r*=0.250 | Ref k_eq=0, r*=0.250, margin=-0.170 | diff=0.0e+00 | PASS |

## 7. Pillar 7 — Margin-Liquidation Paradox & Supply-Chain Horizon (M25–M27)

| Benchmark | ECON Systemic Audit | Independent Oracle | Agreement | Status |
| --- | --- | --- | --- | --- |
| exchange_perp_8acct.json | P_eq=10.00, bd=34000, paradox=1 | Ref Tarski P_eq=10.00, bd=34000, liq=8/8 | diff=0.0e+00 | PASS |
| supply_chain_tier3_10firm.json | tau=2.00, horizon=-8.00, searu_sf=204.06 | Ref payables=258.00, M27_horizon=-8.00 | diff=0.0e+00 | PASS |

## 8. Pillar 8 — Unified 3D Commercial Institutional DRC/LVS Audits (M28–M30)

| Institution Spec | 3D Margins (INC, DRO, SYS) | Verdict / Weakest / Repair | Agreement | Status |
| --- | --- | --- | --- | --- |
| inst_fund_kpi_raw.json | M=(-7400, -34785, -5063)bps, M*=-34785bps | verdict=VETOED, weak=DRO, repair=67503.10 | min_check=EXACT | PASS |
| inst_fund_kpi_repaired.json | M=(13075, 8770, 7000)bps, M*=7000bps | verdict=CERTIFIED, weak=SYS, repair=0.00 | min_check=EXACT | PASS |
| inst_platform_raw.json | M=(-1700, -4187, -1500)bps, M*=-4187bps | verdict=VETOED, weak=DRO, repair=9327.00 | min_check=EXACT | PASS |
| inst_platform_repaired.json | M=(2500, 8603, 3500)bps, M*=2500bps | verdict=CERTIFIED, weak=INC, repair=0.00 | min_check=EXACT | PASS |
| inst_exchange_margin_raw.json | M=(600, -5473, -19739)bps, M*=-19739bps | verdict=VETOED, weak=SYS, repair=47645.60 | min_check=EXACT | PASS |
| inst_exchange_margin_repaired.json | M=(600, 5843, 1765)bps, M*=600bps | verdict=CERTIFIED, weak=INC, repair=0.00 | min_check=EXACT | PASS |
| inst_supply_chain_raw.json | M=(-100, -4644, -17354)bps, M*=-17354bps | verdict=VETOED, weak=SYS, repair=37881.20 | min_check=EXACT | PASS |
| inst_supply_chain_repaired.json | M=(800, 8746, 872)bps, M*=800bps | verdict=CERTIFIED, weak=INC, repair=0.00 | min_check=EXACT | PASS |

## 9. Zero-FP-Trust `__int128` Certificate Checks

| Certificate | Pillar | Kind | Status Tag | Verify |
| --- | --- | --- | --- | --- |
| act_copula_dro.econcert.json | actuary | risk_dro | CERTIFIED | PASS |
| act_copula_ruin.econcert.json | actuary | ruin_bound | CERTIFIED | PASS |
| act_credit_dro.econcert.json | actuary | risk_dro | CERTIFIED | PASS |
| act_credit_ruin.econcert.json | actuary | ruin_bound | CERTIFIED | PASS |
| act_danish_dro.econcert.json | actuary | risk_dro | CERTIFIED | PASS |
| act_danish_ruin.econcert.json | actuary | ruin_bound | CERTIFIED | PASS |
| adv_platform.econcert.json | adverse | adverse | CERTIFIED | PASS |
| adv_spiral.econcert.json | adverse | adverse | OBSTRUCTION | PASS |
| ag_clawback.econcert.json | agency | agency | CERTIFIED | PASS |
| ag_raw.econcert.json | agency | agency | VETOED | PASS |
| clr_ecb12.econcert.json | clearing | clearing | CERTIFIED | PASS |
| clr_ring6.econcert.json | clearing | clearing | CERTIFIED | PASS |
| clr_stress24.econcert.json | clearing | clearing | CERTIFIED | PASS |
| fin_btc_mot.econcert.json | fin | mot_sos | CERTIFIED | PASS |
| fin_spx_mot.econcert.json | fin | mot_sos | CERTIFIED | PASS |
| fin_txo_butterfly.econcert.json | fin | arb_scan | ARBITRAGE | PASS |
| fin_txo_clean.econcert.json | fin | arb_scan | CERTIFIED | PASS |
| fin_txo_conv.econcert.json | fin | arb_scan | ARBITRAGE | PASS |
| inst_exch_raw.econcert.json | inst | inst | VETOED | PASS |
| inst_exch_rep.econcert.json | inst | inst | CERTIFIED | PASS |
| inst_fund_raw.econcert.json | inst | inst | VETOED | PASS |
| inst_fund_rep.econcert.json | inst | inst | CERTIFIED | PASS |
| inst_plat_raw.econcert.json | inst | inst | VETOED | PASS |
| inst_plat_rep.econcert.json | inst | inst | CERTIFIED | PASS |
| inst_supp_raw.econcert.json | inst | inst | VETOED | PASS |
| inst_supp_rep.econcert.json | inst | inst | CERTIFIED | PASS |
| mech_ele_lmp_vcg.econcert.json | mech | vcg | CERTIFIED | PASS |
| mech_fisher4.econcert.json | mech | fisher | CERTIFIED | PASS |
| mech_ms_k6.econcert.json | mech | ms_deficit | OBSTRUCTION | PASS |
| mech_spectrum_vcg.econcert.json | mech | vcg | CERTIFIED | PASS |
| mgn_perp8.econcert.json | margin | margin | VETOED | PASS |
| sup_tier3.econcert.json | supply | clearing | CERTIFIED | PASS |

## 10. Lean 4 Kernel (`leanchecker`) Axiom Audit (`0 sorryAx`, `0 ofReduceBool`)

| Module | Axioms | Status |
| --- | --- | --- |
| CertEconAdverse | 'CertEconAdverse.platform_take_rate_margin_sound' depends on axioms: [propext, Quot.sound] | PASS |
| CertEconAgency | 'CertEconAgency.agency_clawback_ic_sound' depends on axioms: [propext, Classical.choice, Quot.sound] | PASS |
| CertEconArbBidAsk | 'CertEconArbBidAsk.certified_butterfly_arbitrage' depends on axioms: [propext, Classical.choice, Quot.sound] | PASS |
| CertEconClearing | 'CertEconClearing.clearing_tarski_bracket_sound' depends on axioms: [propext, Quot.sound] | PASS |
| CertEconCopulaObs | 'CertEconCopulaObs.dro_amplification_witness' does not depend on any axioms | PASS |
| CertEconFisherEq | 'CertEconFisherEq.fisher_clear_sound' depends on axioms: [propext, Quot.sound] | PASS |
| CertEconInstRaw | 'CertEconInstRaw.inst_minimal_repair_sound' depends on axioms: [propext, Quot.sound] | PASS |
| CertEconInstRepaired | 'CertEconInstRepaired.inst_3d_certified' depends on axioms: [propext, Classical.choice, Quot.sound] | PASS |
| CertEconMargin | 'CertEconMargin.margin_cascade_fp_sound' depends on axioms: [propext, Quot.sound] | PASS |
| CertEconMechVcg | 'CertEconMechVcg.vcg_certified_dsic_ir' depends on axioms: [propext, Quot.sound] | PASS |
| CertEconMotSos | 'CertEconMotSos.sos_form_nonneg' depends on axioms: [propext, Classical.choice, Quot.sound] | PASS |
| CertEconMsDeficit | 'CertEconMsDeficit.ms_deficit_certified' depends on axioms: [propext, Classical.choice, Quot.sound] | PASS |
| CertEconRiskDro | 'CertEconRiskDro.sos_form_nonneg' depends on axioms: [propext, Classical.choice, Quot.sound] | PASS |
| CertEconSniperUpgrade | 'CertEconSniperUpgrade.certified_no_bidask_arbitrage' does not depend on any axioms | PASS |

