/* SPDX-License-Identifier: MIT */
#include "econ_actuary.h"
#include "econ_cert.h"
#include "econ_clearing.h"
#include "econ_fin.h"
#include "econ_mech.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(void) {
    printf("ECON v%s — Certified Model-Free Economics, Finance, Actuarial & Clearing Engine\n",
           ECON_VERSION_STRING);
    printf("Usage:\n");
    printf("  econ_cli --version | selftest | cert-verify <cert.json> [--no-hash]\n");
    printf("  econ_cli arb-scan <chain.csv> [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli mot-bound <mkt.csv> [--payoff fwd_start|var_swap|lookback] [--param P] [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli risk-dro <risk.json> [--alpha A] [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli ruin-bound <risk.json> [--target-ruin P] [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli clear-net <net.json> [--alpha-fire A] [--searu] [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli mech-verify <auc.json> [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli bilateral-ms --K <int> [--second-best] [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli fisher-eq <fisher.json> [--json f] [--cert c] [--emit-lean l --name M]\n");
    printf("  econ_cli bench --dir <bench_dir> --out <bench.json> --cert-dir <certs_dir>\n");
}

static int run_selftest(void) {
    int pass = 0, total = 16;

    /* T01: SHA-256 NIST KAT */
    char hex[65];
    econ_sha256_bytes((const uint8_t *)"abc", 3U, hex);
    int ok1 = (strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    if (ok1) pass++;
    printf("  [T01] SHA-256 NIST KAT                : %s\n", ok1 ? "PASS" : "FAIL");

    /* T02: __int128 Cholesky+SDD & Tamper Trap */
    int64_t A2[4] = {10000, -2500, -2500, 10000};
    int64_t L2[4] = {0}, mu2 = 0;
    int rc2a = econ_cert_cholesky_sdd_i64(2, A2, L2, &mu2);
    int rc2b = econ_verify_cholesky_sdd_i64(2, A2, L2, mu2 + 5000);
    int ok2 = (rc2a == 0 && mu2 > 0 && rc2b != 0);
    if (ok2) pass++;
    printf("  [T02] 128b Cholesky+SDD & Tamper Trap : %s (mu=%" PRId64 ")\n", ok2 ? "PASS" : "FAIL", mu2);

    /* T03: Two-Phase Simplex LP Strong Duality */
    double c3[2] = {3.0, 2.0};
    double A3[4] = {1.0, 1.0, 2.0, 1.0};
    econ_lp_sense_t s3[2] = {ECON_LP_GE, ECON_LP_GE};
    double b3[2] = {4.0, 6.0}, x3[2] = {0}, y3[2] = {0}, obj3 = 0.0;
    int rc3 = econ_lp_solve(2, 2, c3, A3, s3, b3, x3, y3, &obj3);
    double dual_obj3 = 4.0 * y3[0] + 6.0 * y3[1];
    int ok3 = (rc3 == 0 && fabs(obj3 - 10.0) < 1e-6 && fabs(dual_obj3 - obj3) < 1e-6);
    if (ok3) pass++;
    printf("  [T03] Simplex LP Primal=Dual Duality  : %s (p=%.4f, d=%.4f)\n",
           ok3 ? "PASS" : "FAIL", obj3, dual_obj3);

    /* T04: Bid-Ask Spread Rejects SNIPER Mid-Price False Positive */
    econ_chain_t ch4;
    memset(&ch4, 0, sizeof(ch4));
    ch4.n_strikes = 1; ch4.spot = 22500.0; ch4.tax_div = 50000.0; ch4.fee = 1.5;
    ch4.quotes[0] = (econ_option_quote_t){22500.0, 495.0, 505.0, 498.0, 508.0, 22495.0, 22503.0};
    econ_arb_res_t r4;
    econ_arb_scan(&ch4, NULL, &r4, NULL);
    int ok4 = (r4.sniper_flags == 1 && r4.sniper_false_pos == 1 && r4.arb_found == 0);
    if (ok4) pass++;
    printf("  [T04] Bid-Ask+Tax Kills SNIPER FP     : %s (flags=%d, fp=%d, arb=%d)\n",
           ok4 ? "PASS" : "FAIL", r4.sniper_flags, r4.sniper_false_pos, r4.arb_found);

    /* T05: Conversion & Butterfly M10 Arbitrage Detection */
    econ_chain_t ch5;
    memset(&ch5, 0, sizeof(ch5));
    ch5.n_strikes = 3; ch5.spot = 22500.0; ch5.tax_div = 50000.0; ch5.fee = 1.0;
    ch5.quotes[0] = (econ_option_quote_t){22300.0, 580.0, 582.0, 380.0, 382.0, 22499.0, 22501.0};
    ch5.quotes[1] = (econ_option_quote_t){22500.0, 520.0, 522.0, 519.0, 521.0, 22499.0, 22501.0};
    ch5.quotes[2] = (econ_option_quote_t){22700.0, 350.0, 352.0, 549.0, 551.0, 22499.0, 22501.0};
    econ_arb_res_t r5;
    econ_cert_record_t *c5 = (econ_cert_record_t *)calloc(1, sizeof(econ_cert_record_t));
    econ_arb_scan(&ch5, NULL, &r5, c5);
    int ok5 = (r5.arb_found == 1 && r5.best_mode == 3 && r5.sniper_false_neg >= 1 && c5->arb_edge_int > 0);
    if (ok5) pass++;
    free(c5);
    printf("  [T05] Butterfly M10 Detection (FN)    : %s (mode=%d, edge=%.2f)\n",
           ok5 ? "PASS" : "FAIL", r5.best_mode, r5.best_edge);

    /* T06: 2-Period MOT LP + Super/Sub-Hedge Duals + SOS Hankel */
    econ_mot_market_t mkt6;
    memset(&mkt6, 0, sizeof(mkt6));
    mkt6.s0 = 100.0; mkt6.fwd = 100.0; mkt6.t1 = 0.25; mkt6.t2 = 0.50; mkt6.iv_atm = 0.25;
    mkt6.n1 = 4; mkt6.n2 = 6;
    double s1_6[4] = {85.0, 95.0, 105.0, 115.0};
    double q1_6[4] = {0.15, 0.35, 0.35, 0.15};
    double s2_6[6] = {70.0, 85.0, 95.0, 105.0, 115.0, 130.0};
    double q2_6[6] = {0.07, 0.16, 0.27, 0.27, 0.16, 0.07};
    memcpy(mkt6.s1, s1_6, sizeof(s1_6)); memcpy(mkt6.q1, q1_6, sizeof(q1_6));
    memcpy(mkt6.s2, s2_6, sizeof(s2_6)); memcpy(mkt6.q2, q2_6, sizeof(q2_6));
    econ_mot_res_t r6;
    econ_mot_bound(&mkt6, ECON_PAYOFF_FWD_START, 1.0, NULL, &r6, NULL);
    int ok6 = (r6.certified && r6.frechet_lb <= r6.mot_lb + 1e-6 && r6.mot_ub <= r6.frechet_ub + 1e-6 &&
               r6.tightening_ratio < 0.99);
    if (ok6) pass++;
    printf("  [T06] 2-Period MOT LP + SOS Tightening: %s ([%.3f, %.3f], ratio=%.3f)\n",
           ok6 ? "PASS" : "FAIL", r6.mot_lb, r6.mot_ub, r6.tightening_ratio);

    /* T07: Copula-Free Risk DRO & Amplification vs Gaussian */
    econ_risk_table_t tbl7;
    memset(&tbl7, 0, sizeof(tbl7));
    tbl7.n_assets = 3; tbl7.n_bins = 32; tbl7.alpha = 0.95;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) tbl7.corr[i * 3 + j] = (i == j) ? 1.0 : 0.2;
        for (int b = 0; b < 32; ++b) {
            double u = ((double)b + 0.5) / 32.0;
            tbl7.samples[i * ECON_RISK_MAX_BINS + b] = pow(1.0 - u, -1.0 / 2.6) - 1.0;
        }
    }
    econ_dro_res_t r7;
    econ_risk_dro(&tbl7, NULL, &r7, NULL);
    int ok7 = (r7.corr_psd_ok && r7.gauss_cvar <= r7.comonotonic_cvar_ub && r7.amp_moment_vs_gauss > 1.2);
    if (ok7) pass++;
    printf("  [T07] Copula-Free DRO Risk Multiplier : %s (gauss=%.2f, momDRO=%.2f)\n",
           ok7 ? "PASS" : "FAIL", r7.gauss_cvar, r7.moment_dro_cvar_ub);

    /* T08: Cantelli One-Sided Ruin Bound vs Gaussian Normal Approx */
    econ_ruin_res_t r8;
    econ_ruin_bound(&tbl7, 0.005, NULL, &r8, NULL);
    int ok8 = (r8.cantelli_ruin_ub >= r8.gauss_ruin_prob &&
               r8.min_capital_cantelli_995 > r8.min_capital_gauss_995);
    if (ok8) pass++;
    printf("  [T08] Cantelli Ruin M13b Bound        : %s (ruin_ub=%.4f vs gauss=%.4f)\n",
           ok8 ? "PASS" : "FAIL", r8.cantelli_ruin_ub, r8.gauss_ruin_prob);

    /* T09: M15 Constructive Copula Covariance Obstruction */
    const int pA[3][3] = {{4, 1, 1}, {1, 2, 1}, {1, 1, 0}};
    const int pB[3][3] = {{3, 2, 1}, {2, 2, 0}, {1, 0, 1}};
    int eq_marg = 1, covA = 0, covB = 0;
    for (int i = 0; i < 3; ++i) {
        int ra = 0, rb = 0, ca = 0, cb = 0;
        for (int j = 0; j < 3; ++j) {
            ra += pA[i][j]; rb += pB[i][j];
            ca += pA[j][i]; cb += pB[j][i];
            covA += i * j * pA[i][j]; covB += i * j * pB[i][j];
        }
        if (ra != rb || ca != cb) eq_marg = 0;
    }
    int ok9 = (eq_marg && covA == covB && pA[2][2] == 0 && pB[2][2] == 1);
    if (ok9) pass++;
    printf("  [T09] M15 Copula Cov Obstruction      : %s (E[XY]=%d=6, crash=0 vs 1/12)\n",
           ok9 ? "PASS" : "FAIL", covA);

    /* T10: Eisenberg-Noe Clearing Tarski Fixed Point */
    econ_clearing_net_t net10;
    memset(&net10, 0, sizeof(net10));
    net10.n_banks = 4; net10.alpha_fire = 0.0; net10.beta_default = 1.0;
    net10.ext_assets[0] = 0.5; net10.ext_assets[1] = 1.5; net10.ext_assets[2] = 2.0; net10.ext_assets[3] = 50.0;
    net10.L[0 * ECON_CERT_MAX_BANKS + 1] = 6.0; net10.L[0 * ECON_CERT_MAX_BANKS + 3] = 4.0;
    net10.L[1 * ECON_CERT_MAX_BANKS + 2] = 6.0; net10.L[1 * ECON_CERT_MAX_BANKS + 3] = 4.0;
    net10.L[2 * ECON_CERT_MAX_BANKS + 0] = 6.0; net10.L[2 * ECON_CERT_MAX_BANKS + 3] = 4.0;
    econ_clearing_res_t r10;
    econ_clear_net(&net10, -1.0, 0, NULL, &r10, NULL);
    int ok10 = (fabs(r10.total_paid_lower - 10.0) < 0.05 && r10.defaults_lower == 3);
    if (ok10) pass++;
    printf("  [T10] Eisenberg-Noe Tarski Clearing   : %s (paid=%.2f/30, def=%d)\n",
           ok10 ? "PASS" : "FAIL", r10.total_paid_lower, r10.defaults_lower);

    /* T11: Fire-Sale Multi-Equilibrium + SEARU Attack */
    econ_clearing_net_t net11 = net10;
    net11.ext_assets[0] = 5.0; net11.ext_assets[1] = 5.0; net11.ext_assets[2] = 5.0;
    net11.alpha_fire = 5.0; net11.beta_default = 0.5; net11.shock_budget = 2.0;
    econ_clearing_res_t r11;
    econ_clear_net(&net11, -1.0, 1, NULL, &r11, NULL);
    int ok11 = (r11.multi_eq_gap > 10.0 && r11.adv_shock_shortfall >= r11.systemic_shortfall);
    if (ok11) pass++;
    printf("  [T11] Fire-Sale Gap & SEARU Shock     : %s (gap=%.2f, adv=%.2f)\n",
           ok11 ? "PASS" : "FAIL", r11.multi_eq_gap, r11.adv_shock_shortfall);

    /* T12: Minimum Bailout Sufficiency M18 */
    int ok12 = (r10.min_bailout_total >= 7.99 && r10.min_bailout_total <= 8.01);
    if (ok12) pass++;
    printf("  [T12] Minimum Bailout Capital M18     : %s (z*=%.2f)\n",
           ok12 ? "PASS" : "FAIL", r10.min_bailout_total);

    /* T13: VCG Combinatorial Auction DSIC + IR M5/M6 */
    econ_auction_t auc13;
    memset(&auc13, 0, sizeof(auc13));
    auc13.is_combinatorial = 1; auc13.n_agents = 3; auc13.n_items = 2;
    auc13.vals[0 * ECON_CERT_MAX_OUT + 1] = 10.0; auc13.vals[0 * ECON_CERT_MAX_OUT + 3] = 12.0;
    auc13.vals[1 * ECON_CERT_MAX_OUT + 2] = 9.0;  auc13.vals[1 * ECON_CERT_MAX_OUT + 3] = 11.0;
    auc13.vals[2 * ECON_CERT_MAX_OUT + 3] = 15.0;
    econ_vcg_res_t r13;
    econ_mech_verify(&auc13, NULL, &r13, NULL);
    int ok13 = (r13.dsic_verified && r13.ir_verified && fabs(r13.total_welfare - 19.0) < 1e-6);
    if (ok13) pass++;
    printf("  [T13] VCG Combinatorial DSIC+IR M5/M6 : %s (W=%.2f, rev=%.2f)\n",
           ok13 ? "PASS" : "FAIL", r13.total_welfare, r13.vcg_revenue);

    /* T14: Myerson-Satterthwaite Phase Transition K=4 (0) -> K=5 (-5) */
    econ_ms_res_t r14a, r14b;
    econ_bilateral_ms(4, 0, NULL, &r14a, NULL);
    econ_bilateral_ms(5, 1, NULL, &r14b, NULL);
    int ok14 = (fabs(r14a.max_budget_surplus) < 1e-5 &&
                fabs(r14b.max_budget_surplus + 5.0) < 1e-5 &&
                r14b.impossibility_holds && r14b.second_best_eff_pct > 80.0 &&
                r14b.second_best_eff_pct < 99.9);
    if (ok14) pass++;
    printf("  [T14] MS Discrete Transition K=4->5   : %s (S(4)=%.2f, S(5)=%.2f, 2nd=%.2f%%)\n",
           ok14 ? "PASS" : "FAIL", r14a.max_budget_surplus, r14b.max_budget_surplus,
           r14b.second_best_eff_pct);

    /* T15: Fisher Market Eisenberg-Gale Walras Law M8 */
    econ_fisher_mkt_t fm15;
    memset(&fm15, 0, sizeof(fm15));
    fm15.n_agents = 2; fm15.n_goods = 2;
    fm15.budgets[0] = 60.0; fm15.budgets[1] = 40.0;
    fm15.endowments[0] = 10.0; fm15.endowments[1] = 10.0;
    fm15.utils[0 * ECON_CERT_MAX_GRID + 0] = 3.0; fm15.utils[0 * ECON_CERT_MAX_GRID + 1] = 1.0;
    fm15.utils[1 * ECON_CERT_MAX_GRID + 0] = 1.0; fm15.utils[1 * ECON_CERT_MAX_GRID + 1] = 4.0;
    econ_fisher_res_t r15;
    econ_fisher_eq(&fm15, NULL, &r15, NULL);
    int ok15 = (r15.certified && r15.max_clearing_err < 1e-6 && r15.max_kkt_gap < 1e-5);
    if (ok15) pass++;
    printf("  [T15] Fisher Equilibrium Walras M8    : %s (p=[%.2f,%.2f], clr=%.1e)\n",
           ok15 ? "PASS" : "FAIL", r15.prices[0], r15.prices[1], r15.max_clearing_err);

    /* T16: Certificate Write + Exact __int128 Independent Verify */
    econ_cert_record_t *c16 = (econ_cert_record_t *)calloc(1, sizeof(econ_cert_record_t));
    econ_bilateral_ms(6, 1, NULL, &r14b, c16);
    int rc16w = econ_cert_write_json("/tmp/econ_selftest_cert.json", c16);
    int rc16v = econ_cert_verify_file("/tmp/econ_selftest_cert.json", 0);
    remove("/tmp/econ_selftest_cert.json");
    free(c16);
    int ok16 = (rc16w == 0 && rc16v == 0);
    if (ok16) pass++;
    printf("  [T16] Zero-FP Cert Write & Verify     : %s\n", ok16 ? "PASS" : "FAIL");

    printf("  SELFTEST SUMMARY: %d / %d PASSED\n", pass, total);
    return (pass == total) ? 0 : 1;
}

