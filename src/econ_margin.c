/* SPDX-License-Identifier: MIT */
#include "econ_margin.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_json_key_mgn(const char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = json;
    while ((p = strstr(p, pat)) != NULL) {
        if (p == json || isspace((unsigned char)p[-1]) || p[-1] == '{' || p[-1] == ',') {
            p += strlen(pat);
            while (*p && isspace((unsigned char)*p)) ++p;
            if (*p == ':') {
                ++p;
                while (*p && isspace((unsigned char)*p)) ++p;
                return p;
            }
        } else {
            p += strlen(pat);
        }
    }
    return NULL;
}

static int parse_d_array_mgn(const char *json, const char *key, int n, double *out) {
    const char *v = find_json_key_mgn(json, key);
    if (!v || *v != '[') return -1;
    ++v;
    for (int i = 0; i < n; ++i) {
        while (*v && (isspace((unsigned char)*v) || *v == ',')) ++v;
        char *ep = NULL;
        out[i] = strtod(v, &ep);
        if (ep == v) return -1;
        v = ep;
    }
    return 0;
}

int econ_parse_margin_market(const char *path, econ_margin_mkt_t *out) {
    if (!path || !out) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz <= 0 || sz > 2L * 1024L * 1024L) { fclose(fp); return -1; }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1U);
    if (!buf) { fclose(fp); return -1; }
    size_t nr = fread(buf, 1U, (size_t)sz, fp);
    fclose(fp);
    buf[nr] = '\0';

    memset(out, 0, sizeof(*out));
    const char *v;
    if ((v = find_json_key_mgn(buf, "n_accts")) != NULL) out->n_accts = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_mgn(buf, "P0")) != NULL) out->P0 = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "Pmin")) != NULL) out->Pmin = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "alpha_impact")) != NULL) out->alpha_impact = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "theta_mm")) != NULL) out->theta_mm = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "shock_pct")) != NULL) out->shock_pct = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "equity_target")) != NULL) out->equity_target = strtod(v, NULL);

    if (out->n_accts <= 0 || out->n_accts > ECON_CERT_MAX_ACCTS) { free(buf); return -1; }
    if (parse_d_array_mgn(buf, "h", out->n_accts, out->h) != 0 ||
        parse_d_array_mgn(buf, "d", out->n_accts, out->d) != 0 ||
        parse_d_array_mgn(buf, "c", out->n_accts, out->c) != 0) {
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}

/* Simulate endogenous liquidation cascade + exogenous bad-debt gap when theta is too loose */
static void simulate_margin_point(const econ_margin_mkt_t *mkt, double theta, double alpha,
                                  int *trig_out, double *p_eq_out, double *bad_debt_out,
                                  int *liq_cnt_out, double *liq_units_out,
                                  int *rounds_out, double *macro_margin_out) {
    int na = mkt->n_accts;
    double P_start = mkt->P0 * (1.0 - mkt->shock_pct);
    if (P_start < mkt->Pmin) P_start = mkt->Pmin;

    int cur[ECON_CERT_MAX_ACCTS] = {0}, nxt[ECON_CERT_MAX_ACCTS] = {0};
    double P_cur = P_start;
    int rounds = 0;

    for (int it = 0; it < 64; ++it) {
        double Q = 0.0;
        for (int i = 0; i < na; ++i) Q += mkt->h[i] * (double)cur[i];
        P_cur = P_start - alpha * Q;
        if (P_cur < mkt->Pmin) P_cur = mkt->Pmin;

        int changed = 0;
        for (int i = 0; i < na; ++i) {
            double req_eq = (1.0 - theta) * P_cur * mkt->h[i] + mkt->c[i];
            nxt[i] = (req_eq < mkt->d[i] - 1e-12) ? 1 : 0;
            if (nxt[i] != cur[i]) changed = 1;
        }
        memcpy(cur, nxt, (size_t)na * sizeof(int));
        ++rounds;
        if (!changed) break;
    }

    double Q = 0.0, bd = 0.0, tot_debt = 0.0, min_eq_buf = 1.0;
    int lcnt = 0;
    for (int i = 0; i < na; ++i) {
        tot_debt += mkt->d[i];
        Q += mkt->h[i] * (double)cur[i];
        lcnt += cur[i];
        double gap = mkt->d[i] - mkt->c[i] - P_cur * mkt->h[i];
        if (gap > 0.0) bd += gap;
        /* When maintenance margin theta is dangerously low, residual gap risk under jump stress hurts solvency:
           effective micro stress gap beyond instantaneous liquidation threshold */
        double buf_frac = (mkt->d[i] > 1e-12)
            ? ((mkt->c[i] + P_cur * mkt->h[i] - mkt->d[i]) / mkt->d[i])
            : 1.0;
        if (buf_frac < min_eq_buf) min_eq_buf = buf_frac;
    }

    /* Effective safety margin captures BOTH:
       (1) cascade bad-debt loss and forced-liquidation fraction from hair-trigger rules (high theta)
       (2) minimum residual equity headroom to unmodeled jump hazard (penalized when theta < 0.08) */
    double bd_ratio = (tot_debt > 1e-12) ? (bd / tot_debt) : 0.0;
    double liq_penalty = 0.25 * ((double)lcnt / (double)na);
    double undermargin_gap = (theta < 0.08) ? (0.08 - theta) * 1.5 : 0.0;
    double safety = (min_eq_buf > -1.0 ? fmin(min_eq_buf, 0.20) : -1.0)
                    - bd_ratio - liq_penalty - undermargin_gap;

    if (trig_out) memcpy(trig_out, cur, (size_t)na * sizeof(int));
    if (p_eq_out) *p_eq_out = P_cur;
    if (bad_debt_out) *bad_debt_out = bd;
    if (liq_cnt_out) *liq_cnt_out = lcnt;
    if (liq_units_out) *liq_units_out = Q;
    if (rounds_out) *rounds_out = rounds;
    if (macro_margin_out) *macro_margin_out = safety;
}

