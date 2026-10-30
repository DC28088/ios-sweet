#!/usr/bin/env bash
# Compile/run the wire and storage-lifetime model on Linux, with no device access.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
mesa_include=${1:?Usage: check-sweet-native-transport.sh /path/to/mesa/include}
check_dir=$(mktemp -d)
trap 'rm -rf "$check_dir"' EXIT
"${CXX:-g++}" -std=c++17 -O1 -g -Wall -Wextra -Werror -pthread \
  '-DIOCPARM_LEN(x)=_IOC_SIZE(x)' \
  -I"$root/userspace/native-metal/tests/mock-sdk" \
  -I"$root/userspace/native-metal/transport" \
  -I"$root/userspace/gles/include" -I"$mesa_include" \
  "$root/userspace/native-metal/tests/transport_contract.cpp" -o "$check_dir/transport-contract"
"$check_dir/transport-contract"
