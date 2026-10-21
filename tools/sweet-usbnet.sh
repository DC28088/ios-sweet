#!/bin/sh
# Capture the sweet USB Ethernet kernel console. NetworkManager configures only
# the phone's fixed MAC; no sudo is needed if the desktop NM policy permits it.
# Usage: tools/sweet-usbnet.sh [LOGFILE] [--duration SECONDS]
exec python3 "$(dirname "$0")/sweet-usb-log.py" "$@"
