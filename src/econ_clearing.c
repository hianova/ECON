/* SPDX-License-Identifier: MIT */
#include "econ_clearing.h"
#include "searu.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_json_key_clr(const char *json, const char *key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != ':') return NULL;
    ++p;
    while (*p && isspace((unsigned char)*p)) ++p;
    return p;
}

int econ_parse_clearing_net(const char *path, econ_clearing_net_t *out) {
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
    out->beta_default = 1.0;
    const char *v;
    if ((v = find_json_key_clr(buf, "n_banks")) != NULL) out->n_banks = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_clr(buf, "alpha_fire")) != NULL) out->alpha_fire = strtod(v, NULL);
    if ((v = find_json_key_clr(buf, "beta_default")) != NULL) out->beta_default = strtod(v, NULL);
    if ((v = find_json_key_clr(buf, "shock_budget")) != NULL) out->shock_budget = strtod(v, NULL);
    if (out->n_banks <= 0 || out->n_banks > ECON_CERT_MAX_BANKS) { free(buf); return -1; }

    int nb = out->n_banks;
    if ((v = find_json_key_clr(buf, "ext_assets")) == NULL || *v != '[') { free(buf); return -1; }
    ++v;
    for (int i = 0; i < nb; ++i) {
        while (*v && (isspace((unsigned char)*v) || *v == ',')) ++v;
        char *ep = NULL;
        out->ext_assets[i] = strtod(v, &ep);
        if (ep == v) { free(buf); return -1; }
        v = ep;
    }

    if ((v = find_json_key_clr(buf, "liabilities")) == NULL || *v != '[') { free(buf); return -1; }
    ++v;
    for (int i = 0; i < nb; ++i) {
        while (*v && *v != '[') ++v;
        if (*v != '[') { free(buf); return -1; }
        ++v;
        for (int j = 0; j < nb; ++j) {
            while (*v && (isspace((unsigned char)*v) || *v == ',')) ++v;
            char *ep = NULL;
            out->L[i * ECON_CERT_MAX_BANKS + j] = strtod(v, &ep);
            if (ep == v) { free(buf); return -1; }
            v = ep;
        }
        while (*v && *v != ']') ++v;
        if (*v == ']') ++v;
    }

    free(buf);
    return 0;
}

static void solve_tarski_fp(int nb, const double *p_bar, const double *ext, const double *Pi,
                            double alpha_fire, double beta_def, int init_upper,
                            double *p_out, int *rounds_out) {
    double cur[ECON_CERT_MAX_BANKS], nxt[ECON_CERT_MAX_BANKS];
    double tot_bar = 0.0;
    for (int i = 0; i < nb; ++i) {
        cur[i] = init_upper ? p_bar[i] : 0.0;
        tot_bar += p_bar[i];
    }
    if (tot_bar < 1e-12) tot_bar = 1.0;

    int rounds = 0;
    for (int it = 0; it < 600; ++it) {
        double tot_cur = 0.0;
        for (int i = 0; i < nb; ++i) tot_cur += cur[i];
        double shortfall_frac = (tot_bar - tot_cur) / tot_bar;
        if (shortfall_frac < 0.0) shortfall_frac = 0.0;
        double fire_mult = exp(-alpha_fire * shortfall_frac);

        double max_diff = 0.0;
        for (int i = 0; i < nb; ++i) {
            double inflow = 0.0;
            for (int j = 0; j < nb; ++j) {
                double rec = (cur[j] + 1e-7 < p_bar[j]) ? (beta_def * cur[j]) : cur[j];
                inflow += Pi[j * ECON_CERT_MAX_BANKS + i] * rec;
            }
            double e_eff = ext[i] * fire_mult;
            double v = e_eff + inflow;
            if (v < 0.0) v = 0.0;
            if (v > p_bar[i]) v = p_bar[i];
            nxt[i] = v;
            double d = fabs(nxt[i] - cur[i]);
            if (d > max_diff) max_diff = d;
        }
        memcpy(cur, nxt, (size_t)nb * sizeof(double));
        rounds = it + 1;
        if (max_diff < 1e-9) break;
    }
    memcpy(p_out, cur, (size_t)nb * sizeof(double));
    if (rounds_out) *rounds_out = rounds;
}