static void maybe_write_json_hdr(FILE *fp) { (void)fp; }

static int run_bench_suite(const char *dir, const char *out_json, const char *cert_dir) {
    char pbuf[512], cbuf[512];
    econ_cert_record_t *rec = (econ_cert_record_t *)calloc(1, sizeof(econ_cert_record_t));
    FILE *fp = fopen(out_json, "wb");
    if (!rec || !fp) { free(rec); if (fp) fclose(fp); return -1; }
    maybe_write_json_hdr(fp);
    fprintf(fp, "{\n  \"version\": \"%s\",\n", ECON_VERSION_STRING);

    /* 1. Fin: Option chains + 2-period MOT */
    static const char *chains[3] = {
        "taifex_txo_clean.csv", "taifex_txo_arb_conv.csv", "taifex_txo_arb_butterfly.csv"
    };
    fprintf(fp, "  \"arb\": [\n");
    for (int i = 0; i < 3; ++i) {
        snprintf(pbuf, sizeof(pbuf), "%s/fin/%s", dir, chains[i]);
        econ_chain_t ch;
        econ_arb_res_t res;
        if (econ_parse_option_chain(pbuf, &ch) != 0 || econ_arb_scan(&ch, pbuf, &res, rec) != 0) {
            fclose(fp); free(rec); return -1;
        }
        snprintf(cbuf, sizeof(cbuf), "%s/fin_%s.econcert.json", cert_dir,
                 (i == 0) ? "txo_clean" : ((i == 1) ? "txo_conv" : "txo_butterfly"));
        econ_cert_write_json(cbuf, rec);
        fprintf(fp, "    {\"name\": \"%s\", \"n_strikes\": %d, \"arb_found\": %d, \"best_mode\": %d, "
                    "\"best_edge\": %.6f, \"sniper_flags\": %d, \"sniper_fp\": %d, \"sniper_fn\": %d, "
                    "\"butterfly_cnt\": %d}%s\n",
                chains[i], res.n_strikes, res.arb_found, res.best_mode, res.best_edge,
                res.sniper_flags, res.sniper_false_pos, res.sniper_false_neg,
                res.econ_butterfly_count, (i + 1 < 3) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    static const char *mots[2] = {"spx_2period_skew.csv", "btc_2period_fat_tail.csv"};
    fprintf(fp, "  \"mot\": [\n");
    for (int i = 0; i < 2; ++i) {
        snprintf(pbuf, sizeof(pbuf), "%s/fin/%s", dir, mots[i]);
        econ_mot_market_t mkt;
        econ_mot_res_t res;
        if (econ_parse_mot_market(pbuf, &mkt) != 0 ||
            econ_mot_bound(&mkt, ECON_PAYOFF_FWD_START, 1.0, pbuf, &res, rec) != 0) {
            fclose(fp); free(rec); return -1;
        }
        snprintf(cbuf, sizeof(cbuf), "%s/fin_%s.econcert.json", cert_dir,
                 (i == 0) ? "spx_mot" : "btc_mot");
        econ_cert_write_json(cbuf, rec);
        fprintf(fp, "    {\"name\": \"%s\", \"frechet_lb\": %.6f, \"mot_lb\": %.6f, \"sos_lb\": %.6f, "
                    "\"bs_price\": %.6f, \"sos_ub\": %.6f, \"mot_ub\": %.6f, \"frechet_ub\": %.6f, "
                    "\"tightening_ratio\": %.6f, \"certified\": %d}%s\n",
                mots[i], res.frechet_lb, res.mot_lb, res.sos_lb, res.bs_price,
                res.sos_ub, res.mot_ub, res.frechet_ub, res.tightening_ratio,
                res.certified, (i + 1 < 2) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    /* 2. Actuary: DRO Risk + Cantelli Ruin */
    static const char *risks[3] = {
        "danish_fire_heavy_tail.json", "credit_portfolio_5d.json", "copula_obstruction_2d.json"
    };
    fprintf(fp, "  \"actuary\": [\n");
    for (int i = 0; i < 3; ++i) {
        snprintf(pbuf, sizeof(pbuf), "%s/actuary/%s", dir, risks[i]);
        econ_risk_table_t tbl;
        econ_dro_res_t rd;
        econ_ruin_res_t rr;
        if (econ_parse_risk_table(pbuf, &tbl) != 0 || econ_risk_dro(&tbl, pbuf, &rd, rec) != 0) {
            fclose(fp); free(rec); return -1;
        }
        snprintf(cbuf, sizeof(cbuf), "%s/act_%s.econcert.json", cert_dir,
                 (i == 0) ? "danish_dro" : ((i == 1) ? "credit_dro" : "copula_dro"));
        econ_cert_write_json(cbuf, rec);
        if (econ_ruin_bound(&tbl, 0.005, pbuf, &rr, rec) != 0) {
            fclose(fp); free(rec); return -1;
        }
        snprintf(cbuf, sizeof(cbuf), "%s/act_%s_ruin.econcert.json", cert_dir,
                 (i == 0) ? "danish" : ((i == 1) ? "credit" : "copula"));
        econ_cert_write_json(cbuf, rec);
        fprintf(fp, "    {\"name\": \"%s\", \"n_assets\": %d, \"gauss_var\": %.6f, \"gauss_cvar\": %.6f, "
                    "\"ra_var_ub\": %.6f, \"comon_cvar_ub\": %.6f, \"mom_dro_cvar_ub\": %.6f, "
                    "\"amp_copula\": %.6f, \"amp_moment\": %.6f, \"gauss_ruin\": %.8e, "
                    "\"cantelli_ruin_ub\": %.8e, \"min_cap_gauss\": %.6f, \"min_cap_cantelli\": %.6f}%s\n",
                risks[i], rd.n_assets, rd.gauss_var, rd.gauss_cvar, rd.ra_var_ub,
                rd.comonotonic_cvar_ub, rd.moment_dro_cvar_ub, rd.amp_copula_vs_gauss,
                rd.amp_moment_vs_gauss, rr.gauss_ruin_prob, rr.cantelli_ruin_ub,
                rr.min_capital_gauss_995, rr.min_capital_cantelli_995, (i + 1 < 3) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    /* 3. Clearing Networks */
    static const char *nets[3] = {
        "ring_network_6.json", "ecb_core_periphery_12.json", "stress_cascade_24.json"
    };
    fprintf(fp, "  \"clearing\": [\n");
    for (int i = 0; i < 3; ++i) {
        snprintf(pbuf, sizeof(pbuf), "%s/clearing/%s", dir, nets[i]);
        econ_clearing_net_t net;
        econ_clearing_res_t res;
        if (econ_parse_clearing_net(pbuf, &net) != 0 ||
            econ_clear_net(&net, -1.0, 1, pbuf, &res, rec) != 0) {
            fclose(fp); free(rec); return -1;
        }
        snprintf(cbuf, sizeof(cbuf), "%s/clr_%s.econcert.json", cert_dir,
                 (i == 0) ? "ring6" : ((i == 1) ? "ecb12" : "stress24"));
        econ_cert_write_json(cbuf, rec);
        fprintf(fp, "    {\"name\": \"%s\", \"n_banks\": %d, \"total_obligations\": %.6f, "
                    "\"paid_upper\": %.6f, \"paid_lower\": %.6f, \"multi_eq_gap\": %.6f, "
                    "\"defaults_upper\": %d, \"defaults_lower\": %d, \"min_bailout\": %.6f, "
                    "\"adv_shortfall\": %.6f, \"adv_amp\": %.6f}%s\n",
                nets[i], res.n_banks, res.total_obligations, res.total_paid_upper,
                res.total_paid_lower, res.multi_eq_gap, res.defaults_upper, res.defaults_lower,
                res.min_bailout_total, res.adv_shock_shortfall, res.adv_shock_amp,
                (i + 1 < 3) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    /* 4. Mechanisms: VCG + MS + Fisher */
    static const char *aucs[2] = {"spectrum_comb_auction.json", "ele_lmp_dispatch_auction.json"};
    fprintf(fp, "  \"vcg\": [\n");
    for (int i = 0; i < 2; ++i) {
        snprintf(pbuf, sizeof(pbuf), "%s/mech/%s", dir, aucs[i]);
        econ_auction_t auc;
        econ_vcg_res_t res;
        if (econ_parse_auction(pbuf, &auc) != 0 || econ_mech_verify(&auc, pbuf, &res, rec) != 0) {
            fclose(fp); free(rec); return -1;
        }
        snprintf(cbuf, sizeof(cbuf), "%s/mech_%s.econcert.json", cert_dir,
                 (i == 0) ? "spectrum_vcg" : "ele_lmp_vcg");
        econ_cert_write_json(cbuf, rec);
        fprintf(fp, "    {\"name\": \"%s\", \"n_agents\": %d, \"n_outcomes\": %d, "
                    "\"total_welfare\": %.6f, \"vcg_revenue\": %.6f, \"myerson_rev\": %.6f, "
                    "\"dsic_ok\": %d, \"ir_ok\": %d}%s\n",
                aucs[i], res.n_agents, res.n_outcomes, res.total_welfare, res.vcg_revenue,
                res.myerson_reserve_rev, res.dsic_verified, res.ir_verified, (i + 1 < 2) ? "," : "");
    }
    fprintf(fp, "  ],\n");

    econ_ms_res_t ms6;
    econ_bilateral_ms(6, 1, NULL, &ms6, rec);
    snprintf(cbuf, sizeof(cbuf), "%s/mech_ms_k6.econcert.json", cert_dir);
    econ_cert_write_json(cbuf, rec);
    fprintf(fp, "  \"ms\": {\"K\": %d, \"efficient_welfare\": %.6f, \"min_rent\": %.6f, "
                "\"max_surplus\": %.6f, \"norm_ratio\": %.6f, \"formula\": %.6f, "
                "\"second_best_eff_pct\": %.6f},\n",
            ms6.K, ms6.efficient_welfare, ms6.min_rent_required, ms6.max_budget_surplus,
            ms6.norm_surplus_ratio, ms6.theoretical_formula, ms6.second_best_eff_pct);

    snprintf(pbuf, sizeof(pbuf), "%s/mech/fisher_4agent_3good.json", dir);
    econ_fisher_mkt_t fm;
    econ_fisher_res_t rf;
    if (econ_parse_fisher(pbuf, &fm) != 0 || econ_fisher_eq(&fm, pbuf, &rf, rec) != 0) {
        fclose(fp); free(rec); return -1;
    }
    snprintf(cbuf, sizeof(cbuf), "%s/mech_fisher4.econcert.json", cert_dir);
    econ_cert_write_json(cbuf, rec);
    fprintf(fp, "  \"fisher\": {\"name\": \"fisher_4agent_3good.json\", \"n_agents\": %d, \"n_goods\": %d, "
                "\"max_clearing_err\": %.8e, \"walras_residual\": %.8e, \"max_kkt_gap\": %.8e, "
                "\"eg_welfare\": %.6f, \"certified\": %d}\n}\n",
            rf.n_agents, rf.n_goods, rf.max_clearing_err, rf.walras_residual, rf.max_kkt_gap,
            rf.social_welfare_eg, rf.certified);

    fclose(fp);
    free(rec);
    printf("  [BENCH] Verified EM-style 4-Pillar ECON Suite (Fin + Actuary + Clearing + Mech) -> %s\n", out_json);
    return 0;
}

static void parse_common_flags(int argc, char **argv, int start_idx,
                               const char **json_out, const char **cert_out,
                               const char **lean_out, const char **mod_name) {
    for (int i = start_idx; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0 && i + 1 < argc) *json_out = argv[++i];
        else if (strcmp(argv[i], "--cert") == 0 && i + 1 < argc) *cert_out = argv[++i];
        else if (strcmp(argv[i], "--emit-lean") == 0 && i + 1 < argc) *lean_out = argv[++i];
        else if (strcmp(argv[i], "--name") == 0 && i + 1 < argc) *mod_name = argv[++i];
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }
    if (strcmp(argv[1], "--version") == 0) {
        printf("ECON %s\n", ECON_VERSION_STRING);
        return 0;
    }
    if (strcmp(argv[1], "selftest") == 0) return run_selftest();

    if (strcmp(argv[1], "cert-verify") == 0 && argc >= 3) {
        int verify_hash = 1;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--no-hash") == 0) verify_hash = 0;
        }
        int rc = econ_cert_verify_file(argv[2], verify_hash);
        printf("[CERT-VERIFY] %s -> %s\n", argv[2], (rc == 0) ? "PASS" : "FAIL");
        return (rc == 0) ? 0 : 1;
    }

    if (strcmp(argv[1], "bench") == 0) {
        const char *dir = "benchmarks", *out = "build/econ_bench.json", *cdir = "build/certs";
        for (int i = 2; i + 1 < argc; ++i) {
            if (strcmp(argv[i], "--dir") == 0) dir = argv[++i];
            else if (strcmp(argv[i], "--out") == 0) out = argv[++i];
            else if (strcmp(argv[i], "--cert-dir") == 0) cdir = argv[++i];
        }
        return (run_bench_suite(dir, out, cdir) == 0) ? 0 : 1;
    }

    const char *json_out = NULL, *cert_out = NULL, *lean_out = NULL, *mod_name = "CertEcon";
    econ_cert_record_t *rec = (econ_cert_record_t *)calloc(1, sizeof(econ_cert_record_t));
    if (!rec) return 1;
    int rc = 1;

    if (strcmp(argv[1], "arb-scan") == 0 && argc >= 3) {
        parse_common_flags(argc, argv, 3, &json_out, &cert_out, &lean_out, &mod_name);
        econ_chain_t ch; econ_arb_res_t res;
        if (econ_parse_option_chain(argv[2], &ch) == 0 && econ_arb_scan(&ch, argv[2], &res, rec) == 0) {
            printf("[ARB-SCAN] %s -> strikes=%d arb=%d mode=%d edge=%.4f sniper(flags=%d, fp=%d, fn=%d)\n",
                   argv[2], res.n_strikes, res.arb_found, res.best_mode, res.best_edge,
                   res.sniper_flags, res.sniper_false_pos, res.sniper_false_neg);
            if (json_out) {
                FILE *fp = fopen(json_out, "wb");
                if (fp) {
                    fprintf(fp, "{\"n_strikes\": %d, \"arb_found\": %d, \"best_mode\": %d, \"best_edge\": %.8f, "
                                "\"best_k1\": %.4f, \"best_k2\": %.4f, \"best_k3\": %.4f, "
                                "\"sniper_flags\": %d, \"sniper_false_pos\": %d, \"sniper_false_neg\": %d, "
                                "\"econ_conv_count\": %d, \"econ_rev_count\": %d, "
                                "\"econ_vertical_count\": %d, \"econ_butterfly_count\": %d}\n",
                            res.n_strikes, res.arb_found, res.best_mode, res.best_edge,
                            res.best_k1, res.best_k2, res.best_k3,
                            res.sniper_flags, res.sniper_false_pos, res.sniper_false_neg,
                            res.econ_conv_count, res.econ_rev_count,
                            res.econ_vertical_count, res.econ_butterfly_count);
                    fclose(fp);
                }
            }
            rc = 0;
        }
    } else if (strcmp(argv[1], "mot-bound") == 0 && argc >= 3) {
        econ_payoff_t pk = ECON_PAYOFF_FWD_START;
        double param = 1.0;
        for (int i = 3; i + 1 < argc; ++i) {
            if (strcmp(argv[i], "--payoff") == 0) {
                const char *ps = argv[++i];
                if (strcmp(ps, "var_swap") == 0) pk = ECON_PAYOFF_VAR_SWAP;
                else if (strcmp(ps, "lookback") == 0) pk = ECON_PAYOFF_LOOKBACK;
                else pk = ECON_PAYOFF_FWD_START;
            } else if (strcmp(argv[i], "--param") == 0) {
                param = strtod(argv[++i], NULL);
            }
        }
        parse_common_flags(argc, argv, 3, &json_out, &cert_out, &lean_out, &mod_name);
        econ_mot_market_t mkt; econ_mot_res_t res;
        if (econ_parse_mot_market(argv[2], &mkt) == 0 && econ_mot_bound(&mkt, pk, param, argv[2], &res, rec) == 0) {
            printf("[MOT-BOUND] %s -> Frechet=[%.4f, %.4f] MOT=[%.4f, %.4f] SOS=[%.4f, %.4f] BS=%.4f ratio=%.4f\n",
                   argv[2], res.frechet_lb, res.frechet_ub, res.mot_lb, res.mot_ub,
                   res.sos_lb, res.sos_ub, res.bs_price, res.tightening_ratio);
            if (json_out) {
                FILE *fp = fopen(json_out, "wb");
                if (fp) {
                    fprintf(fp, "{\"frechet_lb\": %.8f, \"mot_lb\": %.8f, \"sos_lb\": %.8f, "
                                "\"bs_price\": %.8f, \"sos_ub\": %.8f, \"mot_ub\": %.8f, "
                                "\"frechet_ub\": %.8f, \"tightening_ratio\": %.8f, \"certified\": %d}\n",
                            res.frechet_lb, res.mot_lb, res.sos_lb, res.bs_price,
                            res.sos_ub, res.mot_ub, res.frechet_ub, res.tightening_ratio, res.certified);
                    fclose(fp);
                }
            }
            rc = 0;
        }
    } else if (strcmp(argv[1], "risk-dro") == 0 && argc >= 3) {
        double alpha_ov = -1.0;
        for (int i = 3; i + 1 < argc; ++i) if (strcmp(argv[i], "--alpha") == 0) alpha_ov = strtod(argv[++i], NULL);
        parse_common_flags(argc, argv, 3, &json_out, &cert_out, &lean_out, &mod_name);
        econ_risk_table_t tbl; econ_dro_res_t res;
        if (econ_parse_risk_table(argv[2], &tbl) == 0) {
            if (alpha_ov > 0.5 && alpha_ov < 1.0) tbl.alpha = alpha_ov;
            if (econ_risk_dro(&tbl, argv[2], &res, rec) == 0) {
                printf("[RISK-DRO] %s -> GaussCVaR=%.4f ComonCVaR=%.4f MomDRO=%.4f (amp=%.2fx/%.2fx)\n",
                       argv[2], res.gauss_cvar, res.comonotonic_cvar_ub, res.moment_dro_cvar_ub,
                       res.amp_copula_vs_gauss, res.amp_moment_vs_gauss);
                if (json_out) {
                    FILE *fp = fopen(json_out, "wb");
                    if (fp) {
                        fprintf(fp, "{\"n_assets\": %d, \"alpha\": %.6f, \"gauss_var\": %.8f, "
                                    "\"gauss_cvar\": %.8f, \"ra_var_ub\": %.8f, "
                                    "\"comonotonic_cvar_ub\": %.8f, \"moment_dro_cvar_ub\": %.8f, "
                                    "\"amp_copula_vs_gauss\": %.8f, \"amp_moment_vs_gauss\": %.8f, "
                                    "\"corr_psd_ok\": %d}\n",
                                res.n_assets, res.alpha, res.gauss_var, res.gauss_cvar,
                                res.ra_var_ub, res.comonotonic_cvar_ub, res.moment_dro_cvar_ub,
                                res.amp_copula_vs_gauss, res.amp_moment_vs_gauss, res.corr_psd_ok);
                        fclose(fp);
                    }
                }
                rc = 0;
            }
        }
    } else if (strcmp(argv[1], "ruin-bound") == 0 && argc >= 3) {
        double tr = 0.005;
        for (int i = 3; i + 1 < argc; ++i) if (strcmp(argv[i], "--target-ruin") == 0) tr = strtod(argv[++i], NULL);
        parse_common_flags(argc, argv, 3, &json_out, &cert_out, &lean_out, &mod_name);
        econ_risk_table_t tbl; econ_ruin_res_t res;
        if (econ_parse_risk_table(argv[2], &tbl) == 0 && econ_ruin_bound(&tbl, tr, argv[2], &res, rec) == 0) {
            printf("[RUIN-BOUND] %s -> p_gauss=%.4e p_cantelli_ub=%.4e (ratio=%.1fx, u*_995: %.2f vs %.2f)\n",
                   argv[2], res.gauss_ruin_prob, res.cantelli_ruin_ub, res.ruin_ratio_ub_over_gauss,
                   res.min_capital_gauss_995, res.min_capital_cantelli_995);
            if (json_out) {
                FILE *fp = fopen(json_out, "wb");
                if (fp) {
                    fprintf(fp, "{\"mu_loss\": %.8f, \"sigma_loss\": %.8f, \"threshold_u\": %.8f, "
                                "\"gauss_ruin_prob\": %.10e, \"cantelli_ruin_ub\": %.10e, "
                                "\"ruin_ratio\": %.8f, \"min_capital_gauss\": %.8f, "
                                "\"min_capital_cantelli\": %.8f}\n",
                            res.mu_loss, res.sigma_loss, res.threshold_u, res.gauss_ruin_prob,
                            res.cantelli_ruin_ub, res.ruin_ratio_ub_over_gauss,
                            res.min_capital_gauss_995, res.min_capital_cantelli_995);
                    fclose(fp);
                }
            }
            rc = 0;
        }
    } else if (strcmp(argv[1], "clear-net") == 0 && argc >= 3) {
        double af = -1.0;
        int searu = 0;
        for (int i = 3; i < argc; ++i) {
            if (strcmp(argv[i], "--alpha-fire") == 0 && i + 1 < argc) af = strtod(argv[++i], NULL);
            else if (strcmp(argv[i], "--searu") == 0) searu = 1;
        }
        parse_common_flags(argc, argv, 3, &json_out, &cert_out, &lean_out, &mod_name);
        econ_clearing_net_t net; econ_clearing_res_t res;
        if (econ_parse_clearing_net(argv[2], &net) == 0 &&
            econ_clear_net(&net, af, searu, argv[2], &res, rec) == 0) {
            printf("[CLEAR-NET] %s -> banks=%d p_up=%.2f p_lo=%.2f gap=%.2f def=(%d,%d) z*=%.2f adv=%.2f\n",
                   argv[2], res.n_banks, res.total_paid_upper, res.total_paid_lower, res.multi_eq_gap,
                   res.defaults_upper, res.defaults_lower, res.min_bailout_total, res.adv_shock_shortfall);
            if (json_out) {
                FILE *fp = fopen(json_out, "wb");
                if (fp) {
                    fprintf(fp, "{\"n_banks\": %d, \"total_obligations\": %.8f, \"paid_upper\": %.8f, "
                                "\"paid_lower\": %.8f, \"multi_eq_gap\": %.8f, \"defaults_upper\": %d, "
                                "\"defaults_lower\": %d, \"cascade_rounds\": %d, \"systemic_shortfall\": %.8f, "
                                "\"min_bailout_total\": %.8f, \"adv_shock_shortfall\": %.8f, "
                                "\"adv_shock_amp\": %.8f}\n",
                            res.n_banks, res.total_obligations, res.total_paid_upper, res.total_paid_lower,
                            res.multi_eq_gap, res.defaults_upper, res.defaults_lower, res.cascade_rounds,
                            res.systemic_shortfall, res.min_bailout_total, res.adv_shock_shortfall,
                            res.adv_shock_amp);
                    fclose(fp);
                }
            }
            rc = 0;
        }
    } else if (strcmp(argv[1], "mech-verify") == 0 && argc >= 3) {
        parse_common_flags(argc, argv, 3, &json_out, &cert_out, &lean_out, &mod_name);
        econ_auction_t auc; econ_vcg_res_t res;
        if (econ_parse_auction(argv[2], &auc) == 0 && econ_mech_verify(&auc, argv[2], &res, rec) == 0) {
            printf("[MECH-VERIFY] %s -> W=%.2f rev=%.2f dsic=%d ir=%d\n",
                   argv[2], res.total_welfare, res.vcg_revenue, res.dsic_verified, res.ir_verified);
            if (json_out) {
                FILE *fp = fopen(json_out, "wb");
                if (fp) {
                    fprintf(fp, "{\"n_agents\": %d, \"n_outcomes\": %d, \"chosen_outcome\": %d, "
                                "\"total_welfare\": %.8f, \"vcg_revenue\": %.8f, \"myerson_reserve_rev\": %.8f, "
                                "\"dsic_verified\": %d, \"ir_verified\": %d}\n",
                            res.n_agents, res.n_outcomes, res.chosen_outcome, res.total_welfare,
                            res.vcg_revenue, res.myerson_reserve_rev, res.dsic_verified, res.ir_verified);
                    fclose(fp);
                }
            }
            rc = 0;
        }
    } else if (strcmp(argv[1], "bilateral-ms") == 0) {
        int K = 6, sb = 0;
        for (int i = 2; i < argc; ++i) {
            if (strcmp(argv[i], "--K") == 0 && i + 1 < argc) K = (int)strtol(argv[++i], NULL, 10);
            else if (strcmp(argv[i], "--second-best") == 0) sb = 1;
        }
        parse_common_flags(argc, argv, 2, &json_out, &cert_out, &lean_out, &mod_name);
        econ_ms_res_t res;
        if (econ_bilateral_ms(K, sb, NULL, &res, rec) == 0) {
            printf("[BILATERAL-MS] K=%d W=%.2f rent=%.2f surplus=%.4f norm=%.6f (formula=%.6f) 2ndBest=%.2f%%\n",
                   res.K, res.efficient_welfare, res.min_rent_required, res.max_budget_surplus,
                   res.norm_surplus_ratio, res.theoretical_formula, res.second_best_eff_pct);
            if (json_out) {
                FILE *fp = fopen(json_out, "wb");
                if (fp) {
                    fprintf(fp, "{\"K\": %d, \"efficient_welfare\": %.8f, \"min_rent_required\": %.8f, "
                                "\"max_budget_surplus\": %.8f, \"norm_surplus_ratio\": %.8f, "
                                "\"theoretical_formula\": %.8f, \"second_best_welfare\": %.8f, "
                                "\"second_best_eff_pct\": %.8f, \"impossibility_holds\": %d}\n",
                            res.K, res.efficient_welfare, res.min_rent_required, res.max_budget_surplus,
                            res.norm_surplus_ratio, res.theoretical_formula, res.second_best_welfare,
                            res.second_best_eff_pct, res.impossibility_holds);
                    fclose(fp);
                }
            }
            rc = 0;
        }
    } else if (strcmp(argv[1], "fisher-eq") == 0 && argc >= 3) {
        parse_common_flags(argc, argv, 3, &json_out, &cert_out, &lean_out, &mod_name);
        econ_fisher_mkt_t fm; econ_fisher_res_t res;
        if (econ_parse_fisher(argv[2], &fm) == 0 && econ_fisher_eq(&fm, argv[2], &res, rec) == 0) {
            printf("[FISHER-EQ] %s -> clr=%.2e walras=%.2e kkt=%.2e EG=%.4f (%s)\n",
                   argv[2], res.max_clearing_err, res.walras_residual, res.max_kkt_gap,
                   res.social_welfare_eg, res.certified ? "CERTIFIED" : "VETOED");
            if (json_out) {
                FILE *fp = fopen(json_out, "wb");
                if (fp) {
                    fprintf(fp, "{\"n_agents\": %d, \"n_goods\": %d, \"max_clearing_err\": %.10e, "
                                "\"walras_residual\": %.10e, \"max_kkt_gap\": %.10e, "
                                "\"social_welfare_eg\": %.8f, \"certified\": %d}\n",
                            res.n_agents, res.n_goods, res.max_clearing_err, res.walras_residual,
                            res.max_kkt_gap, res.social_welfare_eg, res.certified);
                    fclose(fp);
                }
            }
            rc = 0;
        }
    } else {
        print_usage();
    }

    if (rc == 0) {
        if (cert_out && econ_cert_write_json(cert_out, rec) != 0) rc = 1;
        if (lean_out && econ_cert_emit_lean(lean_out, mod_name, rec) != 0) rc = 1;
    }
    free(rec);
    return rc;
}