int econ_margin_audit(const econ_margin_mkt_t *mkt, double theta_override,
                      double alpha_override, const char *input_path,
                      econ_margin_res_t *res, econ_cert_record_t *cert_out) {
    if (!mkt || !res || mkt->n_accts <= 0 || mkt->n_accts > ECON_CERT_MAX_ACCTS) return -1;
    memset(res, 0, sizeof(*res));
    int na = mkt->n_accts;
    double theta = (theta_override >= 0.0) ? theta_override : mkt->theta_mm;
    double alpha = (alpha_override >= 0.0) ? alpha_override : mkt->alpha_impact;

    res->n_accts = na;
    res->theta_mm = theta;
    res->alpha_impact = alpha;
    res->P_start = fmax(mkt->Pmin, mkt->P0 * (1.0 - mkt->shock_pct));
    for (int i = 0; i < na; ++i) res->total_debt += mkt->d[i];
    res->micro_buffer_pct = theta;

    simulate_margin_point(mkt, theta, alpha, res->triggered, &res->P_eq, &res->bad_debt,
                          &res->liquidated_count, &res->liquidated_units,
                          &res->cascade_rounds, &res->macro_safety_margin);

    /* Sweep theta in [0.02, 0.45] to locate optimal interior band and detect Micro-Macro Paradox */
    double best_th = theta, best_m = -1e9;
    for (int g = 0; g <= 86; ++g) {
        double th_g = 0.02 + 0.005 * (double)g;
        double m_g = 0.0, bd_g = 0.0;
        int lc_g = 0;
        simulate_margin_point(mkt, th_g, alpha, NULL, NULL, &bd_g, &lc_g, NULL, NULL, &m_g);
        if (m_g > best_m) {
            best_m = m_g;
            best_th = th_g;
        }
    }
    res->theta_opt = best_th;
    res->opt_safety_margin = best_m;
    res->paradox_detected = (theta > best_th + 0.02 && res->macro_safety_margin + 1e-6 < best_m) ? 1 : 0;

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "margin");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "margin");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }

        const int64_t SCALE = 1000;
        const int64_t PS = 100;
        cert_out->has_margin = 1;
        cert_out->mgn_n_accts = na;
        cert_out->mgn_scale = SCALE;
        cert_out->mgn_P0 = (int64_t)llround(res->P_start * (double)PS);
        cert_out->mgn_Pmin = (int64_t)llround(mkt->Pmin * (double)PS);
        cert_out->mgn_alpha = (int64_t)llround(alpha * (double)PS * (double)SCALE);
        cert_out->mgn_theta = (int64_t)llround(theta * (double)SCALE);
        for (int i = 0; i < na; ++i) {
            cert_out->mgn_h[i] = (int64_t)llround(mkt->h[i]);
            cert_out->mgn_d[i] = (int64_t)llround(mkt->d[i] * (double)PS);
            cert_out->mgn_c[i] = (int64_t)llround(mkt->c[i] * (double)PS);
            cert_out->mgn_trig[i] = 0;
        }

        /* Exact integer Tarski fixed point matching Lean M25 `checkMarginCascade` */
        __extension__ typedef __int128 i128;
        int64_t Peq_i = cert_out->mgn_P0;
        for (int it = 0; it < 64; ++it) {
            i128 Q = 0;
            for (int i = 0; i < na; ++i) Q += (i128)cert_out->mgn_h[i] * (i128)cert_out->mgn_trig[i];
            i128 rawP = (i128)cert_out->mgn_P0 - ((i128)cert_out->mgn_alpha * Q) / (i128)SCALE;
            Peq_i = (rawP < (i128)cert_out->mgn_Pmin) ? cert_out->mgn_Pmin : (int64_t)rawP;
            int changed = 0;
            for (int i = 0; i < na; ++i) {
                i128 lhs = ((i128)SCALE - (i128)cert_out->mgn_theta) * (i128)Peq_i * (i128)cert_out->mgn_h[i] +
                           (i128)SCALE * (i128)cert_out->mgn_c[i];
                i128 rhs = (i128)SCALE * (i128)cert_out->mgn_d[i];
                int64_t nt = (lhs < rhs) ? 1 : 0;
                if (nt != cert_out->mgn_trig[i]) {
                    cert_out->mgn_trig[i] = nt;
                    changed = 1;
                }
            }
            if (!changed) break;
        }
        i128 Q_fin = 0;
        for (int i = 0; i < na; ++i) Q_fin += (i128)cert_out->mgn_h[i] * (i128)cert_out->mgn_trig[i];
        i128 rawP = (i128)cert_out->mgn_P0 - ((i128)cert_out->mgn_alpha * Q_fin) / (i128)SCALE;
        cert_out->mgn_P_eq = (rawP < (i128)cert_out->mgn_Pmin) ? cert_out->mgn_Pmin : (int64_t)rawP;

        i128 bd_i = 0;
        for (int i = 0; i < na; ++i) {
            i128 gap = (i128)cert_out->mgn_d[i] - (i128)cert_out->mgn_c[i] -
                       (i128)cert_out->mgn_P_eq * (i128)cert_out->mgn_h[i];
            if (gap > 0) bd_i += gap;
        }
        cert_out->mgn_bad_debt = (int64_t)bd_i;
        snprintf(cert_out->status, sizeof(cert_out->status), "%s",
                 (cert_out->mgn_bad_debt == 0 && !res->paradox_detected) ? "CERTIFIED" : "VETOED");
    }
    return 0;
}

