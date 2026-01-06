# --build --runtime-debug -CFLAGS "-g3 -O0 -fno-omit-frame-pointer" -LDFLAGS "-g" \
export VERILATOR_ROOT=/usr/share/verilator

verilator --trace -cc \
  -Isoc \
  soc/cpu.sv \
  soc/rf.sv soc/sm.sv soc/pc.sv soc/dec.sv soc/alu.sv soc/csr.sv soc/com.sv soc/defs.vh \
  --timescale "1ns/1ns" \
  --no-timing \
  --Mdir obj_cpu

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
  --Mdir obj_soc

make -C obj_cpu -f Vcpu.mk libVcpu.a
make -C obj_soc -f VysyxSoCTop.mk libVysyxSoCTop.a libverilated.a

g++ -std=c++17 \
  -Iobj_cpu -Iobj_soc \
  -I$VERILATOR_ROOT/include \
  -I"$VERILATOR_ROOT/include/vltstd" \
  soc/soc_main.cpp soc/c_dpi.cpp \
  obj_soc/libVysyxSoCTop.a obj_cpu/libVcpu.a \
  obj_soc/libverilated.a \
  -o bin/testbench

./bin/testbench "$@"
