RISCV_TESTS=riscv-tests-am

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

cd $RISCV_TESTS
make ARCH=minirv-npc run cpu="$CPU" verbose=4
cd - >/dev/null
