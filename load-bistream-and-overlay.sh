#!/bin/bash

# exit on errors
set -e

if [[ "$1" == "-r" ]] ; then
  DEVICETREE_RELOAD=1
  shift
else
  DEVICETREE_RELOAD=0
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
BITSTREAM="$(realpath "${1:-$SCRIPT_DIR/default.bit}")"
OVERLAY="$(realpath "${2:-$SCRIPT_DIR/zcu104-uio-only.dtbo}")"
OVL_DIR=/sys/kernel/config/device-tree/overlays/full

if [ ! -e "$BITSTREAM" ] ; then
  echo "$BITSTREAM does not exist"
  exit 1
fi
if [ ! -e "$OVERLAY" ] ; then
  echo "$OVERLAY does not exist"
  exit 1
fi

echo "Loading bitstream $BITSTREAM..."
echo 0 > /sys/class/fpga_manager/fpga0/flags
ln -sf "$(realpath "$BITSTREAM")" /lib/firmware/bitstream.bit
echo bitstream.bit > /sys/class/fpga_manager/fpga0/firmware

if [ ! -d $OVL_DIR ] || [ $DEVICETREE_RELOAD -eq 1 ] ; then
  echo "Loading device tree overlay $OVERLAY"
  [ -d $OVL_DIR ] && rmdir $OVL_DIR
  mkdir /sys/kernel/config/device-tree/overlays/full
  ln -sf "$(realpath "$OVERLAY")" /lib/firmware/overlay.dtbo
  echo overlay.dtbo > $OVL_DIR/path
else
  echo "Device tree overlay already loaded at $OVL_DIR"
fi
