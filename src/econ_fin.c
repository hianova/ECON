/* SPDX-License-Identifier: MIT */
#include "econ_fin.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__extension__ typedef __int128 econ_i128;

static double norm_cdf(double x) {
    return 0.5 * erfc(-x * 0.70710678118654752440);
}

static double bs_call_price(double S, double K, double r, double sigma, double T) {
    if (T <= 1e-12 || sigma <= 1e-12) {
        double v = S - K * exp(-r * T);
        return (v > 0.0) ? v : 0.0;
    }
    double vs = sigma * sqrt(T);
    double d1 = (log(S / K) + (r + 0.5 * sigma * sigma) * T) / vs;
    double d2 = d1 - vs;
    return S * norm_cdf(d1) - K * exp(-r * T) * norm_cdf(d2);
}

int econ_parse_option_chain(const char *path, econ_chain_t *out) {
    if (!path || !out) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    memset(out, 0, sizeof(*out));
    out->spot = 22500.0;
    out->r_rate = 0.015;
    out->ttm = 30.0 / 365.0;
    out->tax_div = 50000.0;
    out->fee = 1.5;

    char line[512];
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        if (line[0] == '#') {
            char *p;
            if ((p = strstr(line, "spot=")) != NULL) out->spot = strtod(p + 5, NULL);
            if ((p = strstr(line, "r=")) != NULL) out->r_rate = strtod(p + 2, NULL);
            if ((p = strstr(line, "ttm=")) != NULL) out->ttm = strtod(p + 4, NULL);
            if ((p = strstr(line, "tax_div=")) != NULL) out->tax_div = strtod(p + 8, NULL);
            if ((p = strstr(line, "fee=")) != NULL) out->fee = strtod(p + 4, NULL);
            if ((p = strstr(line, "fut_bid=")) != NULL) out->fut_bid = strtod(p + 8, NULL);
            if ((p = strstr(line, "fut_ask=")) != NULL) out->fut_ask = strtod(p + 8, NULL);
            continue;
        }
        if (strncmp(line, "strike", 6) == 0) continue;
        double k, cb, ca, pb, pa, fb = out->fut_bid, fa = out->fut_ask;
        int nc = sscanf(line, " %lf , %lf , %lf , %lf , %lf , %lf , %lf",
                        &k, &cb, &ca, &pb, &pa, &fb, &fa);
        if (nc >= 5 && out->n_strikes < ECON_MAX_STRIKES) {
            econ_option_quote_t *q = &out->quotes[out->n_strikes++];
            q->strike = k; q->c_bid = cb; q->c_ask = ca;
            q->p_bid = pb; q->p_ask = pa;
            q->fut_bid = (nc >= 6) ? fb : out->fut_bid;
            q->fut_ask = (nc >= 7) ? fa : out->fut_ask;
            if (out->fut_bid <= 0.0) out->fut_bid = q->fut_bid;
            if (out->fut_ask <= 0.0) out->fut_ask = q->fut_ask;
        }
    }
    fclose(fp);
    return (out->n_strikes > 0) ? 0 : -1;
}

