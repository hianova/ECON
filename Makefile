export PATH := $(HOME)/.elan/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/local/bin:$(PATH)

ifeq ($(origin CC),default)
CC := clang
endif
CXX  ?= clang++
LEAN ?= lean

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SHLIB_EXT   := dylib
SHLIB_FLAGS := -dynamiclib
else
SHLIB_EXT   := so
SHLIB_FLAGS := -shared
endif

FLOW_DIR       := $(abspath ../FLOW)
ELE_DIR        := $(abspath ../ELE)
FLOW_OLEAN_DIR := $(FLOW_DIR)/build/lean
FLOW_OLEAN     := $(FLOW_OLEAN_DIR)/Flow.olean
FLOW_BIN       := $(FLOW_DIR)/build/flow
FLOW_H         := $(FLOW_DIR)/core/flow.h
SEARU_H        := $(FLOW_DIR)/core/searu.h
FLOW_IR_H      := $(FLOW_DIR)/sdk/flow_ir.h
FLOW_IR_IO_H   := $(FLOW_DIR)/sdk/flow_ir_io.h
FLOW_INC_FLAGS := -I"$(FLOW_DIR)/core" -I"$(FLOW_DIR)/sdk"

ECON_CFLAGS  := -std=c17 -O3 -Wall -Wextra -Werror -Wpedantic -Wconversion -Iinclude $(FLOW_INC_FLAGS)
ECON_LDFLAGS := -lm -pthread
ECON_HDRS    := include/econ_cert.h include/econ_fin.h include/econ_actuary.h include/econ_clearing.h include/econ_mech.h include/econ_agency.h include/econ_adverse.h include/econ_margin.h include/econ_inst.h
ECON_SRCS    := src/econ_cert.c src/econ_fin.c src/econ_actuary.c src/econ_clearing.c src/econ_mech.c src/econ_agency.c src/econ_adverse.c src/econ_margin.c src/econ_inst.c

NOGPR_FLAG   := $(shell $(CC) -mgeneral-regs-only -x c -c /dev/null -o /dev/null 2>/dev/null && echo -mgeneral-regs-only)
NOJT_FLAG    := $(shell $(CC) -fno-jump-tables -x c -c /dev/null -o /dev/null 2>/dev/null && echo -fno-jump-tables)
NOCHK_FLAG   := $(shell $(CC) -mstack-probe-size=65536 -x c -c /dev/null -o /dev/null 2>/dev/null && echo -mstack-probe-size=65536)
UBTRAP_FLAGS := $(shell $(CC) -fsanitize=signed-integer-overflow,shift-exponent,bounds,null,return,unreachable -fsanitize-trap=all -x c -c /dev/null -o /dev/null 2>/dev/null && echo "-fsanitize=signed-integer-overflow,shift-exponent,bounds,null,return,unreachable -fsanitize-trap=all")
CFLAGS_GPR   := -std=c17 $(NOGPR_FLAG) $(NOJT_FLAG) $(NOCHK_FLAG) $(UBTRAP_FLAGS) -O2 -Wall -Wextra -Wpedantic -Werror

BUILD_DIR     := build
ECON_OLEAN    := $(BUILD_DIR)/Econ.olean
ECON_FLOW     := $(BUILD_DIR)/econ_taifex.flow
ECON_ABI_H    := $(BUILD_DIR)/econ_ring0_abi.h
ECON_FIR      := $(BUILD_DIR)/econ_bidask_arb.fir
RING0_GATE_C  := $(BUILD_DIR)/econ_ring0_gate.c
RING0_GATE_O  := $(BUILD_DIR)/econ_ring0_gate.o
RING0_GATE    := $(BUILD_DIR)/econ_ring0_gate

