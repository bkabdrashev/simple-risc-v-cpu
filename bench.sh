MICROBENCH_PATH=/home/bekzat/chip_bootcamp/am-kernels/benchmarks/microbench

usage() {
  echo "Usage:"
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

cd $MICROBENCH_PATH
make ARCH=minirv-npc run verbose=4 cpu="$CPU" mainargs=train
cd - >/dev/null
