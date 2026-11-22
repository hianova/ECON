/* SPDX-License-Identifier: MIT */
#include "econ_agency.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_json_key_ag(const char *json, const char *key) {
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

static int parse_d_array_ag(const char *json, const char *key, int n, double *out) {
    const char *v = find_json_key_ag(json, key);
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

int econ_parse_agency_spec(const char *path, econ_agency_spec_t *out) {
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
    if ((v = find_json_key_ag(buf, "n_states")) != NULL) out->n_states = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_ag(buf, "alpha")) != NULL) out->alpha = strtod(v, NULL);
    if ((v = find_json_key_ag(buf, "beta")) != NULL) out->beta = strtod(v, NULL);
    if ((v = find_json_key_ag(buf, "strike_K")) != NULL) out->strike_K = strtod(v, NULL);
    if ((v = find_json_key_ag(buf, "clawback_D")) != NULL) out->clawback_D = strtod(v, NULL);
    if ((v = find_json_key_ag(buf, "c_safe")) != NULL) out->c_safe = strtod(v, NULL);
    if ((v = find_json_key_ag(buf, "c_risk")) != NULL) out->c_risk = strtod(v, NULL);
    if ((v = find_json_key_ag(buf, "u_reserve")) != NULL) out->u_reserve = strtod(v, NULL);

    if (out->n_states <= 0 || out->n_states > ECON_CERT_MAX_STATES) { free(buf); return -1; }
    if (parse_d_array_ag(buf, "returns", out->n_states, out->returns) != 0 ||
        parse_d_array_ag(buf, "p_safe", out->n_states, out->p_safe) != 0 ||
        parse_d_array_ag(buf, "p_risk", out->n_states, out->p_risk) != 0) {
        free(buf);
        return -1;
    }
    free(buf);
    return 0;
}

static double pos_d(double x) { return (x > 0.0) ? x : 0.0; }