ECONCERT_OLN  := $(BUILD_DIR)/EconCert.olean
ECON_LIB      := $(BUILD_DIR)/libecon.$(SHLIB_EXT)
ECON_CLI      := $(BUILD_DIR)/econ_cli
CERT_DIR      := $(BUILD_DIR)/certs
CERT_MODS     := CertEconArbBidAsk CertEconSniperUpgrade CertEconMotSos CertEconRiskDro CertEconCopulaObs CertEconClearing CertEconMechVcg CertEconMsDeficit CertEconFisherEq CertEconAgency CertEconAdverse CertEconMargin CertEconInstRaw CertEconInstRepaired

.DEFAULT_GOAL := all
.PHONY: all hygiene-gate flow-dep ring0-gate econ-build econ-benchmarks econ-gate test verify experiments clean

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

hygiene-gate: | $(BUILD_DIR)
	@echo "=== [Gate 1] ECON SSOT Hygiene, Thin Econ.lean (<=150 lines, 0 Theorems) & C++20 ABI Probe ==="
	@! test -d flow || (echo "ERROR: Vendored flow/ directory detected!" >&2 && exit 1)
	@! grep -Eq '^[[:space:]]*((theorem|lemma|axiom)[[:space:]]|@\[extern)' Econ.lean || \
		(echo "ERROR: Econ.lean violates SSOT pure-config rule!" >&2 && exit 1)
	@econ_lines="$$(wc -l < Econ.lean | tr -d '[:space:]')"; \
	if [ "$$econ_lines" -gt 150 ]; then echo "ERROR: Econ.lean exceeds 150 lines ($$econ_lines)" >&2; exit 1; fi
	@! grep -Eq '^[[:space:]]*axiom[[:space:]]|\bsorry\b|native_decide' lean/EconCert.lean || \
		(echo "ERROR: lean/EconCert.lean contains sorry, axiom, or native_decide!" >&2 && exit 1)
	@printf '#include "econ_cert.h"\n#include "econ_fin.h"\n#include "econ_actuary.h"\n#include "econ_clearing.h"\n#include "econ_mech.h"\n#include "econ_agency.h"\n#include "econ_adverse.h"\n#include "econ_margin.h"\n#include "econ_inst.h"\nint econ_cpp20_probe(void){ return (int)sizeof(econ_cert_record_t); }\n' | \
		$(CXX) -x c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Iinclude $(FLOW_INC_FLAGS) -c - -o $(BUILD_DIR)/.cpp_probe.o
	@rm -f $(BUILD_DIR)/.cpp_probe.o
	@echo "  [PASS] Hygiene + Thin Econ.lean (<=150 lines, 0 theorems) + C++20 Header Cleanliness"

flow-dep:
	@test -f "$(FLOW_OLEAN)" && test -f "$(FLOW_OLEAN_DIR)/Flow/Schema.olean" && test -f "$(FLOW_OLEAN_DIR)/Flow/Interval.olean" && test -f "$(FLOW_OLEAN_DIR)/Flow/CholeskySDD.olean" && test -x "$(FLOW_BIN)" || \
		$(MAKE) -C "$(FLOW_DIR)" all

$(ECON_OLEAN): Econ.lean $(FLOW_OLEAN) | $(BUILD_DIR) hygiene-gate flow-dep
	LEAN_PATH="$(FLOW_OLEAN_DIR)" $(LEAN) -DwarningAsError=true -o $@ Econ.lean

$(ECON_FLOW): Econ.lean $(ECON_OLEAN) | $(BUILD_DIR) flow-dep
	LEAN_PATH="$(FLOW_OLEAN_DIR)" $(LEAN) -DwarningAsError=true --run Econ.lean $(BUILD_DIR)
	@sz="$$(wc -c < $(ECON_FLOW) | tr -d '[:space:]')" && test "$$sz" -eq 320
	"$(FLOW_BIN)" verify $(ECON_FLOW) > /dev/null

$(ECON_ABI_H) $(ECON_FIR): $(ECON_FLOW)
	@test -f "$@"