int econ_arb_scan(const econ_chain_t *ch, const char *input_path,
                  econ_arb_res_t *res, econ_cert_record_t *cert_out) {
    if (!ch || !res || ch->n_strikes <= 0) return -1;
    memset(res, 0, sizeof(*res));
    res->n_strikes = ch->n_strikes;

    const double sniper_thresh = 2.0; /* SNIPER Econ/Sniper.lean bEnter = 2.0 */
    int best_idx = 0;
    int best_i = 0, best_j = 1, best_k = 2;
    double best_cost = 0.0;
    double best_mid_res = 0.0;

    int has_any_butterfly_per_strike[ECON_MAX_STRIKES];
    memset(has_any_butterfly_per_strike, 0, sizeof(has_any_butterfly_per_strike));

    /* Check pairwise vertical call spread monotonicity & triples for butterfly convexity (M10) */
    for (int i = 0; i < ch->n_strikes; ++i) {
        if (i + 1 < ch->n_strikes && ch->quotes[i].strike < ch->quotes[i + 1].strike) {
            if (ch->quotes[i + 1].c_bid - ch->quotes[i].c_ask - ch->fee > 1e-6) {
                res->econ_vertical_count++;
            }
        }
        for (int j = i + 1; j < ch->n_strikes; ++j) {
            for (int k = j + 1; k < ch->n_strikes; ++k) {
                double K1 = ch->quotes[i].strike;
                double K2 = ch->quotes[j].strike;
                double K3 = ch->quotes[k].strike;
                if (!(K1 < K2 && K2 < K3)) continue;
                double span = K3 - K1;
                double w1 = K3 - K2;
                double w3 = K2 - K1;
                double bfly_cost = ch->fee * 2.0 * span;
                double bfly_credit = span * ch->quotes[j].c_bid - w1 * ch->quotes[i].c_ask -
                                     w3 * ch->quotes[k].c_ask - bfly_cost;
                double edge_unit = bfly_credit / span;
                if (edge_unit > 1e-6) {
                    res->econ_butterfly_count++;
                    has_any_butterfly_per_strike[j] = 1;
                    if (!res->arb_found || edge_unit > res->best_edge) {
                        res->arb_found = 1;
                        res->best_mode = 3;
                        res->best_edge = edge_unit;
                        res->best_k1 = K1; res->best_k2 = K2; res->best_k3 = K3;
                        best_i = i; best_j = j; best_k = k;
                        best_cost = bfly_cost;
                    }
                }
            }
        }
    }

    for (int i = 0; i < ch->n_strikes; ++i) {
        const econ_option_quote_t *q = &ch->quotes[i];
        double f_mid = 0.5 * (q->fut_bid + q->fut_ask);
        double tax = (ch->tax_div > 0.0) ? (f_mid / ch->tax_div) : 0.0;
        double cost = ch->fee + tax;

        double c_mid = 0.5 * (q->c_bid + q->c_ask);
        double p_mid = 0.5 * (q->p_bid + q->p_ask);
        double syn_mid = q->strike + c_mid - p_mid;
        double mid_res = syn_mid - f_mid;
        if (fabs(mid_res) > res->max_sniper_mid_abs) {
            res->max_sniper_mid_abs = fabs(mid_res);
            if (!res->arb_found) {
                best_idx = i;
                best_mid_res = mid_res;
            }
        }

        double syn_ask = q->strike + q->c_ask - q->p_bid;
        double syn_bid = q->strike + q->c_bid - q->p_ask;
        double conv_edge = q->fut_bid - syn_ask - cost;
        double rev_edge  = syn_bid - q->fut_ask - cost;

        int sniper_hit = (fabs(mid_res) >= sniper_thresh);
        int conv_hit = (conv_edge > 1e-6);
        int rev_hit  = (rev_edge > 1e-6);
        if (sniper_hit) res->sniper_flags++;
        if (conv_hit) res->econ_conv_count++;
        if (rev_hit) res->econ_rev_count++;
        if (sniper_hit && !conv_hit && !rev_hit) res->sniper_false_pos++;
        if (!sniper_hit && has_any_butterfly_per_strike[i]) res->sniper_false_neg++;

        if (conv_hit && (!res->arb_found || res->best_mode != 3) &&
            (!res->arb_found || conv_edge > res->best_edge)) {
            res->arb_found = 1;
            res->best_mode = 1;
            res->best_edge = conv_edge;
            res->best_k1 = q->strike;
            best_idx = i; best_cost = cost; best_mid_res = mid_res;
        }
        if (rev_hit && (!res->arb_found || res->best_mode != 3) &&
            (!res->arb_found || rev_edge > res->best_edge)) {
            res->arb_found = 1;
            res->best_mode = 2;
            res->best_edge = rev_edge;
            res->best_k1 = q->strike;
            best_idx = i; best_cost = cost; best_mid_res = mid_res;
        }
    }

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "fin");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "arb_scan");
        snprintf(cert_out->status, sizeof(cert_out->status), "%s",
                 res->arb_found ? "ARBITRAGE" : "CERTIFIED");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }
        const int64_t S = 100; /* 0.01 pt ticks -> avoids i64 overflow on span * price */
        cert_out->has_arb = 1;
        cert_out->arb_mode = res->best_mode;
        cert_out->arb_scale = S;
        if (res->best_mode == 3) {
            const econ_option_quote_t *q1 = &ch->quotes[best_i];
            const econ_option_quote_t *q2 = &ch->quotes[best_j];
            const econ_option_quote_t *q3 = &ch->quotes[best_k];
            cert_out->k1 = (int64_t)llround(q1->strike * (double)S);
            cert_out->k2 = (int64_t)llround(q2->strike * (double)S);
            cert_out->k3 = (int64_t)llround(q3->strike * (double)S);
            cert_out->c_bid1 = (int64_t)llround(q1->c_bid * (double)S);
            cert_out->c_ask1 = (int64_t)llround(q1->c_ask * (double)S);
            cert_out->p_bid1 = (int64_t)llround(q1->p_bid * (double)S);
            cert_out->p_ask1 = (int64_t)llround(q1->p_ask * (double)S);
            cert_out->f_bid  = (int64_t)llround(q1->fut_bid * (double)S);
            cert_out->f_ask  = (int64_t)llround(q1->fut_ask * (double)S);
            cert_out->c_bid2 = (int64_t)llround(q2->c_bid * (double)S);
            cert_out->c_ask3 = (int64_t)llround(q3->c_ask * (double)S);
            cert_out->arb_cost_int = (int64_t)llround(best_cost * (double)S * (double)S);
            cert_out->arb_edge_int =
                (cert_out->k3 - cert_out->k1) * cert_out->c_bid2 -
                (cert_out->k3 - cert_out->k2) * cert_out->c_ask1 -
                (cert_out->k2 - cert_out->k1) * cert_out->c_ask3 -
                cert_out->arb_cost_int;
        } else {
            const econ_option_quote_t *qb = &ch->quotes[best_idx];
            double f_mid = 0.5 * (qb->fut_bid + qb->fut_ask);
            double cost = ch->fee + ((ch->tax_div > 0.0) ? (f_mid / ch->tax_div) : 0.0);
            cert_out->k1 = (int64_t)llround(qb->strike * (double)S);
            cert_out->k2 = cert_out->k1 + S;
            cert_out->k3 = cert_out->k1 + 2 * S;
            cert_out->c_bid1 = (int64_t)llround(qb->c_bid * (double)S);
            cert_out->c_ask1 = (int64_t)llround(qb->c_ask * (double)S);
            cert_out->p_bid1 = (int64_t)llround(qb->p_bid * (double)S);
            cert_out->p_ask1 = (int64_t)llround(qb->p_ask * (double)S);
            cert_out->f_bid  = (int64_t)llround(qb->fut_bid * (double)S);
            cert_out->f_ask  = (int64_t)llround(qb->fut_ask * (double)S);
            cert_out->c_bid2 = cert_out->c_bid1;
            cert_out->c_ask3 = cert_out->c_ask1;
            cert_out->arb_cost_int = (int64_t)llround(cost * (double)S);
            if (res->best_mode == 1) {
                cert_out->arb_edge_int = cert_out->f_bid -
                    (cert_out->k1 + cert_out->c_ask1 - cert_out->p_bid1) - cert_out->arb_cost_int;
            } else if (res->best_mode == 2) {
                cert_out->arb_edge_int =
                    (cert_out->k1 + cert_out->c_bid1 - cert_out->p_ask1) - cert_out->f_ask -
                    cert_out->arb_cost_int;
            } else {
                cert_out->arb_edge_int = 0;
            }
            cert_out->sniper_mid_res_int = (int64_t)llround(best_mid_res * (double)S);
        }
    }
    return 0;
}