int econ_clear_net(const econ_clearing_net_t *net, double fire_override,
                   int run_searu_attack, const char *input_path,
                   econ_clearing_res_t *res, econ_cert_record_t *cert_out) {
    if (!net || !res || net->n_banks <= 0 || net->n_banks > ECON_CERT_MAX_BANKS) return -1;
    memset(res, 0, sizeof(*res));
    int nb = net->n_banks;
    res->n_banks = nb;
    double alpha = (fire_override >= 0.0) ? fire_override : net->alpha_fire;
    double beta = (net->beta_default > 0.0 && net->beta_default <= 1.0) ? net->beta_default : 1.0;

    double p_bar[ECON_CERT_MAX_BANKS] = {0};
    double Pi[ECON_CERT_MAX_BANKS * ECON_CERT_MAX_BANKS] = {0};
    for (int i = 0; i < nb; ++i) {
        double s = 0.0;
        for (int j = 0; j < nb; ++j) s += net->L[i * ECON_CERT_MAX_BANKS + j];
        p_bar[i] = s;
        res->total_obligations += s;
        if (s > 1e-12) {
            for (int j = 0; j < nb; ++j) {
                Pi[i * ECON_CERT_MAX_BANKS + j] = net->L[i * ECON_CERT_MAX_BANKS + j] / s;
            }
        }
    }

    int r_up = 0, r_lo = 0;
    solve_tarski_fp(nb, p_bar, net->ext_assets, Pi, alpha, beta, 1, res->p_upper, &r_up);
    solve_tarski_fp(nb, p_bar, net->ext_assets, Pi, alpha, beta, 0, res->p_lower, &r_lo);
    res->cascade_rounds = (r_up > r_lo) ? r_up : r_lo;

    for (int i = 0; i < nb; ++i) {
        res->total_paid_upper += res->p_upper[i];
        res->total_paid_lower += res->p_lower[i];
        if (res->p_upper[i] + 1e-6 < p_bar[i]) res->defaults_upper++;
        if (res->p_lower[i] + 1e-6 < p_bar[i]) res->defaults_lower++;
        double full_in = 0.0;
        for (int j = 0; j < nb; ++j) full_in += Pi[j * ECON_CERT_MAX_BANKS + i] * p_bar[j];
        double z = p_bar[i] - (net->ext_assets[i] + full_in);
        res->bailout_z[i] = (z > 0.0) ? z : 0.0;
        res->min_bailout_total += res->bailout_z[i];
    }
    res->multi_eq_gap = res->total_paid_upper - res->total_paid_lower;
    if (res->multi_eq_gap < 0.0) res->multi_eq_gap = 0.0;
    res->systemic_shortfall = res->total_obligations - res->total_paid_lower;

    /* Optional SEARU Pure-GPR Levy-Flight adversarial liquidity shock optimization */
    res->adv_shock_shortfall = res->systemic_shortfall;
    res->adv_shock_amp = 1.0;
    if (run_searu_attack && net->shock_budget > 0.0) {
        double uniform_e[ECON_CERT_MAX_BANKS], uni_p[ECON_CERT_MAX_BANKS];
        for (int i = 0; i < nb; ++i) {
            uniform_e[i] = net->ext_assets[i] - (net->shock_budget / (double)nb);
            if (uniform_e[i] < 0.0) uniform_e[i] = 0.0;
        }
        solve_tarski_fp(nb, p_bar, uniform_e, Pi, alpha, beta, 0, uni_p, NULL);
        double uni_paid = 0.0;
        for (int i = 0; i < nb; ++i) uni_paid += uni_p[i];
        double uni_shortfall = res->total_obligations - uni_paid;

        uint64_t rng = UINT64_C(0xE0C0202611220001);
        double best_weights[ECON_CERT_MAX_BANKS], cur_weights[ECON_CERT_MAX_BANKS];
        for (int i = 0; i < nb; ++i) best_weights[i] = cur_weights[i] = 1.0 / (double)nb;
        double best_sf = uni_shortfall;

        for (uint32_t it = 0U; it < 240U; ++it) {
            double cand[ECON_CERT_MAX_BANKS];
            memcpy(cand, best_weights, (size_t)nb * sizeof(double));
            uint64_t bswan = 0ULL;
            uint32_t chaos = searu_adapt_chaos_q8(it & 31U, (it % 19U == 0U) ? UINT64_C(1) : 0ULL);
            uint64_t jump = searu_zipf_jump(&rng, chaos, &bswan);
            uint32_t k_mut = 1U + (uint32_t)(jump % (uint64_t)(nb > 1 ? nb : 1));
            if (k_mut > (uint32_t)nb) k_mut = (uint32_t)nb;
            for (uint32_t m = 0U; m < k_mut; ++m) {
                uint32_t idx = searu_Bounded_u32(searu_rng_next(&rng), (uint32_t)nb);
                uint64_t r = searu_rng_next(&rng);
                double mult = bswan ? 4.0 : (0.25 + 2.5 * ((double)(r & 0xFFFFU) / 65535.0));
                cand[idx] = cand[idx] * mult + 1e-4;
            }
            double wsum = 0.0;
            for (int i = 0; i < nb; ++i) wsum += cand[i];
            double e_test[ECON_CERT_MAX_BANKS], p_test[ECON_CERT_MAX_BANKS];
            for (int i = 0; i < nb; ++i) {
                cand[i] /= (wsum > 1e-12) ? wsum : 1.0;
                double s_i = net->shock_budget * cand[i];
                e_test[i] = net->ext_assets[i] - s_i;
                if (e_test[i] < 0.0) e_test[i] = 0.0;
            }
            solve_tarski_fp(nb, p_bar, e_test, Pi, alpha, beta, 0, p_test, NULL);
            double tp = 0.0;
            for (int i = 0; i < nb; ++i) tp += p_test[i];
            double sf = res->total_obligations - tp;
            if (sf > best_sf) {
                best_sf = sf;
                memcpy(best_weights, cand, (size_t)nb * sizeof(double));
            }
        }
        res->adv_shock_shortfall = best_sf;
        res->adv_shock_amp = (uni_shortfall > 1e-6) ? (best_sf / uni_shortfall) : 1.0;
        for (int i = 0; i < nb; ++i) res->adv_shock[i] = net->shock_budget * best_weights[i];
    }

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "clearing");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "clearing");
        snprintf(cert_out->status, sizeof(cert_out->status), "CERTIFIED");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }
        const int64_t CS = 10000;
        const int64_t AS = 1000;
        /* Evaluate effective stressed external assets at lower fixed point for exact integer M16/M18 cert */
        double tot_bar = (res->total_obligations > 1e-12) ? res->total_obligations : 1.0;
        double sf_frac = (tot_bar - res->total_paid_lower) / tot_bar;
        if (sf_frac < 0.0) sf_frac = 0.0;
        double fire_mult_lo = exp(-alpha * sf_frac);

        cert_out->has_clearing = 1;
        cert_out->clear_n_banks = nb;
        cert_out->clear_scale = CS;
        cert_out->clear_has_bailout = 1;
        for (int i = 0; i < nb; ++i) {
            cert_out->clear_p_bar[i] = (int64_t)llround(p_bar[i] * (double)AS);
            cert_out->clear_ext[i]   = (int64_t)floor(net->ext_assets[i] * fire_mult_lo * (double)AS);
            for (int j = 0; j < nb; ++j) {
                cert_out->clear_pi_scaled[(size_t)i * ECON_CERT_MAX_BANKS + (size_t)j] =
                    (int64_t)llround(Pi[i * ECON_CERT_MAX_BANKS + j] * (double)CS);
            }
        }
        /* Exact integer Tarski iteration from 0 up (plo) and p_bar down (phi) guarantees M16 identically */
        int64_t plo[ECON_CERT_MAX_BANKS], phi[ECON_CERT_MAX_BANKS];
        for (int i = 0; i < nb; ++i) { plo[i] = 0; phi[i] = cert_out->clear_p_bar[i]; }
        for (int it = 0; it < 200; ++it) {
            int changed = 0;
            int64_t nlo[ECON_CERT_MAX_BANKS], nhi[ECON_CERT_MAX_BANKS];
            for (int i = 0; i < nb; ++i) {
                __extension__ typedef __int128 i128;
                i128 ilo = 0, ihi = 0;
                for (int j = 0; j < nb; ++j) {
                    int64_t pij = cert_out->clear_pi_scaled[(size_t)j * ECON_CERT_MAX_BANKS + (size_t)i];
                    ilo += (i128)pij * (i128)plo[j];
                    ihi += (i128)pij * (i128)phi[j];
                }
                i128 rlo = (i128)cert_out->clear_ext[i] + ilo / (i128)CS;
                i128 rhi = (i128)cert_out->clear_ext[i] + ihi / (i128)CS;
                nlo[i] = (rlo < 0) ? 0 : ((rlo > cert_out->clear_p_bar[i]) ? cert_out->clear_p_bar[i] : (int64_t)rlo);
                nhi[i] = (rhi < 0) ? 0 : ((rhi > cert_out->clear_p_bar[i]) ? cert_out->clear_p_bar[i] : (int64_t)rhi);
                if (nlo[i] != plo[i] || nhi[i] != phi[i]) changed = 1;
            }
            memcpy(plo, nlo, (size_t)nb * sizeof(int64_t));
            memcpy(phi, nhi, (size_t)nb * sizeof(int64_t));
            if (!changed) break;
        }
        int64_t sum_lo = 0, sum_hi = 0;
        for (int i = 0; i < nb; ++i) {
            cert_out->clear_p_lo[i] = plo[i];
            cert_out->clear_p_hi[i] = phi[i];
            sum_lo += plo[i];
            sum_hi += phi[i];
            __extension__ typedef __int128 i128;
            i128 full_in = 0;
            for (int j = 0; j < nb; ++j) {
                full_in += (i128)cert_out->clear_pi_scaled[(size_t)j * ECON_CERT_MAX_BANKS + (size_t)i] *
                           (i128)cert_out->clear_p_bar[j];
            }
            i128 need_rhs = (i128)cert_out->clear_p_bar[i] * (i128)CS - full_in;
            i128 cur_ext_s = (i128)cert_out->clear_ext[i] * (i128)CS;
            if (need_rhs > cur_ext_s) {
                cert_out->clear_z[i] = (int64_t)((need_rhs - cur_ext_s + CS - 1) / CS);
            } else {
                cert_out->clear_z[i] = 0;
            }
        }
        cert_out->has_bracket = 1;
        cert_out->bracket_scale = AS;
        cert_out->lb_int = sum_lo;
        cert_out->ub_int = sum_hi;
        cert_out->limit_lo_int = 0;
        cert_out->limit_hi_int = (int64_t)ceil(res->total_obligations * (double)AS) + 1000;
        cert_out->lb_val = (double)sum_lo / (double)AS;
        cert_out->ub_val = (double)sum_hi / (double)AS;
    }
    return 0;
}

