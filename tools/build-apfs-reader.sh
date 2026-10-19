#!/bin/bash
# Build an APFS reader with LZBITMAP support locally, without installing it.
set -euo pipefail
cd "$(dirname "$0")/.."
repo=$PWD
src="$repo/research/apfs-fuse-current"
revision=66b86bd525e8cb90f9012543be89b1f092b75cf3
for tool in git cmake gcc g++ make; do
	command -v "$tool" >/dev/null || {
		echo "Missing $tool. On Fedora, install the build dependencies:" >&2
		echo 'sudo dnf install cmake gcc-c++ fuse3-devel zlib-ng-compat-devel bzip2-devel' >&2
		exit 1
	}
done
if [[ ! -e "$src" ]]; then
	git clone --recursive https://github.com/sgan81/apfs-fuse.git "$src"
	git -C "$src" checkout --detach "$revision"
	git -C "$src" submodule update --init --recursive
fi
[[ $(git -C "$src" rev-parse HEAD) == "$revision" ]] || {
	echo "Unexpected APFS source revision at $src; preserving it." >&2; exit 1;
}
cmake -S "$src" -B "$src/build" \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_CXX_FLAGS='-include cstdint'
cmake --build "$src/build" --target apfs-fuse -j "${JOBS:-2}"
printf 'APFS reader built: %s\n' "$src/build/apfs-fuse"
printf 'Use APFS_FUSE=%q with tools/mkiosroot.sh\n' "$src/build/apfs-fuse"
