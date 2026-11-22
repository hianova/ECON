/* SPDX-License-Identifier: MIT */
#include "econ_inst.h"
#include "econ_actuary.h"
#include "econ_adverse.h"
#include "econ_agency.h"
#include "econ_clearing.h"
#include "econ_margin.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_json_key_inst(const char *json, const char *key) {
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

static int parse_str_inst(const char *json, const char *key, char *out, size_t cap) {
    const char *v = find_json_key_inst(json, key);
    if (!v || *v != '"') return -1;
    ++v;
    size_t k = 0U;
    while (*v && *v != '"') {
        if (k + 1U < cap) out[k++] = *v;
        ++v;
    }
    out[k] = '\0';
    return 0;
}

static int parse_double_inst(const char *json, const char *key, double def_val, double *out) {
    const char *v = find_json_key_inst(json, key);
    if (!v) { *out = def_val; return -1; }
    char *ep = NULL;
    double val = strtod(v, &ep);
    if (ep == v) { *out = def_val; return -1; }
    *out = val;
    return 0;
}

int econ_parse_inst_spec(const char *path, econ_inst_spec_t *out) {
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
    out->target_margin_bps = 500.0;
    out->c_inc = 1.0; out->c_dro = 1.0; out->c_sys = 1.0;
    out->beta_override = -1.0;
    out->take_override = -1.0;
    out->dro_reserve_mult = 1.0;
    out->theta_override = -1.0;
    out->tau_override = -1.0;
    out->bailout_buffer = 0.0;
    out->chain_fee_bps = 400.0;

    parse_str_inst(buf, "institution_name", out->institution_name, sizeof(out->institution_name));
    if (parse_str_inst(buf, "institution_type", out->institution_type, sizeof(out->institution_type)) != 0) {
        free(buf);
        return -1;
    }
    parse_double_inst(buf, "target_margin_bps", 500.0, &out->target_margin_bps);
    parse_double_inst(buf, "c_inc", 1.0, &out->c_inc);
    parse_double_inst(buf, "c_dro", 1.0, &out->c_dro);
    parse_double_inst(buf, "c_sys", 1.0, &out->c_sys);

    parse_str_inst(buf, "inc_ref_path", out->inc_ref_path, sizeof(out->inc_ref_path));
    parse_str_inst(buf, "dro_ref_path", out->dro_ref_path, sizeof(out->dro_ref_path));
    parse_str_inst(buf, "sys_ref_path", out->sys_ref_path, sizeof(out->sys_ref_path));

    parse_double_inst(buf, "beta_override", -1.0, &out->beta_override);
    parse_double_inst(buf, "take_override", -1.0, &out->take_override);
    parse_double_inst(buf, "dro_reserve_mult", 1.0, &out->dro_reserve_mult);
    parse_double_inst(buf, "theta_override", -1.0, &out->theta_override);
    parse_double_inst(buf, "tau_override", -1.0, &out->tau_override);
    parse_double_inst(buf, "bailout_buffer", 0.0, &out->bailout_buffer);
    parse_double_inst(buf, "chain_fee_bps", 400.0, &out->chain_fee_bps);

    free(buf);
    return 0;
}

static double eval_dro_margin_bps(const econ_inst_spec_t *spec) {
    if (spec->dro_ref_path[0] == '\0') return spec->chain_fee_bps;
    econ_risk_table_t tbl;
    if (econ_parse_risk_table(spec->dro_ref_path, &tbl) != 0) return -1000.0;
    econ_ruin_res_t rr;
    if (econ_ruin_bound(&tbl, 0.01, NULL, &rr, NULL) != 0) return -1000.0;
    double effective_cap = rr.threshold_u * spec->dro_reserve_mult;
    double denom = fmax(rr.min_capital_cantelli_995, 1.0);
    return 10000.0 * (effective_cap - rr.min_capital_cantelli_995) / denom;
}

