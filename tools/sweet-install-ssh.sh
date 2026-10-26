#!/bin/bash
# Install SSH (dropbear, public-key login only) + dash + toybox into the running iOS root through the
# kernel file server (xnu 0168, boot-arg sweetfiles=1, TCP 192.168.7.2:2223 over the USB cable).
# Adapted from ios-flame/sunfish tools/sunfish-install-ssh.sh.
# Binaries: the sweet-ssh workflow artifact (gh run download ID -n sweet-ssh-ios16 -D DIR).
# launchd reads its job cache at boot: reboot afterwards, then: ssh -i research/ssh/id_ed25519 root@192.168.7.2
#
#   tools/sweet-install-ssh.sh DIR
set -euo pipefail
cd "$(dirname "$0")/.."
B=${1:?usage: tools/sweet-install-ssh.sh DIR-with-dropbearmulti-dash[-toybox]}
F="python3 -I tools/sweet-files.py"
K=research/ssh            # git-ignored: the key and the launchd.plist copies stay local
[ -f "$B/dropbearmulti" ] && [ -f "$B/dash" ] || { echo "missing $B/dropbearmulti or $B/dash" >&2; exit 1; }
(cd "$B" && shasum -a 256 -c SHA256SUMS --ignore-missing)
mkdir -p $K
[ -f $K/id_ed25519 ] || ssh-keygen -q -t ed25519 -N '' -C ios-sweet -f $K/id_ed25519

$F mkdir /var/local 755
$F mkdir /var/local/bin 755
$F mkdir /private/etc/dropbear 700
$F mkdir /var/root/.ssh 700
$F put "$B/dropbearmulti" /var/local/bin/dropbearmulti 755
$F put "$B/dash" /var/local/bin/dash 755
for l in dropbear dropbearkey scp dbclient; do
	$F rm /var/local/bin/$l 2>/dev/null || true
	$F link /var/local/bin/dropbearmulti /var/local/bin/$l
done
$F link /var/local/bin/dash /var/local/bin/sh || true
$F link /var/local/bin/dash /bin/sh || echo "(/bin/sh exists)"
if [ -f "$B/toybox" ]; then
	$F put "$B/toybox" /var/local/bin/toybox 755
	for a in ls cat cp mv rm mkdir rmdir ln chmod chown ps kill grep sed head tail wc find xargs \
		sort uniq cut tr df du mount uname date env id whoami hexdump od dd sleep touch stat \
		which file tee printf echo true false test basename dirname readlink realpath uptime \
		free top hostname nc ping ifconfig netstat dmesg sync md5sum sha256sum tar gzip less vi; do
		$F link /var/local/bin/toybox /var/local/bin/$a >/dev/null 2>&1 || true
	done
fi
$F put $K/id_ed25519.pub /var/root/.ssh/authorized_keys 600
printf 'export PATH=/var/local/bin:/usr/bin:/bin:/usr/sbin:/sbin\nexport PS1="sweet# "\n' > $K/profile
$F put $K/profile /var/root/.profile 644

# launchd job cache: keep the original on the phone and here.
$F get /System/Library/xpc/launchd.plist $K/launchd.plist.cur
[ -f $K/launchd.plist.orig ] || cp $K/launchd.plist.cur $K/launchd.plist.orig
python3 -I - "$K/launchd.plist.cur" "$K/launchd.plist.new" <<'PY'
import plistlib, sys
p = plistlib.load(open(sys.argv[1], 'rb'))
p['LaunchDaemons']['/System/Library/LaunchDaemons/org.iossweet.dropbear.plist'] = {
    'Label': 'org.iossweet.dropbear',
    # -F foreground, -R host keys made on first connection; -p 192.168.7.2:22: the USB link only
    'ProgramArguments': ['/var/local/bin/dropbear', '-F', '-R', '-p', '192.168.7.2:22'],
    'RunAtLoad': True, 'KeepAlive': True, 'UserName': 'root',
    'EnvironmentVariables': {'PATH': '/var/local/bin:/usr/bin:/bin:/usr/sbin:/sbin'},
    'StandardErrorPath': '/var/local/dropbear.log',
}
plistlib.dump(p, open(sys.argv[2], 'wb'), fmt=plistlib.FMT_BINARY)
print('launchd jobs:', len(p['LaunchDaemons']))
PY
$F put $K/launchd.plist.cur /System/Library/xpc/launchd.plist.pre-ssh 644
$F put $K/launchd.plist.new /System/Library/xpc/launchd.plist 644
$F sync
echo "installed; reboot and boot again, then: ssh -i $K/id_ed25519 root@192.168.7.2"
