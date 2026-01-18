#!/usr/bin/env bash
set -euo pipefail

export VERILATOR_ROOT=/usr/share/verilator
RTL_ROOT=/home/bekzat/chip_bootcamp

DBG_CFLAGS="-g3 -O0 -fno-omit-frame-pointer"
DBG_LDFLAGS="-g"

usage() {
  echo "Usage:"
  echo "  $0 slow [testbench_args...]  #    debug build + run"
  echo "  $0 fast [testbench_args...]  # no debug build + run"
}

MODE="${1:-slow}"
shift || true
TB_ARGS=("$@")

case "$MODE" in
  slow)
    DEBUG_BUILD=1
    ;;
  fast)
    DEBUG_BUILD=0
    ;;
  *)
    usage
    exit 1
    ;;
esac

OBJ_CPU="obj_cpu_${MODE}"
OBJ_SOC="obj_soc_${MODE}"
TB_BIN="bin/testbench_${MODE}"

cd "$RTL_ROOT"

verilator --trace -cc \
  -Wall \
  -I"$RTL_ROOT/soc" \
  soc/cpu.sv \
  soc/rf.sv soc/pc.sv soc/exu.sv soc/idu.sv soc/alu.sv soc/csr.sv soc/com.sv soc/icache.sv \
  --timescale "1ns/1ns" \
  --no-timing \
  --Mdir "$OBJ_CPU"

verilator --trace -cc \
  -IysyxSoC/perip/uart16550/rtl \
  -IysyxSoC/perip/spi/rtl \
  -Isoc \
  $(find ysyxSoC/perip -type f -name '*.v') \
  $(find soc/ -type f -name '*.sv') \
  $(find soc/ -type f -name '*.vh') \
  ysyxSoC/ready-to-run/D-stage/ysyxSoCFull.v \
  --timescale "1ns/1ns" \
  --no-timing \
  --top-module ysyxSoCTop \
  --Mdir "$OBJ_SOC"

if [[ "$DEBUG_BUILD" -eq 1 ]]; then
  make -C "$OBJ_CPU" -f Vcpu.mk libVcpu.a \
    OPT_FAST="-O0 -g3 -fno-omit-frame-pointer" \
    OPT_SLOW="-O0 -g3 -fno-omit-frame-pointer"

  make -C "$OBJ_SOC" -f VysyxSoCTop.mk libVysyxSoCTop.a \
    OPT_FAST="-O0 -g3 -fno-omit-frame-pointer" \
    OPT_SLOW="-O0 -g3 -fno-omit-frame-pointer"
else
  # No OPT_FAST/OPT_SLOW overrides
  make -C "$OBJ_CPU" -f Vcpu.mk libVcpu.a
  make -C "$OBJ_SOC" -f VysyxSoCTop.mk libVysyxSoCTop.a
fi

g++ -std=c++17 -g \
  -I"$OBJ_CPU" -I"$OBJ_SOC" \
  -I"$VERILATOR_ROOT/include" \
  -I"$VERILATOR_ROOT/include/vltstd" \
  soc/soc_main.cpp \
  "$OBJ_SOC/libVysyxSoCTop.a" "$OBJ_CPU/libVcpu.a" \
  libverilated.a \
  -o "$TB_BIN"

cd - >/dev/null

"$RTL_ROOT/$TB_BIN" "${TB_ARGS[@]}"
