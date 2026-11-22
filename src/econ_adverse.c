/* SPDX-License-Identifier: MIT */
#include "econ_adverse.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_json_key_adv(const char *json, const char *key) {
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

static int parse_d_array_adv(const char *json, const char *key, int n, double *out) {
    const char *v = find_json_key_adv(json, key);
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

int econ_parse_adverse_spec(const char *path, econ_adverse_spec_t *out) {
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
    if ((v = find_json_key_adv(buf, "n_types")) != NULL) out->n_types = (int)strtol(v, NULL, 10);
    if ((v = find_json_key_adv(buf, "take_rate")) != NULL) out->take_rate = strtod(v, NULL);
    if (out->n_types <= 0 || out->n_types > ECON_CERT_MAX_TYPES) { free(buf); return -1; }
    if (parse_d_array_adv(buf, "weights", out->n_types, out->weights) != 0 ||
        parse_d_array_adv(buf, "values", out->n_types, out->values) != 0 ||
        parse_d_array_adv(buf, "costs", out->n_types, out->costs) != 0) {
        free(buf);
        return -1;
    }

    if ((v = find_json_key_adv(buf, "has_screening")) != NULL && strtol(v, NULL, 10) > 0) {
        out->has_screening = 1;
        if ((v = find_json_key_adv(buf, "qH")) != NULL) out->qH = strtod(v, NULL);
        if ((v = find_json_key_adv(buf, "qL")) != NULL) out->qL = strtod(v, NULL);
        if ((v = find_json_key_adv(buf, "cH")) != NULL) out->cH = strtod(v, NULL);
        if ((v = find_json_key_adv(buf, "cL")) != NULL) out->cL = strtod(v, NULL);
        if ((v = find_json_key_adv(buf, "PH")) != NULL) out->PH = strtod(v, NULL);
        if ((v = find_json_key_adv(buf, "PL")) != NULL) out->PL = strtod(v, NULL);
        if ((v = find_json_key_adv(buf, "BH")) != NULL) out->BH = strtod(v, NULL);
        if ((v = find_json_key_adv(buf, "BL")) != NULL) out->BL = strtod(v, NULL);
    }
    free(buf);

    /* Ensure nondecreasing quality order */
    for (int i = 0; i < out->n_types; ++i) {
        for (int j = i + 1; j < out->n_types; ++j) {
            if (out->values[j] < out->values[i]) {
                double tw = out->weights[i], tv = out->values[i], tc = out->costs[i];
                out->weights[i] = out->weights[j]; out->values[i] = out->values[j]; out->costs[i] = out->costs[j];
                out->weights[j] = tw; out->values[j] = tv; out->costs[j] = tc;
            }
        }
    }
    return 0;
}

int econ_adverse_audit(const econ_adverse_spec_t *spec, double take_rate_override,
                       const char *input_path, econ_adverse_res_t *res,
                       econ_cert_record_t *cert_out) {
    if (!spec || !res || spec->n_types <= 0 || spec->n_types > ECON_CERT_MAX_TYPES) return -1;
    memset(res, 0, sizeof(*res));
    int N = spec->n_types;
    double r = (take_rate_override >= 0.0) ? take_rate_override : spec->take_rate;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;

    res->n_types = N;
    res->take_rate = r;

    double tot_w = 0.0;
    for (int i = 0; i < N; ++i) tot_w += spec->weights[i];
    if (tot_w <= 1e-12) return -1;

    /* Exact closed-form critical take-rate across all nonempty prefix equilibria 1..N */
    double r_star = 0.0;
    for (int k = 1; k <= N; ++k) {
        double m = 0.0, vsum = 0.0, cmax = 0.0;
        for (int i = 0; i < k; ++i) {
            m += spec->weights[i];
            vsum += spec->weights[i] * spec->values[i];
            if (spec->costs[i] > cmax) cmax = spec->costs[i];
        }
        double avg_v = (m > 1e-12) ? (vsum / m) : 0.0;
        double rk = (avg_v > 1e-12) ? (1.0 - cmax / avg_v) : 0.0;
        if (rk > r_star) r_star = rk;
    }
    if (r_star < 0.0) r_star = 0.0;
    if (r_star > 1.0) r_star = 1.0;

    /* Discrete Tarski prefix unraveling under take rate r */
    int k = N, rounds = 0;
    while (k > 0) {
        double m = 0.0, vsum = 0.0;
        for (int i = 0; i < k; ++i) {
            m += spec->weights[i];
            vsum += spec->weights[i] * spec->values[i];
        }
        double pool_p = (m > 1e-12) ? (vsum / m) : 0.0;
        double seller_net = (1.0 - r) * pool_p;
        int next_k = k;
        while (next_k > 0 && spec->costs[next_k - 1] > seller_net + 1e-12) {
            --next_k;
        }
        if (next_k == k) break;
        k = next_k;
        ++rounds;
    }

    double eq_m = 0.0, eq_v = 0.0, eq_c = 0.0;
    for (int i = 0; i < k; ++i) {
        eq_m += spec->weights[i];
        eq_v += spec->weights[i] * spec->values[i];
        eq_c += spec->weights[i] * spec->costs[i];
    }
    res->k_eq = k;
    res->unravel_rounds = rounds;
    res->critical_r_star = r_star;
    res->safety_margin_r = r_star - r;
    res->surviving_mass_frac = eq_m / tot_w;
    res->eq_pool_price = (eq_m > 1e-12) ? (eq_v / eq_m) : 0.0;
    res->platform_revenue = r * eq_v;
    res->total_welfare = eq_v - eq_c;
    res->full_collapse = (k == 0) ? 1 : 0;

    res->screened_r_star = r_star;
    if (spec->has_screening) {
        double uHH = (spec->PH - spec->cH) - spec->qH * spec->BH;
        double uHL = (spec->PL - spec->cH) - spec->qH * spec->BL;
        double uLL = (spec->PL - spec->cL) - spec->qL * spec->BL;
        double uLH = (spec->PH - spec->cL) - spec->qL * spec->BH;
        if (spec->qH >= 0.0 && spec->qH < spec->qL &&
            uHH >= -1e-9 && uLL >= -1e-9 && uHL <= uHH + 1e-9 && uLH <= uLL + 1e-9 &&
            spec->BL <= spec->BH + 1e-9 && spec->PL <= spec->PH + 1e-9) {
            res->screening_valid = 1;
            /* Under separating screening bond, highest separated type earns PH directly */
            double best_v = spec->values[N - 1], best_c = spec->costs[N - 1];
            double sep_r_star = (best_v > 1e-12) ? (1.0 - best_c / best_v) : r_star;
            if (sep_r_star > res->screened_r_star) res->screened_r_star = sep_r_star;
            if (res->screened_r_star > 1.0) res->screened_r_star = 1.0;
        }
    }

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "adverse");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "adverse");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }

        const int64_t RSCALE = 10000;
        const int64_t VSCALE = 1000;
        const int64_t WSCALE = 1000;
        cert_out->has_adverse = 1;
        cert_out->adv_n_types = N;
        cert_out->adv_scale = RSCALE;
        cert_out->adv_r = (int64_t)llround(r * (double)RSCALE);
        for (int i = 0; i < N; ++i) {
            cert_out->adv_w[i] = (int64_t)llround(spec->weights[i] * (double)WSCALE);
            if (cert_out->adv_w[i] <= 0) cert_out->adv_w[i] = 1;
            cert_out->adv_v[i] = (int64_t)llround(spec->values[i] * (double)VSCALE);
            cert_out->adv_c[i] = (int64_t)llround(spec->costs[i] * (double)VSCALE);
        }

        /* Integer prefix Tarski unraveling guaranteed identical to M22/M23 integer checks */
        __extension__ typedef __int128 i128;
        int k_int = N;
        while (k_int > 0) {
            i128 mass = 0, val = 0;
            for (int i = 0; i < k_int; ++i) {
                mass += (i128)cert_out->adv_w[i];
                val  += (i128)cert_out->adv_w[i] * (i128)cert_out->adv_v[i];
            }
            int nk = k_int;
            while (nk > 0 &&
                   (i128)cert_out->adv_c[nk - 1] * ((i128)RSCALE * mass) >
                   (i128)(RSCALE - cert_out->adv_r) * val) {
                --nk;
            }
            if (nk == k_int) break;
            k_int = nk;
        }
        cert_out->adv_k_eq = k_int;

        if (k_int > 0) {
            i128 mass = 0, val = 0;
            for (int i = 0; i < k_int; ++i) {
                mass += (i128)cert_out->adv_w[i];
                val  += (i128)cert_out->adv_w[i] * (i128)cert_out->adv_v[i];
            }
            int64_t exact_rstar = RSCALE;
            for (int i = 0; i < k_int; ++i) {
                i128 num = (i128)cert_out->adv_c[i] * ((i128)RSCALE * mass);
                i128 ceil_div = (val > 0) ? ((num + val - 1) / val) : RSCALE;
                int64_t ri = RSCALE - (int64_t)ceil_div;
                if (ri < exact_rstar) exact_rstar = ri;
            }
            if (exact_rstar < cert_out->adv_r) exact_rstar = cert_out->adv_r;
            cert_out->adv_r_star = exact_rstar;
            snprintf(cert_out->status, sizeof(cert_out->status), "CERTIFIED");
        } else {
            cert_out->adv_r_star = 0;
            snprintf(cert_out->status, sizeof(cert_out->status), "OBSTRUCTION");
        }

        if (res->screening_valid) {
            const int64_t SS = 1000;
            cert_out->adv_has_screen = 1;
            cert_out->adv_scr_scale = SS;
            cert_out->adv_qH = (int64_t)llround(spec->qH * (double)SS);
            cert_out->adv_qL = (int64_t)llround(spec->qL * (double)SS);
            cert_out->adv_cH = (int64_t)llround(spec->cH * (double)SS);
            cert_out->adv_cL = (int64_t)llround(spec->cL * (double)SS);
            cert_out->adv_PH = (int64_t)llround(spec->PH * (double)SS);
            cert_out->adv_PL = (int64_t)llround(spec->PL * (double)SS);
            cert_out->adv_BH = (int64_t)llround(spec->BH * (double)SS);
            cert_out->adv_BL = (int64_t)llround(spec->BL * (double)SS);
        }
    }
    return 0;
}

int econ_fast_adverse_eval(const char *path, double take_rate, double *out6) {
    if (!path || !out6) return -1;
    econ_adverse_spec_t sp;
    if (econ_parse_adverse_spec(path, &sp) != 0) return -1;
    econ_adverse_res_t res;
    if (econ_adverse_audit(&sp, take_rate, NULL, &res, NULL) != 0) return -1;
    out6[0] = (double)res.k_eq;
    out6[1] = res.surviving_mass_frac;
    out6[2] = res.eq_pool_price;
    out6[3] = res.platform_revenue;
    out6[4] = res.critical_r_star;
    out6[5] = res.safety_margin_r;
    return 0;
}
