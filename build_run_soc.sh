# --build --runtime-debug -CFLAGS "-g3 -O0 -fno-omit-frame-pointer" -LDFLAGS "-g" \
export VERILATOR_ROOT=/usr/share/verilator
RTL_ROOT=/home/bekzat/chip_bootcamp/

# OPT_FAST="-O0 -g3 -fno-omit-frame-pointer" \
# OPT_SLOW="-O0 -g3 -fno-omit-frame-pointer" \
DBG_CFLAGS="-g3 -O0 -fno-omit-frame-pointer"
DBG_LDFLAGS="-g"

cd $RTL_ROOT
verilator --trace -cc \
  -Wall \
  -I$RTL_ROOT/soc \
  soc/cpu.sv \
  soc/rf.sv soc/pc.sv soc/exu.sv soc/idu.sv soc/alu.sv soc/csr.sv soc/com.sv \
  --timescale "1ns/1ns" \
  --no-timing \
  --Mdir obj_cpu \
&& \
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
  --Mdir obj_soc \
&& \
make -C obj_cpu -f Vcpu.mk libVcpu.a \
&& \
make -C obj_soc -f VysyxSoCTop.mk libVysyxSoCTop.a \
&& \
g++ -std=c++17 -g \
  -Iobj_cpu -Iobj_soc \
  -I$VERILATOR_ROOT/include \
  -I"$VERILATOR_ROOT/include/vltstd" \
  soc/soc_main.cpp\
  obj_soc/libVysyxSoCTop.a obj_cpu/libVcpu.a \
  libverilated.a \
  -o bin/testbench \
&& \
cd - \
&& \
$RTL_ROOT/bin/testbench "$@"
