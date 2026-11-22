#!/usr/bin/env python3
"""Pure Python stdlib SDK & in-process ctypes session wrapper for ECON v1.0.0."""

from __future__ import annotations

import ctypes
import json
import os
import subprocess
import sys
import tempfile


class EconEngine:
    """Thin Python wrapper over `build/econ_cli` returning parsed JSON outputs."""

    def __init__(self, cli_path: str = "build/econ_cli"):
        self.cli_path = cli_path

    def _run_json(self, sub_args: list[str]) -> dict:
        with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tf:
            jp = tf.name
        try:
            cmd = [self.cli_path] + sub_args + ["--json", jp]
            subprocess.run(cmd, check=True, capture_output=True, text=True)
            with open(jp, "r") as f:
                return json.load(f)
        finally:
            if os.path.exists(jp):
                os.remove(jp)

    def selftest(self) -> int:
        res = subprocess.run([self.cli_path, "selftest"], capture_output=True, text=True)
        return res.returncode

    def arb_scan(self, csv_path: str, cert_path: str | None = None, lean_path: str | None = None, mod_name: str = "CertArb") -> dict:
        args = ["arb-scan", csv_path]
        if cert_path:
            args += ["--cert", cert_path]
        if lean_path:
            args += ["--emit-lean", lean_path, "--name", mod_name]
        return self._run_json(args)

    def mot_bound(self, csv_path: str, payoff: str = "fwd_start", param: float = 1.0, cert_path: str | None = None) -> dict:
        args = ["mot-bound", csv_path, "--payoff", payoff, "--param", str(param)]
        if cert_path:
            args += ["--cert", cert_path]
        return self._run_json(args)

    def risk_dro(self, json_path: str, alpha: float | None = None, cert_path: str | None = None) -> dict:
        args = ["risk-dro", json_path]
        if alpha is not None:
            args += ["--alpha", str(alpha)]
        if cert_path:
            args += ["--cert", cert_path]
        return self._run_json(args)

    def ruin_bound(self, json_path: str, target_ruin: float = 0.005, cert_path: str | None = None) -> dict:
        args = ["ruin-bound", json_path, "--target-ruin", str(target_ruin)]
        if cert_path:
            args += ["--cert", cert_path]
        return self._run_json(args)

    def clear_net(self, json_path: str, alpha_fire: float | None = None, searu: bool = False, cert_path: str | None = None) -> dict:
        args = ["clear-net", json_path]
        if alpha_fire is not None:
            args += ["--alpha-fire", str(alpha_fire)]
        if searu:
            args += ["--searu"]
        if cert_path:
            args += ["--cert", cert_path]
        return self._run_json(args)

    def mech_verify(self, json_path: str, cert_path: str | None = None) -> dict:
        args = ["mech-verify", json_path]
        if cert_path:
            args += ["--cert", cert_path]
        return self._run_json(args)

    def bilateral_ms(self, K: int, second_best: bool = False, cert_path: str | None = None) -> dict:
        args = ["bilateral-ms", "--K", str(K)]
        if second_best:
            args += ["--second-best"]
        if cert_path:
            args += ["--cert", cert_path]
        return self._run_json(args)

    def fisher_eq(self, json_path: str, cert_path: str | None = None) -> dict:
        args = ["fisher-eq", json_path]
        if cert_path:
            args += ["--cert", cert_path]
        return self._run_json(args)

    def cert_verify(self, cert_path: str, check_hash: bool = True) -> bool:
        args = [self.cli_path, "cert-verify", cert_path]
        if not check_hash:
            args.append("--no-hash")
        res = subprocess.run(args, capture_output=True, text=True)
        return res.returncode == 0


class EconFastSession:
    """In-process persistent ctypes clearing & MS session over build/libecon.{dylib,so}."""

    _LIB_CACHE: dict[str, ctypes.CDLL] = {}

    @classmethod
    def _load_lib(cls, lib_path: str | None = None) -> ctypes.CDLL:
        if lib_path is None:
            ext = "dylib" if sys.platform == "darwin" else "so"
            lib_path = os.path.join("build", f"libecon.{ext}")
        key = os.path.abspath(lib_path)
        if key not in cls._LIB_CACHE:
            lib = ctypes.CDLL(key)
            lib.econ_clear_session_create.argtypes = [ctypes.c_char_p]
            lib.econ_clear_session_create.restype = ctypes.c_void_p
            lib.econ_clear_session_destroy.argtypes = [ctypes.c_void_p]
            lib.econ_clear_session_destroy.restype = None
            lib.econ_clear_session_sweep.argtypes = [
                ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
                ctypes.POINTER(ctypes.c_double),
            ]
            lib.econ_clear_session_sweep.restype = ctypes.c_int
            lib.econ_fast_ms_eval.argtypes = [
                ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_double)
            ]
            lib.econ_fast_ms_eval.restype = ctypes.c_int
            cls._LIB_CACHE[key] = lib
        return cls._LIB_CACHE[key]

    def __init__(self, net_json_path: str | None = None, lib_path: str | None = None):
        self._lib = self._load_lib(lib_path)
        self._sess = None
        if net_json_path is not None:
            self._sess = self._lib.econ_clear_session_create(net_json_path.encode("utf-8"))
            if not self._sess:
                raise RuntimeError(f"Failed to create EconClearingSession for {net_json_path}")

    def close(self) -> None:
        if getattr(self, "_sess", None):
            self._lib.econ_clear_session_destroy(self._sess)
            self._sess = None

    def __del__(self) -> None:
        self.close()

    def sweep_clearing(self, alpha_fire: float, beta_def: float = 0.85, shock_scale: float = 1.0) -> dict:
        if not self._sess:
            raise RuntimeError("No clearing network loaded in EconFastSession")
        buf = (ctypes.c_double * 6)()
        rc = self._lib.econ_clear_session_sweep(
            self._sess, float(alpha_fire), float(beta_def), float(shock_scale), buf
        )
        if rc != 0:
            raise RuntimeError("econ_clear_session_sweep failed")
        return {
            "paid_upper": buf[0],
            "paid_lower": buf[1],
            "multi_eq_gap": buf[2],
            "defaults_lower": int(round(buf[3])),
            "systemic_shortfall": buf[4],
            "min_bailout": buf[5],
        }

    def eval_ms(self, K: int, second_best: bool = True) -> dict:
        buf = (ctypes.c_double * 5)()
        rc = self._lib.econ_fast_ms_eval(int(K), 1 if second_best else 0, buf)
        if rc != 0:
            raise RuntimeError(f"econ_fast_ms_eval failed for K={K}")
        return {
            "K": K,
            "efficient_welfare": buf[0],
            "min_rent": buf[1],
            "max_budget_surplus": buf[2],
            "norm_surplus_ratio": buf[3],
            "second_best_eff_pct": buf[4],
        }