int econ_parse_supply_chain(const char *path, econ_supply_chain_t *out) {
    if (!path || !out) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz <= 0 || sz > 2L * 1024L * 1024L) { fclose(fp); return -1; }
    rewind(fp);
    char *buf = (char *)malloc((size_t)sz + 1U);
    if (!buf) { fclose(fp); return -1; }
    size_t nr = fread(buf, 1U, (size_t)sz, fp);
    fclose(fp);
    buf[nr] = '\0';

    memset(out, 0, sizeof(*out));
    out->tau_horizon = 1.0;
    out->beta_default = 0.85;
    const char *v;
    if ((v = find_json_key_mgn(buf, "n_firms")) != NULL) out->n_firms = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_mgn(buf, "core_idx")) != NULL) out->core_idx = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_mgn(buf, "tau_horizon")) != NULL) out->tau_horizon = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "alpha_fire")) != NULL) out->alpha_fire = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "beta_default")) != NULL) out->beta_default = strtod(v, NULL);
    if ((v = find_json_key_mgn(buf, "shock_budget")) != NULL) out->shock_budget = strtod(v, NULL);
    if (out->n_firms <= 0 || out->n_firms > ECON_CERT_MAX_BANKS) { free(buf); return -1; }

    int nf = out->n_firms;
    if (parse_d_array_mgn(buf, "ext_assets", nf, out->ext_assets) != 0) { free(buf); return -1; }
    if ((v = find_json_key_mgn(buf, "base_payables")) == NULL || *v != '[') { free(buf); return -1; }
    ++v;
    for (int i = 0; i < nf; ++i) {
        while (*v && *v != '[') ++v;
        if (*v != '[') { free(buf); return -1; }
        ++v;
        for (int j = 0; j < nf; ++j) {
            while (*v && (isspace((unsigned char)*v) || *v == ',')) ++v;
            char *ep = NULL;
            out->L_base[i * ECON_CERT_MAX_BANKS + j] = strtod(v, &ep);
            if (ep == v) { free(buf); return -1; }
            v = ep;
        }
        while (*v && *v != ']') ++v;
        if (*v == ']') ++v;
    }
    free(buf);
    return 0;
}

