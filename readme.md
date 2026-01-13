# RISC-V 32 CPU design

## Build/Run
To run the verilated SoC/CPU:
```txt
./build_run.sh

Usage:
  ./build_run.sh fast|slow  vsoc|vcpu|gold [trace <path>] [cycles] [memcmp] [verbose] [delay <cycles> <cycles>] [check] [timeout <cycles>] [seed <number>] bin|random
    fast|slow          | fast is -Os build, slow is -g -O0 build; default is slow
    vsoc|vcpu|gold     : select at least one to run: vsoc -- verilated SoC, vcpu -- verilated CPU, gold -- Golden Model
    [trace <path>]     : saves the trace of the run at <path> (only for vcpu and vsoc)
    [cycles]           : shows every 1'000'000 cycles
    [memcmp]           : compare full memory
    [verbose]          : verbosity level
      0 -- None, 1 -- Error, 2 -- Failed (default), 3 -- Warning, 4 -- Info
    [delay <cycles> <cycles>]   : vcpu random delay in [<cycles>, <cycles>) for memory read/write
    [check]            : on ebreak check a0 == 0, otherwise test failed
    [timeout <cycles>] : timeout after <cycles> cycles
    [seed <number>]    : set initial seed to <number>
    random <tests> <n_insts> <JBLSCE | all>: <tests> times random tests with <n_insts> <JBLSCE | all> instructions; conflicts with bin
      J -- jumps, B -- branches, L -- loads, S -- store, C -- calc, E -- system
    bin <path>               : loads the bin file to flash and runs it; conflicts with random
```

## Test
This does ./am-kernels/tests/cpu-tests/* and ./riscv-tests-am/*.
```txt
./test.sh
```

Testing

## Benchmarks

## Simple Architecture
```
+---+        +---+          +---+
|IFU|------->|IDU| -------> |LSU|
+---+<---+   +---+          +---+
         |      |             |
       +---+    |             |
       |EXU|<---+             |
       +---+<-----------------+
```

