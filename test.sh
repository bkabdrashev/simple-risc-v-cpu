CPU_TESTS=/home/bekzat/chip_bootcamp/am-kernels/tests/cpu-tests
RISCV_TESTS=/home/bekzat/chip_bootcamp/riscv-tests-am

usage() {
  echo "Usge:"
  echo "  $0 vsoc"
  echo "  $0 vcpu"
}

CPU="${1:-vsoc}"
shift || true

case "$CPU" in
  vsoc)
    ;;
  vcpu)
    ;;
  *)
    usage
    exit 1
    ;;
esac

cd $CPU_TESTS
make ARCH=minirv-npc run cpu="$CPU" verbose=4
cd - >/dev/null

cd $RISCV_TESTS
make ARCH=minirv-npc run cpu="$CPU" verbose=4
cd - >/dev/null
