#!/usr/bin/env sh
verilator --trace -cc \
  soc/cpu.sv \
  soc/rf.sv soc/sm.sv soc/pc.sv soc/dec.sv soc/alu.sv soc/csr.sv soc/defs.vh \
  --timescale "1ns/1ns" \
  --no-timing \
  --exe soc/cpu_main.cpp soc/c_dpi.cpp \
&& make -C obj_dir -f Vcpu.mk Vcpu \
&& ./obj_dir/Vcpu "$@"