ring0-gate: hygiene-gate flow-dep $(ECON_FLOW) $(ECON_ABI_H) $(ECON_FIR)
	@echo "=== [Gate 2] Ring-0 320B FLOW + Pure-GPR Bid-Ask Arbitrage .fir (SNIPER Upgrade) & Silicon Projection ==="
	@"$(FLOW_BIN)" verify $(ECON_FLOW)
	@printf '#include "econ_ring0_abi.h"\n#include "flow_ir.h"\nint r0_cpp(void){ return (int)(sizeof(econ_wire_frame_t) + sizeof(econ_manifold_state_t)); }\n' | \
		$(CXX) -x c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror $(FLOW_INC_FLAGS) -I$(BUILD_DIR) -c - -o $(BUILD_DIR)/.r0_cpp.o
	@rm -f $(BUILD_DIR)/.r0_cpp.o
	@printf '%s\n' \
		'#include "econ_ring0_abi.h"' \
		'#include "flow_ir_io.h"' \
		'#include <stdio.h>' \
		'static flow_ir_prog_t g_prog;' \
		'static uint8_t g_buf[FLOW_IR_MAX_FILE_BYTES];' \
		'int main(void) {' \
		'  if (ECON_TOTAL_CELLS != 2U) return 1;' \
		'  econ_wire_frame_t wf = {UINT64_C(0x45434F4E57495231), 1ULL, 2ULL, 3ULL, 4ULL, INT64_C(22500) * FLOW_Q16_ONE, 1U, 3U, 9ULL};' \
		'  uint8_t wire[64]; econ_wire_frame_pack_bytes(wire, &wf);' \
		'  econ_wire_frame_t wf2 = {0}; econ_wire_frame_unpack_bytes(wire, &wf2);' \
		'  if (wf2.magic != wf.magic || wf2.strike_q16 != wf.strike_q16 || wf2.seq_no != 9ULL) return 2;' \
		'  if (!flow_ir_load_file(&g_prog, "$(ECON_FIR)", g_buf, sizeof(g_buf))) return 3;' \
		'  if (!flow_ir_admits_unitary(&g_prog)) return 4;' \
		'  int64_t Q = FLOW_Q16_ONE;' \
		'  uint64_t out[4] = {0};' \
		'  /* Case A: SNIPER False Positive (mid_res = +2.5 >= 2.0, wide spread/cost rejects conversion/reversal) */' \
		'  uint64_t in_fp[8] = {(uint64_t)(22500*Q), (uint64_t)(498*Q), (uint64_t)(506*Q), (uint64_t)(496*Q), (uint64_t)(504*Q), (uint64_t)(22496*Q), (uint64_t)(22504*Q), (uint64_t)(2*Q)};' \
		'  if (!flow_ir_eval_u64(&g_prog, in_fp, out)) return 5;' \
		'  if ((int64_t)out[0] > 0 || (int64_t)out[1] > 0) return 6;' \
		'  /* Case B: Genuine Conversion Arbitrage (fut_bid=22510 > 22500 + 502 - 498 + 2 = 22506 -> edge=+4) */' \
		'  uint64_t in_conv[8] = {(uint64_t)(22500*Q), (uint64_t)(500*Q), (uint64_t)(502*Q), (uint64_t)(498*Q), (uint64_t)(500*Q), (uint64_t)(22510*Q), (uint64_t)(22512*Q), (uint64_t)(2*Q)};' \
		'  if (!flow_ir_eval_u64(&g_prog, in_conv, out) || (int64_t)out[0] != 4 * Q) return 7;' \
		'  /* Case C: Genuine Reversal Arbitrage (22500 + 510 - 500 = 22510 > fut_ask=22502 + 2 -> edge=+6) */' \
		'  uint64_t in_rev[8] = {(uint64_t)(22500*Q), (uint64_t)(510*Q), (uint64_t)(512*Q), (uint64_t)(498*Q), (uint64_t)(500*Q), (uint64_t)(22500*Q), (uint64_t)(22502*Q), (uint64_t)(2*Q)};' \
		'  if (!flow_ir_eval_u64(&g_prog, in_rev, out) || (int64_t)out[1] != 6 * Q) return 8;' \
		'  printf("  [ECON-GPR] WireFrame64 64B + econ_bidask_arb.fir 0-ULP Conversion/Reversal/FP PASS\n");' \
		'  return 0;' \
		'}' > $(RING0_GATE_C)
	@$(CC) $(CFLAGS_GPR) $(FLOW_INC_FLAGS) -I$(BUILD_DIR) -c $(RING0_GATE_C) -o $(RING0_GATE_O)
	@! objdump -d $(RING0_GATE_O) | grep -Eq '[[:space:]](blr|br|udiv|sdiv)[[:space:]]' || \
		(echo "ERROR: Forbidden br/blr/div instruction in econ_ring0_gate.o!" >&2 && exit 1)
	@! objdump -d $(RING0_GATE_O) | grep -Eq '([[:space:]]|%)(fadd|fsub|fmul|fdiv|scvtf|ucvtf|fcvtzs|fcvtzu|fcmp|fmov)' || \
		(echo "ERROR: FPU/SIMD instruction leaked into econ_ring0_gate.o!" >&2 && exit 1)
	@$(CC) $(CFLAGS_GPR) $(RING0_GATE_O) -o $(RING0_GATE)
	@$(RING0_GATE)
	@if [ -x "$(ELE_DIR)/build/ele_bootstrap" ] && [ -f "$(ELE_DIR)/build/pdk_sky130.flow" ]; then \
		"$(ELE_DIR)/build/ele_bootstrap" --fir-synth $(ECON_FIR) -o $(BUILD_DIR)/econ_bidask_arb.sv --pdk "$(ELE_DIR)/build/pdk_sky130.flow" --gds-out $(BUILD_DIR)/econ_bidask_arb_sky130.gds --verify-fir; \
	fi