int econ_audit_institution(const econ_inst_spec_t *spec, const char *input_path,
                           econ_inst_res_t *res, econ_cert_record_t *cert_out) {
    if (!spec || !res) return -1;
    memset(res, 0, sizeof(*res));
    snprintf(res->institution_name, sizeof(res->institution_name), "%s", spec->institution_name);
    snprintf(res->institution_type, sizeof(res->institution_type), "%s", spec->institution_type);
    res->target_margin_bps = spec->target_margin_bps;

    double m_inc = 0.0, m_dro = 0.0, m_sys = 0.0;
    char note[160] = "All 3 institutional safety margins strictly positive";

    if (strcmp(spec->institution_type, "fund_agency") == 0) {
        econ_agency_spec_t ag;
        econ_agency_res_t ar;
        if (econ_parse_agency_spec(spec->inc_ref_path, &ag) != 0 ||
            econ_agency_audit(&ag, spec->beta_override, NULL, &ar, NULL) != 0) return -1;
        /* Normalize incentive gap into bps of strike/capital scale */
        double norm_scale = fmax(fabs(ag.strike_K), 10.0) * fmax(ag.alpha, 0.1);
        m_inc = 10000.0 * ar.ic_margin / norm_scale;
        /* Cantelli reserve margin against agent-chosen strategy variance */
        double chosen_ruin = ar.ic_satisfied ? ar.cantelli_ruin_safe : ar.cantelli_ruin_risk;
        double ruin_cap = 0.08 * spec->dro_reserve_mult;
        m_dro = 10000.0 * (ruin_cap - chosen_ruin) / fmax(ruin_cap, 0.01);

        if (spec->sys_ref_path[0] != '\0') {
            econ_clearing_net_t net;
            econ_clearing_res_t cr;
            if (econ_parse_clearing_net(spec->sys_ref_path, &net) != 0) return -1;
            for (int i = 0; i < net.n_banks; ++i) net.ext_assets[i] += spec->bailout_buffer;
            if (econ_clear_net(&net, -1.0, 1, NULL, &cr, NULL) != 0) return -1;
            double tot = fmax(cr.total_obligations, 1.0);
            double sf = cr.adv_shock_shortfall;
            m_sys = 10000.0 * (spec->chain_fee_bps / 10000.0 - sf / tot);
        } else {
            m_sys = 800.0;
        }
        if (m_inc <= 0.0) {
            snprintf(note, sizeof(note), "Limited-liability call convexity lures risky strategy (m_inc=%.0fbps)", m_inc);
        } else if (m_dro <= 0.0) {
            snprintf(note, sizeof(note), "Model-free Cantelli tail ruin exceeds reserve limit (m_dro=%.0fbps)", m_dro);
        }
    } else if (strcmp(spec->institution_type, "platform_adverse") == 0) {
        econ_adverse_spec_t adv;
        econ_adverse_res_t av;
        if (econ_parse_adverse_spec(spec->inc_ref_path, &adv) != 0 ||
            econ_adverse_audit(&adv, spec->take_override, NULL, &av, NULL) != 0) return -1;
        double eff_r_star = av.screening_valid ? av.screened_r_star : av.critical_r_star;
        m_inc = 10000.0 * (eff_r_star - av.take_rate);
        if (av.full_collapse && !av.screening_valid) {
            m_inc = -10000.0 * fmax(av.take_rate - av.critical_r_star, 0.05);
        }
        m_dro = eval_dro_margin_bps(spec);
        if (spec->sys_ref_path[0] != '\0') {
            econ_clearing_net_t net;
            econ_clearing_res_t cr;
            if (econ_parse_clearing_net(spec->sys_ref_path, &net) != 0) return -1;
            for (int i = 0; i < net.n_banks; ++i) net.ext_assets[i] += spec->bailout_buffer;
            if (econ_clear_net(&net, -1.0, 1, NULL, &cr, NULL) != 0) return -1;
            double tot = fmax(cr.total_obligations, 1.0);
            m_sys = 10000.0 * (0.30 - cr.adv_shock_shortfall / tot);
        } else {
            m_sys = 10000.0 * (av.surviving_mass_frac - 0.15);
        }
        if (m_inc <= 0.0) {
            snprintf(note, sizeof(note), "Adverse selection take-rate cliff unraveling (m_inc=%.0fbps)", m_inc);
        }
    } else if (strcmp(spec->institution_type, "exchange_margin") == 0) {
        m_inc = spec->chain_fee_bps; /* arbitrage/fee incentive margin in bps */
        m_dro = eval_dro_margin_bps(spec);
        econ_margin_mkt_t mkt;
        econ_margin_res_t mr;
        if (econ_parse_margin_market(spec->sys_ref_path, &mkt) != 0) return -1;
        for (int i = 0; i < mkt.n_accts; ++i) mkt.c[i] += spec->bailout_buffer;
        if (econ_margin_audit(&mkt, spec->theta_override, -1.0, NULL, &mr, NULL) != 0) return -1;
        m_sys = 10000.0 * mr.macro_safety_margin;
        if (mr.paradox_detected && m_sys <= 0.0) {
            snprintf(note, sizeof(note), "Micro-Macro margin paradox: hair-trigger theta triggers cascade (bd=%.1f)", mr.bad_debt);
        } else if (m_sys <= 0.0) {
            snprintf(note, sizeof(note), "Liquidation cascade insolvency under stress (m_sys=%.0fbps)", m_sys);
        }
    } else if (strcmp(spec->institution_type, "supply_chain") == 0) {
        econ_supply_chain_t sc;
        econ_supply_res_t sr;
        if (econ_parse_supply_chain(spec->sys_ref_path, &sc) != 0) return -1;
        for (int i = 0; i < sc.n_firms; ++i) sc.ext_assets[i] += spec->bailout_buffer;
        double tau = (spec->tau_override > 0.0) ? spec->tau_override : sc.tau_horizon;
        if (econ_supply_chain_audit(&sc, tau, 1, NULL, &sr, NULL) != 0) return -1;
        /* Suppliers stay in if prompt-pay / margin covers horizon financing burden */
        m_inc = spec->chain_fee_bps - 400.0 * fmax(tau - 1.0, 0.0);
        m_dro = eval_dro_margin_bps(spec);
        m_sys = 10000.0 * sr.systemic_safety_margin;
        if (m_sys <= 0.0) {
            snprintf(note, sizeof(note), "Supply-chain horizon tau=%.2f triggers SEARU contagion (sf=%.1f)", tau, sr.searu_shortfall);
        }
    } else {
        return -1;
    }

    res->m_inc_bps = m_inc;
    res->m_dro_bps = m_dro;
    res->m_sys_bps = m_sys;

    int64_t mi = (int64_t)llround(m_inc);
    int64_t md = (int64_t)llround(m_dro);
    int64_t ms = (int64_t)llround(m_sys);
    int64_t mast = mi;
    if (md < mast) mast = md;
    if (ms < mast) mast = ms;

    res->master_margin_bps = (double)mast;
    res->certified = (mast > 0) ? 1 : 0;
    snprintf(res->verdict, sizeof(res->verdict), "%s", res->certified ? "CERTIFIED" : "VETOED");

    if (mi <= md && mi <= ms) snprintf(res->weakest_dim, sizeof(res->weakest_dim), "INC");
    else if (md <= mi && md <= ms) snprintf(res->weakest_dim, sizeof(res->weakest_dim), "DRO");
    else snprintf(res->weakest_dim, sizeof(res->weakest_dim), "SYS");
    snprintf(res->exploit_note, sizeof(res->exploit_note), "%s", note);

    int64_t tgt = (int64_t)llround(spec->target_margin_bps);
    if (tgt <= 0) tgt = 100;
    int64_t ci = (int64_t)llround(fmax(spec->c_inc, 0.0) * 100.0);
    int64_t cd = (int64_t)llround(fmax(spec->c_dro, 0.0) * 100.0);
    int64_t cs = (int64_t)llround(fmax(spec->c_sys, 0.0) * 100.0);
    int64_t di = (tgt > mi) ? (tgt - mi) : 0;
    int64_t dd = (tgt > md) ? (tgt - md) : 0;
    int64_t ds = (tgt > ms) ? (tgt - ms) : 0;
    __extension__ typedef __int128 i128;
    i128 rcost = (i128)ci * (i128)di + (i128)cd * (i128)dd + (i128)cs * (i128)ds;

    res->d_inc_bps = (double)di;
    res->d_dro_bps = (double)dd;
    res->d_sys_bps = (double)ds;
    res->min_repair_cost = (double)rcost / 100.0;

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "inst");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "inst");
        snprintf(cert_out->status, sizeof(cert_out->status), "%s", res->verdict);
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }
        cert_out->has_inst = 1;
        cert_out->inst_scale = 10000;
        cert_out->inst_m_inc = mi;
        cert_out->inst_m_dro = md;
        cert_out->inst_m_sys = ms;
        cert_out->inst_master = mast;
        cert_out->inst_target = tgt;
        cert_out->inst_c_inc = ci;
        cert_out->inst_c_dro = cd;
        cert_out->inst_c_sys = cs;
        cert_out->inst_d_inc = di;
        cert_out->inst_d_dro = dd;
        cert_out->inst_d_sys = ds;
        cert_out->inst_repair_cost = (int64_t)rcost;
        snprintf(cert_out->inst_weakest, sizeof(cert_out->inst_weakest), "%s", res->weakest_dim);
    }
    return 0;
}