int econ_parse_mot_market(const char *path, econ_mot_market_t *out) {
    if (!path || !out) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    memset(out, 0, sizeof(*out));
    out->s0 = 100.0; out->fwd = 100.0;
    out->t1 = 0.25;  out->t2 = 0.50;
    out->iv_atm = 0.20;

    char line[512];
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        if (line[0] == '#') {
            char *p;
            if ((p = strstr(line, "s0=")) != NULL) out->s0 = strtod(p + 3, NULL);
            if ((p = strstr(line, "fwd=")) != NULL) out->fwd = strtod(p + 4, NULL);
            if ((p = strstr(line, "t1=")) != NULL) out->t1 = strtod(p + 3, NULL);
            if ((p = strstr(line, "t2=")) != NULL) out->t2 = strtod(p + 3, NULL);
            if ((p = strstr(line, "iv=")) != NULL) out->iv_atm = strtod(p + 3, NULL);
            continue;
        }
        if (strncmp(line, "t_idx", 5) == 0) continue;
        int tidx = 0;
        double s = 0.0, q = 0.0;
        if (sscanf(line, " %d , %lf , %lf", &tidx, &s, &q) == 3) {
            if (tidx == 1 && out->n1 < ECON_CERT_MAX_GRID) {
                out->s1[out->n1] = s;
                out->q1[out->n1] = q;
                out->n1++;
            } else if (tidx == 2 && out->n2 < ECON_CERT_MAX_GRID) {
                out->s2[out->n2] = s;
                out->q2[out->n2] = q;
                out->n2++;
            }
        }
    }
    fclose(fp);
    return (out->n1 >= 2 && out->n2 >= 2) ? 0 : -1;
}