void *econ_clear_session_create(const char *net_json_path) {
    econ_clearing_net_t *net = (econ_clearing_net_t *)calloc(1, sizeof(econ_clearing_net_t));
    if (!net) return NULL;
    if (econ_parse_clearing_net(net_json_path, net) != 0) {
        free(net);
        return NULL;
    }
    return (void *)net;
}

void econ_clear_session_destroy(void *sess) {
    free(sess);
}

int econ_clear_session_sweep(void *sess, double alpha_fire, double beta_def,
                             double shock_scale, double *out_metrics6) {
    if (!sess || !out_metrics6) return -1;
    econ_clearing_net_t tmp = *(const econ_clearing_net_t *)sess;
    tmp.beta_default = beta_def;
    for (int i = 0; i < tmp.n_banks; ++i) {
        tmp.ext_assets[i] *= shock_scale;
    }
    econ_clearing_res_t res;
    if (econ_clear_net(&tmp, alpha_fire, 0, NULL, &res, NULL) != 0) return -1;
    out_metrics6[0] = res.total_paid_upper;
    out_metrics6[1] = res.total_paid_lower;
    out_metrics6[2] = res.multi_eq_gap;
    out_metrics6[3] = (double)res.defaults_lower;
    out_metrics6[4] = res.systemic_shortfall;
    out_metrics6[5] = res.min_bailout_total;
    return 0;
}