int econ_supply_chain_audit(const econ_supply_chain_t *sc, double tau_override,
                            int run_searu, const char *input_path,
                            econ_supply_res_t *res, econ_cert_record_t *cert_out) {
    if (!sc || !res || sc->n_firms <= 0 || sc->n_firms > ECON_CERT_MAX_BANKS) return -1;
    memset(res, 0, sizeof(*res));
    int nf = sc->n_firms;
    double tau = (tau_override > 0.0) ? tau_override : sc->tau_horizon;
    res->n_firms = nf;
    res->tau_horizon = tau;

    econ_clearing_net_t net;
    memset(&net, 0, sizeof(net));
    net.n_banks = nf;
    net.alpha_fire = sc->alpha_fire;
    net.beta_default = sc->beta_default;
    net.shock_budget = sc->shock_budget;

    double core_base_pay = 0.0;
    for (int j = 0; j < nf; ++j) {
        core_base_pay += sc->L_base[sc->core_idx * ECON_CERT_MAX_BANKS + j];
    }
    res->core_wc_apparent_gain = (tau - 1.0) * core_base_pay;

    for (int i = 0; i < nf; ++i) {
        net.ext_assets[i] = sc->ext_assets[i];
        /* If core stretches payables horizon tau > 1, core keeps working-capital benefit while
           upstream suppliers' receivable claims (effective net financing burden) scale with tau */
        for (int j = 0; j < nf; ++j) {
            net.L[i * ECON_CERT_MAX_BANKS + j] = sc->L_base[i * ECON_CERT_MAX_BANKS + j] * tau;
        }
    }

    econ_clearing_res_t cres;
    if (econ_clear_net(&net, -1.0, run_searu, input_path, &cres, cert_out) != 0) return -1;
    if (cert_out) {
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "supply");
    }

    /* Exact M27 headroom: min_i (ext_i + sum_j L_{j,i} - sum_j L_{i,j}) */
    double min_headroom = 1e9;
    for (int i = 0; i < nf; ++i) {
        double in_i = 0.0, out_i = 0.0;
        for (int j = 0; j < nf; ++j) {
            out_i += net.L[i * ECON_CERT_MAX_BANKS + j];
            in_i  += net.L[j * ECON_CERT_MAX_BANKS + i];
        }
        double h = net.ext_assets[i] + in_i - out_i;
        if (h < min_headroom) min_headroom = h;
    }

    res->total_payables = cres.total_obligations;
    res->shock_horizon_margin = min_headroom;
    res->uniform_shortfall = cres.systemic_shortfall;
    res->searu_shortfall = cres.adv_shock_shortfall;
    res->searu_amp = cres.adv_shock_amp;
    res->min_bailout_total = cres.min_bailout_total;
    res->defaults_searu = cres.defaults_lower;

    double sf_ref = run_searu ? cres.adv_shock_shortfall : cres.systemic_shortfall;
    double tot = (cres.total_obligations > 1e-12) ? cres.total_obligations : 1.0;
    /* Normalized systemic margin > 0 when SEARU worst-case shortfall stays below zero/dust,
       < 0 when SEARU triggers material supply-chain cascade shortfall */
    res->systemic_safety_margin = (min_headroom / tot) - (sf_ref / tot) * 2.0;
    return 0;
}

int econ_fast_margin_eval(const char *path, double theta_mm, double alpha_impact,
                          double shock_pct, double *out6) {
    if (!path || !out6) return -1;
    econ_margin_mkt_t mkt;
    if (econ_parse_margin_market(path, &mkt) != 0) return -1;
    mkt.shock_pct = shock_pct;
    econ_margin_res_t res;
    if (econ_margin_audit(&mkt, theta_mm, alpha_impact, NULL, &res, NULL) != 0) return -1;
    out6[0] = res.P_eq;
    out6[1] = res.bad_debt;
    out6[2] = (double)res.liquidated_count;
    out6[3] = res.liquidated_units;
    out6[4] = res.macro_safety_margin;
    out6[5] = res.theta_opt;
    return 0;
}

int econ_fast_supply_eval(const char *path, double tau, double *out6) {
    if (!path || !out6) return -1;
    econ_supply_chain_t sc;
    if (econ_parse_supply_chain(path, &sc) != 0) return -1;
    econ_supply_res_t res;
    if (econ_supply_chain_audit(&sc, tau, 1, NULL, &res, NULL) != 0) return -1;
    out6[0] = res.core_wc_apparent_gain;
    out6[1] = res.shock_horizon_margin;
    out6[2] = res.uniform_shortfall;
    out6[3] = res.searu_shortfall;
    out6[4] = res.min_bailout_total;
    out6[5] = res.systemic_safety_margin;
    return 0;
}
