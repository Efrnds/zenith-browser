#!/usr/bin/env bash
set -euo pipefail

case "${1:-base}" in
  base) extra=() ;;
  flatpak) extra=(flatpak flatpak-builder) ;;
  release) extra=(flatpak flatpak-builder file libfuse2t64) ;;
  *) echo "Unknown dependency set: $1" >&2; exit 2 ;;
esac

# The hosted runner's Azure mirror can stall for a long time on individual packages.
if [[ -f /etc/apt/apt-mirrors.txt ]]; then
  sudo sed -i 's@http://azure.archive.ubuntu.com/ubuntu@http://archive.ubuntu.com/ubuntu@g' /etc/apt/apt-mirrors.txt
fi

apt_options=(-o Acquire::Retries=1 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30)
sudo apt-get "${apt_options[@]}" update
sudo apt-get "${apt_options[@]}" install -y --no-install-recommends \
  cmake ninja-build pkg-config qt6-base-dev libqt6svg6-dev \
  libwebkit2gtk-4.1-dev libsecret-1-dev libx11-dev xvfb dbus-x11 \
  libxcb-cursor0 "${extra[@]}"