int econ_agency_audit(const econ_agency_spec_t *spec, double beta_override,
                      const char *input_path, econ_agency_res_t *res,
                      econ_cert_record_t *cert_out) {
    if (!spec || !res || spec->n_states <= 0 || spec->n_states > ECON_CERT_MAX_STATES) return -1;
    memset(res, 0, sizeof(*res));
    int ns = spec->n_states;
    double alpha = spec->alpha;
    double beta = (beta_override >= 0.0) ? beta_override : spec->beta;
    res->n_states = ns;
    res->alpha = alpha;
    res->beta = beta;

    double ps_norm = 0.0, pr_norm = 0.0;
    for (int s = 0; s < ns; ++s) {
        ps_norm += spec->p_safe[s];
        pr_norm += spec->p_risk[s];
    }
    if (ps_norm <= 1e-12 || pr_norm <= 1e-12) return -1;

    double up_safe = 0.0, up_risk = 0.0, dn_safe = 0.0, dn_risk = 0.0;
    double ev_safe = 0.0, ev_risk = 0.0;
    double l_safe_mu = 0.0, l_risk_mu = 0.0;
    for (int s = 0; s < ns; ++s) {
        double ps = spec->p_safe[s] / ps_norm;
        double pr = spec->p_risk[s] / pr_norm;
        double r = spec->returns[s];
        double up = pos_d(r - spec->strike_K);
        double dn = pos_d(spec->clawback_D - r);
        up_safe += ps * up;
        up_risk += pr * up;
        dn_safe += ps * dn;
        dn_risk += pr * dn;
        ev_safe += ps * r;
        ev_risk += pr * r;
        l_safe_mu += ps * (-r);
        l_risk_mu += pr * (-r);
    }

    double var_safe = 0.0, var_risk = 0.0;
    for (int s = 0; s < ns; ++s) {
        double ps = spec->p_safe[s] / ps_norm;
        double pr = spec->p_risk[s] / pr_norm;
        double ls = -spec->returns[s] - l_safe_mu;
        double lr = -spec->returns[s] - l_risk_mu;
        var_safe += ps * ls * ls;
        var_risk += pr * lr * lr;
    }

    res->u_safe = alpha * up_safe - beta * dn_safe - spec->c_safe;
    res->u_risk = alpha * up_risk - beta * dn_risk - spec->c_risk;
    res->ic_margin = res->u_safe - res->u_risk;
    res->ic_satisfied = (res->ic_margin > 1e-9) ? 1 : 0;
    res->sys_ev_safe = ev_safe;
    res->sys_ev_risk = ev_risk;
    res->sys_ev_gap = ev_safe - ev_risk;

    double upside_gap = alpha * (up_safe - up_risk) - (spec->c_safe - spec->c_risk);
    double tail_gap = dn_risk - dn_safe;
    if (upside_gap > 1e-12) {
        res->beta_star = 0.0;
    } else if (tail_gap > 1e-12) {
        res->beta_star = (-upside_gap) / tail_gap;
    } else {
        res->beta_star = 1e6;
    }

    double d_safe = spec->u_reserve - l_safe_mu;
    double d_risk = spec->u_reserve - l_risk_mu;
    res->mu_loss_safe = l_safe_mu;
    res->std_loss_safe = sqrt(var_safe > 0.0 ? var_safe : 0.0);
    res->mu_loss_risk = l_risk_mu;
    res->std_loss_risk = sqrt(var_risk > 0.0 ? var_risk : 0.0);
    res->cantelli_ruin_safe = (d_safe > 1e-12) ? (var_safe / (var_safe + d_safe * d_safe)) : 1.0;
    res->cantelli_ruin_risk = (d_risk > 1e-12) ? (var_risk / (var_risk + d_risk * d_risk)) : 1.0;
    res->cantelli_amp_ratio = res->cantelli_ruin_risk / fmax(res->cantelli_ruin_safe, 1e-12);

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "agency");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "agency");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }

        const int64_t PS = 1000;
        const int64_t AS = 1000;
        const int64_t VS = 100;
        cert_out->has_agency = 1;
        cert_out->ag_n_states = ns;
        cert_out->ag_alpha = (int64_t)llround(alpha * (double)AS);
        cert_out->ag_beta  = (int64_t)llround(beta * (double)AS);
        cert_out->ag_K     = (int64_t)llround(spec->strike_K * (double)VS);
        cert_out->ag_D     = (int64_t)llround(spec->clawback_D * (double)VS);
        cert_out->ag_c_safe = (int64_t)llround(spec->c_safe * (double)PS * (double)AS * (double)VS);
        cert_out->ag_c_risk = (int64_t)llround(spec->c_risk * (double)PS * (double)AS * (double)VS);

        __extension__ typedef __int128 i128;
        i128 u_s = -(i128)cert_out->ag_c_safe;
        i128 u_r = -(i128)cert_out->ag_c_risk;
        for (int s = 0; s < ns; ++s) {
            cert_out->ag_p_safe[s] = (int64_t)llround((spec->p_safe[s] / ps_norm) * (double)PS);
            cert_out->ag_p_risk[s] = (int64_t)llround((spec->p_risk[s] / pr_norm) * (double)PS);
            cert_out->ag_R[s]      = (int64_t)llround(spec->returns[s] * (double)VS);
            i128 d_up = (cert_out->ag_R[s] > cert_out->ag_K) ? (i128)(cert_out->ag_R[s] - cert_out->ag_K) : 0;
            i128 d_dn = (cert_out->ag_D > cert_out->ag_R[s]) ? (i128)(cert_out->ag_D - cert_out->ag_R[s]) : 0;
            i128 pay = (i128)cert_out->ag_alpha * d_up - (i128)cert_out->ag_beta * d_dn;
            u_s += (i128)cert_out->ag_p_safe[s] * pay;
            u_r += (i128)cert_out->ag_p_risk[s] * pay;
        }
        cert_out->ag_ic_margin = (int64_t)(u_s - u_r);
        snprintf(cert_out->status, sizeof(cert_out->status), "%s",
                 (cert_out->ag_ic_margin > 0) ? "CERTIFIED" : "VETOED");

        int64_t W_i = 1000;
        int64_t d_s_i = (int64_t)llround(d_safe * 100.0);
        int64_t d_r_i = (int64_t)llround(d_risk * 100.0);
        int64_t s2_s_i = (int64_t)llround(var_safe * 10000.0);
        int64_t s2_r_i = (int64_t)llround(var_risk * 10000.0);
        if (d_r_i > 0 && d_r_i <= d_s_i && s2_s_i >= 0 && s2_s_i <= s2_r_i) {
            cert_out->ag_has_cantelli_amp = 1;
            cert_out->ag_W = W_i;
            cert_out->ag_delta_safe = d_s_i;
            cert_out->ag_delta_risk = d_r_i;
            cert_out->ag_S2_safe = s2_s_i;
            cert_out->ag_S2_risk = s2_r_i;
        }
    }
    return 0;
}

int econ_fast_agency_eval(const char *path, double alpha, double beta, double *out6) {
    if (!path || !out6) return -1;
    econ_agency_spec_t sp;
    if (econ_parse_agency_spec(path, &sp) != 0) return -1;
    sp.alpha = alpha;
    econ_agency_res_t res;
    if (econ_agency_audit(&sp, beta, NULL, &res, NULL) != 0) return -1;
    out6[0] = res.ic_margin;
    out6[1] = res.beta_star;
    out6[2] = res.sys_ev_gap;
    out6[3] = res.cantelli_ruin_safe;
    out6[4] = res.cantelli_ruin_risk;
    out6[5] = res.cantelli_amp_ratio;
    return 0;
}