static double eval_payoff(const econ_mot_market_t *mkt, econ_payoff_t kind, double param,
                          double s1, double s2) {
    if (kind == ECON_PAYOFF_FWD_START) {
        double d = s2 - param * s1;
        return (d > 0.0) ? d : 0.0;
    } else if (kind == ECON_PAYOFF_VAR_SWAP) {
        double r1 = (s1 - mkt->s0) / mkt->s0;
        double r2 = (s2 - s1) / s1;
        return 10000.0 * (r1 * r1 + r2 * r2);
    } else {
        double m = mkt->s0;
        if (s1 > m) m = s1;
        if (s2 > m) m = s2;
        return m - s2;
    }
}

/* Univariate degree-4 SOS super-hedge / sub-hedge over [smin, smax] certified via Cholesky-SDD:
 * Uses empirical Hankel matrix H_{a,b} = sum_j q2[j] u_j^(a+b), u in [-1, 1] and solves
 * primal moment matching / polynomial super-hedge upper/lower bounds via dual SOS LP+PSD. */
static void compute_sos2_univar_bound(const econ_mot_market_t *mkt, econ_payoff_t kind,
                                      double param, double *sos_lb_out, double *sos_ub_out,
                                      int *dim_out, int64_t *A_int_out, int64_t *L_int_out,
                                      int64_t *mu_psd_out, int64_t *d_round_out) {
    /* Fit degree-4 Chebyshev-scaled super-hedge on S2 for worst marginal state s1_mean,
     * and compute its 3x3 Hankel Gram matrix M_0 of degree-2 monomials [1, u, u^2]. */
    double smin = mkt->s2[0], smax = mkt->s2[mkt->n2 - 1];
    double mid = 0.5 * (smin + smax);
    double half = 0.5 * (smax - smin);
    if (half < 1e-6) half = 1.0;

    double g[ECON_CERT_MAX_GRID];
    for (int j = 0; j < mkt->n2; ++j) {
        /* Conditional payoff envelope over s1 weights */
        double avg_p = 0.0;
        for (int i = 0; i < mkt->n1; ++i) {
            avg_p += mkt->q1[i] * eval_payoff(mkt, kind, param, mkt->s1[i], mkt->s2[j]);
        }
        g[j] = avg_p;
    }

    /* LP min sum_{d=0}^4 c_d * E[u^d] s.t. sum_d c_d u_j^d >= g_j; shifted vars c_d = c_d+ - c_d- */
    int n_vars = 10; /* 5 pos + 5 neg coeffs */
    int n_cons = mkt->n2;
    double c[10], A[ECON_CERT_MAX_GRID * 10], b[ECON_CERT_MAX_GRID], x[10];
    econ_lp_sense_t sens[ECON_CERT_MAX_GRID];
    double mom[5] = {0};
    for (int j = 0; j < mkt->n2; ++j) {
        double u = (mkt->s2[j] - mid) / half;
        double up = 1.0;
        for (int d = 0; d < 5; ++d) {
            mom[d] += mkt->q2[j] * up;
            up *= u;
        }
    }
    for (int d = 0; d < 5; ++d) { c[d] = mom[d]; c[5 + d] = -mom[d]; }
    for (int j = 0; j < n_cons; ++j) {
        double u = (mkt->s2[j] - mid) / half;
        double up = 1.0;
        for (int d = 0; d < 5; ++d) {
            A[j * n_vars + d] = up;
            A[j * n_vars + 5 + d] = -up;
            up *= u;
        }
        sens[j] = ECON_LP_GE;
        b[j] = g[j];
    }
    double obj_up = 0.0;
    if (econ_lp_solve(n_vars, n_cons, c, A, sens, b, x, NULL, &obj_up) == 0) {
        *sos_ub_out = obj_up;
    } else {
        double mx = g[0];
        for (int j = 1; j < mkt->n2; ++j) if (g[j] > mx) mx = g[j];
        *sos_ub_out = mx;
    }

    /* Sub-hedge LP: max sum c_d * E[u^d] s.t. sum c_d u_j^d <= g_j */
    for (int d = 0; d < 5; ++d) { c[d] = -mom[d]; c[5 + d] = mom[d]; }
    for (int j = 0; j < n_cons; ++j) sens[j] = ECON_LP_LE;
    double obj_lo = 0.0;
    if (econ_lp_solve(n_vars, n_cons, c, A, sens, b, x, NULL, &obj_lo) == 0) {
        *sos_lb_out = -obj_lo;
    } else {
        double mn = g[0];
        for (int j = 1; j < mkt->n2; ++j) if (g[j] < mn) mn = g[j];
        *sos_lb_out = mn;
    }

    /* Build 3x3 Hankel Moment Matrix H_{ab} = E[u^{a+b}] and certify via __int128 Cholesky-SDD */
    const int d = 3;
    *dim_out = d;
    const double S_mat = 10000.0;
    for (int i = 0; i < d; ++i) {
        for (int j = 0; j < d; ++j) {
            A_int_out[i * d + j] = (int64_t)llround(mom[i + j] * S_mat);
        }
    }
    if (econ_cert_cholesky_sdd_i64(d, A_int_out, L_int_out, mu_psd_out) != 0) {
        *mu_psd_out = -1;
    }
    *d_round_out = 1;
}