$(ECONCERT_OLN): lean/EconCert.lean $(FLOW_OLEAN) | $(BUILD_DIR) flow-dep
	LEAN_PATH="$(FLOW_OLEAN_DIR):$(BUILD_DIR)" $(LEAN) -DwarningAsError=true -o $@ $<

$(ECON_LIB): $(ECON_SRCS) $(ECON_HDRS) $(FLOW_H) $(SEARU_H) | $(BUILD_DIR)
	$(CC) $(ECON_CFLAGS) -fPIC $(SHLIB_FLAGS) $(ECON_SRCS) $(ECON_LDFLAGS) -o $@

$(ECON_CLI): src/econ_cli.c $(ECON_SRCS) $(ECON_HDRS) $(FLOW_H) $(SEARU_H) | $(BUILD_DIR)
	$(CC) $(ECON_CFLAGS) src/econ_cli.c $(ECON_SRCS) $(ECON_LDFLAGS) -o $@

econ-build: hygiene-gate $(ECONCERT_OLN) $(ECON_LIB) $(ECON_CLI)

econ-benchmarks:
	python3 oracles/gen_benchmarks.py

econ-gate: econ-build econ-benchmarks
	@echo "=== [Gate 3] ECON v2.0.0 8-Pillar Engine, Zero-FP-Trust __int128 Certs & Lean 4 Audits ==="
	LEAN_PATH="$(FLOW_OLEAN_DIR):$(BUILD_DIR)" leanchecker EconCert
	./$(ECON_CLI) selftest
	@mkdir -p $(CERT_DIR)
	./$(ECON_CLI) bench --dir benchmarks --out $(BUILD_DIR)/econ_bench.json --cert-dir $(CERT_DIR)
	./$(ECON_CLI) arb-scan benchmarks/fin/taifex_txo_arb_butterfly.csv --emit-lean $(CERT_DIR)/CertEconArbBidAsk.lean --name CertEconArbBidAsk
	./$(ECON_CLI) arb-scan benchmarks/fin/taifex_txo_clean.csv --emit-lean $(CERT_DIR)/CertEconSniperUpgrade.lean --name CertEconSniperUpgrade
	./$(ECON_CLI) mot-bound benchmarks/fin/spx_2period_skew.csv --emit-lean $(CERT_DIR)/CertEconMotSos.lean --name CertEconMotSos
	./$(ECON_CLI) risk-dro benchmarks/actuary/danish_fire_heavy_tail.json --emit-lean $(CERT_DIR)/CertEconRiskDro.lean --name CertEconRiskDro
	./$(ECON_CLI) ruin-bound benchmarks/actuary/copula_obstruction_2d.json --emit-lean $(CERT_DIR)/CertEconCopulaObs.lean --name CertEconCopulaObs
	./$(ECON_CLI) clear-net benchmarks/clearing/ring_network_6.json --searu --emit-lean $(CERT_DIR)/CertEconClearing.lean --name CertEconClearing
	./$(ECON_CLI) mech-verify benchmarks/mech/spectrum_comb_auction.json --emit-lean $(CERT_DIR)/CertEconMechVcg.lean --name CertEconMechVcg
	./$(ECON_CLI) bilateral-ms --K 6 --second-best --emit-lean $(CERT_DIR)/CertEconMsDeficit.lean --name CertEconMsDeficit
	./$(ECON_CLI) fisher-eq benchmarks/mech/fisher_4agent_3good.json --emit-lean $(CERT_DIR)/CertEconFisherEq.lean --name CertEconFisherEq
	./$(ECON_CLI) agency-audit benchmarks/agency/fund_2and20_clawback.json --emit-lean $(CERT_DIR)/CertEconAgency.lean --name CertEconAgency
	./$(ECON_CLI) adverse-audit benchmarks/adverse/platform_marketplace.json --emit-lean $(CERT_DIR)/CertEconAdverse.lean --name CertEconAdverse
	./$(ECON_CLI) margin-audit benchmarks/margin/exchange_perp_8acct.json --emit-lean $(CERT_DIR)/CertEconMargin.lean --name CertEconMargin
	./$(ECON_CLI) audit-inst benchmarks/inst/inst_fund_kpi_raw.json --emit-lean $(CERT_DIR)/CertEconInstRaw.lean --name CertEconInstRaw
	./$(ECON_CLI) audit-inst benchmarks/inst/inst_fund_kpi_repaired.json --emit-lean $(CERT_DIR)/CertEconInstRepaired.lean --name CertEconInstRepaired
	@for c in $(CERT_DIR)/*.econcert.json; do ./$(ECON_CLI) cert-verify "$$c" || exit 1; done
	@for m in $(CERT_MODS); do \
		LEAN_PATH="$(FLOW_OLEAN_DIR):$(BUILD_DIR)" $(LEAN) -DwarningAsError=true -o $(BUILD_DIR)/$$m.olean $(CERT_DIR)/$$m.lean > $(CERT_DIR)/$$m.axioms.txt || exit 1; \
		LEAN_PATH="$(FLOW_OLEAN_DIR):$(BUILD_DIR)" leanchecker $$m || exit 1; \
	done
	python3 oracles/verify_econ_oracle.py --bench $(BUILD_DIR)/econ_bench.json --bench-dir benchmarks --cert-dir $(CERT_DIR) --report ECON_VERIFICATION_REPORT.md
	python3 oracles/test_tools.py

all: hygiene-gate ring0-gate econ-build

test: ring0-gate econ-gate
	@echo "================================================================================"
	@echo "   ALL ECON v2.0.0 RING-0, LEAN 4 (30 THMS), 8-PILLAR & INSTITUTIONAL DRC GATES PASS"
	@echo "================================================================================"

verify: test

experiments: econ-build econ-benchmarks
	python3 experiments/exp1_sniper_vs_econ_arb.py
	python3 experiments/exp2_mot_sos_hierarchy.py
	python3 experiments/exp3_copula_model_risk.py
	python3 experiments/exp4_firesale_tipping_and_ms.py
	python3 experiments/exp5_agency_clawback_phase.py
	python3 experiments/exp6_platform_death_spiral.py
	python3 experiments/exp7_margin_paradox_and_supplychain.py
	python3 experiments/exp8_institution_drc_audit.py

clean:
	rm -rf $(BUILD_DIR)
