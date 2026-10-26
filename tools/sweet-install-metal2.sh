#!/bin/bash
# Install a SweetMetal2.bundle build into the running iOS root through the kernel file server
# (xnu 0168, sweetfiles=1) and restart backboardd over SSH. Replaces only the files whose hash differs;
# each one is first saved from the phone into BACKUP, then removed (running processes keep the old
# inode) and written anew, so no mapped executable is rewritten in place.
#
#   tools/sweet-install-metal2.sh BUNDLE-DIR BACKUP-DIR
#   (BUNDLE-DIR is the artifact's SweetMetal2.bundle; undo: run it again with BACKUP-DIR as BUNDLE-DIR)
set -euo pipefail
cd "$(dirname "$0")/.."
SRC=${1:?usage: tools/sweet-install-metal2.sh BUNDLE-DIR BACKUP-DIR}
BAK=${2:?usage: tools/sweet-install-metal2.sh BUNDLE-DIR BACKUP-DIR}
F="python3 -I tools/sweet-files.py"
DST=/System/Library/Extensions/SweetMetal2.bundle
SSH="ssh -i research/ssh/id_ed25519 -o UserKnownHostsFile=research/ssh/known_hosts root@192.168.7.2"
mkdir -p "$BAK"
changed=0
for rel in SweetMetal2 libvulkan_freedreno.dylib Info.plist _CodeSignature/CodeResources; do
	[ -f "$SRC/$rel" ] || continue
	mkdir -p "$BAK/$(dirname "$rel")"
	$F get "$DST/$rel" "$BAK/$rel" >/dev/null
	if cmp -s "$SRC/$rel" "$BAK/$rel"; then
		echo "same: $rel"
		continue
	fi
	case "$rel" in SweetMetal2|*.dylib) mode=755 ;; *) mode=644 ;; esac
	$F rm "$DST/$rel" >/dev/null
	$F put "$SRC/$rel" "$DST/$rel" $mode >/dev/null
	$F get "$DST/$rel" "$BAK/.readback" >/dev/null
	cmp "$SRC/$rel" "$BAK/.readback"
	echo "installed: $rel $(sha256sum "$SRC/$rel" | cut -c1-16) (was $(sha256sum "$BAK/$rel" | cut -c1-16))"
	changed=1
done
rm -f "$BAK/.readback"
$F sync >/dev/null
[ $changed = 1 ] || { echo "nothing changed"; exit 0; }
timeout 20 $SSH 'ps -A -o pid,comm | while read p c; do case "$c" in */backboardd) echo "restart backboardd $p"; kill $p;; esac; done'