int econ_mot_bound(const econ_mot_market_t *mkt, econ_payoff_t payoff_kind,
                   double param, const char *input_path,
                   econ_mot_res_t *res, econ_cert_record_t *cert_out) {
    if (!mkt || !res || mkt->n1 < 2 || mkt->n2 < 2 ||
        mkt->n1 > ECON_CERT_MAX_GRID || mkt->n2 > ECON_CERT_MAX_GRID) return -1;
    memset(res, 0, sizeof(*res));
    int n1 = mkt->n1, n2 = mkt->n2;
    int nv = n1 * n2;
    int m_frechet = n1 + n2;
    int m_mot = n1 + n2 + n1;

    double *c = (double *)calloc((size_t)nv, sizeof(double));
    double *A = (double *)calloc((size_t)m_mot * (size_t)nv, sizeof(double));
    double *b = (double *)calloc((size_t)m_mot, sizeof(double));
    econ_lp_sense_t *sens = (econ_lp_sense_t *)calloc((size_t)m_mot, sizeof(econ_lp_sense_t));
    double *x = (double *)calloc((size_t)nv, sizeof(double));
    double *y_up = (double *)calloc((size_t)m_mot, sizeof(double));
    double *y_lo = (double *)calloc((size_t)m_mot, sizeof(double));
    double *pay = (double *)calloc((size_t)nv, sizeof(double));
    if (!c || !A || !b || !sens || !x || !y_up || !y_lo || !pay) {
        free(c); free(A); free(b); free(sens); free(x); free(y_up); free(y_lo); free(pay);
        return -1;
    }

    for (int i = 0; i < n1; ++i) {
        for (int j = 0; j < n2; ++j) {
            pay[i * n2 + j] = eval_payoff(mkt, payoff_kind, param, mkt->s1[i], mkt->s2[j]);
        }
    }
    for (int i = 0; i < n1; ++i) {
        for (int j = 0; j < n2; ++j) A[(size_t)i * (size_t)nv + (size_t)(i * n2 + j)] = 1.0;
        b[i] = mkt->q1[i];
        sens[i] = ECON_LP_EQ;
    }
    for (int j = 0; j < n2; ++j) {
        for (int i = 0; i < n1; ++i) A[(size_t)(n1 + j) * (size_t)nv + (size_t)(i * n2 + j)] = 1.0;
        b[n1 + j] = mkt->q2[j];
        sens[n1 + j] = ECON_LP_EQ;
    }
    for (int i = 0; i < n1; ++i) {
        int r = n1 + n2 + i;
        for (int j = 0; j < n2; ++j) {
            A[(size_t)r * (size_t)nv + (size_t)(i * n2 + j)] = mkt->s2[j] - mkt->s1[i];
        }
        b[r] = 0.0;
        sens[r] = ECON_LP_EQ;
    }

    /* Tier 1: Frechet LP (no martingale constraint) */
    for (int k = 0; k < nv; ++k) c[k] = pay[k];
    if (econ_lp_solve(nv, m_frechet, c, A, sens, b, x, NULL, &res->frechet_lb) != 0) {
        res->frechet_lb = 0.0;
    }
    for (int k = 0; k < nv; ++k) c[k] = -pay[k];
    double f_neg_ub = 0.0;
    if (econ_lp_solve(nv, m_frechet, c, A, sens, b, x, NULL, &f_neg_ub) == 0) {
        res->frechet_ub = -f_neg_ub;
    }

    /* Tier 2: Full 2-period MOT LP (primal + dual super/sub-hedge) */
    for (int k = 0; k < nv; ++k) c[k] = pay[k];
    int rc_lo = econ_lp_solve(nv, m_mot, c, A, sens, b, x, y_lo, &res->mot_lb);
    for (int k = 0; k < nv; ++k) c[k] = -pay[k];
    double neg_ub = 0.0;
    int rc_up = econ_lp_solve(nv, m_mot, c, A, sens, b, x, y_up, &neg_ub);
    res->mot_ub = -neg_ub;

    /* Tier 3: Moment-SOS univar relaxation + SDD Hankel cert */
    int sos_dim = 3;
    int64_t sos_A[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM] = {0};
    int64_t sos_L[ECON_CERT_MAX_DIM * ECON_CERT_MAX_DIM] = {0};
    int64_t sos_mu = 0, sos_dr = 1;
    compute_sos2_univar_bound(mkt, payoff_kind, param, &res->sos_lb, &res->sos_ub,
                              &sos_dim, sos_A, sos_L, &sos_mu, &sos_dr);

    /* Black-Scholes reference formula (zero-rate / forward-normalized) */
    if (payoff_kind == ECON_PAYOFF_FWD_START) {
        double dt = mkt->t2 - mkt->t1;
        if (dt < 1e-6) dt = 0.25;
        res->bs_price = bs_call_price(mkt->s0, mkt->s0 * param, 0.0, mkt->iv_atm, dt);
    } else if (payoff_kind == ECON_PAYOFF_VAR_SWAP) {
        res->bs_price = 10000.0 * mkt->iv_atm * mkt->iv_atm * mkt->t2;
    } else {
        double c_ref = bs_call_price(mkt->s0, mkt->s0, 0.0, mkt->iv_atm, mkt->t2);
        res->bs_price = 1.85 * c_ref; /* Lookback put reference scale */
    }

    res->spread_mot = res->mot_ub - res->mot_lb;
    res->spread_frechet = res->frechet_ub - res->frechet_lb;
    res->tightening_ratio = (res->spread_frechet > 1e-9)
        ? (res->spread_mot / res->spread_frechet) : 1.0;
    res->certified = (rc_lo == 0 && rc_up == 0 && res->mot_lb <= res->mot_ub + 1e-6) ? 1 : 0;

    if (cert_out != NULL) {
        memset(cert_out, 0, sizeof(*cert_out));
        snprintf(cert_out->pillar, sizeof(cert_out->pillar), "fin");
        snprintf(cert_out->kind, sizeof(cert_out->kind), "mot_sos");
        snprintf(cert_out->status, sizeof(cert_out->status), "%s",
                 res->certified ? "CERTIFIED" : "VETOED");
        snprintf(cert_out->input_path, sizeof(cert_out->input_path), "%s",
                 input_path ? input_path : "synthetic");
        if (input_path && econ_sha256_file(input_path, cert_out->input_sha256) != 0) {
            snprintf(cert_out->input_sha256, sizeof(cert_out->input_sha256),
                     "0000000000000000000000000000000000000000000000000000000000000000");
        }
        const int64_t MS = 1000;
        cert_out->has_mot = 1;
        cert_out->mot_n1 = n1;
        cert_out->mot_n2 = n2;
        cert_out->mot_scale = MS;
        for (int i = 0; i < n1; ++i) {
            cert_out->s1[i] = (int64_t)llround(mkt->s1[i] * (double)MS);
            cert_out->q1[i] = (int64_t)llround(mkt->q1[i] * 1000000.0);
            cert_out->phi1_up[i] = (int64_t)llround(-y_up[i] * (double)MS);
            cert_out->delta_up[i] = (int64_t)llround(-y_up[n1 + n2 + i] * (double)MS);
            cert_out->phi1_lo[i] = (int64_t)llround(y_lo[i] * (double)MS);
            cert_out->delta_lo[i] = (int64_t)llround(y_lo[n1 + n2 + i] * (double)MS);
        }
        for (int j = 0; j < n2; ++j) {
            cert_out->s2[j] = (int64_t)llround(mkt->s2[j] * (double)MS);
            cert_out->q2[j] = (int64_t)llround(mkt->q2[j] * 1000000.0);
            cert_out->phi2_up[j] = (int64_t)llround(-y_up[n1 + j] * (double)MS);
            cert_out->phi2_lo[j] = (int64_t)llround(y_lo[n1 + j] * (double)MS);
        }
        /* Tighten integer rounding strictly onto phi2_up and phi2_lo so M11 holds identically */
        int64_t worst_sup_def = 0, worst_sub_exc = 0;
        for (int i = 0; i < n1; ++i) {
            for (int j = 0; j < n2; ++j) {
                int64_t py = (int64_t)llround(pay[i * n2 + j] * (double)MS);
                cert_out->payoff[(size_t)i * ECON_CERT_MAX_GRID + (size_t)j] = py;
                econ_i128 ds = (econ_i128)cert_out->s2[j] - (econ_i128)cert_out->s1[i];
                econ_i128 sup = ((econ_i128)cert_out->phi1_up[i] + (econ_i128)cert_out->phi2_up[j]) *
                                (econ_i128)MS + (econ_i128)cert_out->delta_up[i] * ds;
                econ_i128 target = (econ_i128)py * (econ_i128)MS;
                if (sup < target) {
                    int64_t need = (int64_t)((target - sup + MS - 1) / MS);
                    if (need > worst_sup_def) worst_sup_def = need;
                }
                econ_i128 sub = ((econ_i128)cert_out->phi1_lo[i] + (econ_i128)cert_out->phi2_lo[j]) *
                                (econ_i128)MS + (econ_i128)cert_out->delta_lo[i] * ds;
                if (sub > target) {
                    int64_t exc = (int64_t)((sub - target + MS - 1) / MS);
                    if (exc > worst_sub_exc) worst_sub_exc = exc;
                }
            }
        }
        if (worst_sup_def > 0) for (int j = 0; j < n2; ++j) cert_out->phi2_up[j] += worst_sup_def;
        if (worst_sub_exc > 0) for (int j = 0; j < n2; ++j) cert_out->phi2_lo[j] -= worst_sub_exc;

        cert_out->mot_frechet_lb_int = (int64_t)floor(res->frechet_lb * (double)MS);
        cert_out->mot_lb_int = (int64_t)floor(res->mot_lb * (double)MS) - worst_sub_exc;
        cert_out->mot_ub_int = (int64_t)ceil(res->mot_ub * (double)MS) + worst_sup_def;
        cert_out->mot_frechet_ub_int = (int64_t)ceil(res->frechet_ub * (double)MS) + worst_sup_def;
        cert_out->mot_bs_int = (int64_t)llround(res->bs_price * (double)MS);

        if (cert_out->mot_frechet_lb_int > cert_out->mot_lb_int) {
            cert_out->mot_frechet_lb_int = cert_out->mot_lb_int;
        }
        if (cert_out->mot_frechet_ub_int < cert_out->mot_ub_int) {
            cert_out->mot_frechet_ub_int = cert_out->mot_ub_int;
        }

        cert_out->has_bracket = 1;
        cert_out->bracket_scale = MS;
        cert_out->lb_int = cert_out->mot_lb_int;
        cert_out->ub_int = cert_out->mot_ub_int;
        cert_out->limit_lo_int = cert_out->mot_frechet_lb_int;
        cert_out->limit_hi_int = cert_out->mot_frechet_ub_int;
        cert_out->lb_val = res->mot_lb;
        cert_out->ub_val = res->mot_ub;

        if (sos_mu >= 0) {
            cert_out->has_psd = 1;
            cert_out->psd.dim = sos_dim;
            cert_out->psd.scale_q = 10000;
            cert_out->psd.mu_psd = sos_mu;
            cert_out->psd.delta_round = sos_dr;
            cert_out->psd.mu_cert = sos_mu - sos_dr;
            cert_out->psd.ok = (cert_out->psd.mu_cert >= 0) ? 1 : 0;
            memcpy(cert_out->psd.A, sos_A, (size_t)(sos_dim * sos_dim) * sizeof(int64_t));
            memcpy(cert_out->psd.L, sos_L, (size_t)(sos_dim * sos_dim) * sizeof(int64_t));
        }
    }

    free(c); free(A); free(b); free(sens); free(x); free(y_up); free(y_lo); free(pay);
    return 0;
}
