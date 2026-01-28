CPU_TESTS=am-kernels/tests/cpu-tests

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

./build_run.sh fast "$CPU" gold random 1000 100 all delay 1 2 verbose 4
