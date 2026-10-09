#!/bin/sh
set -eu
cd "$(dirname "$0")"
target="${1:-${XDG_DATA_HOME:-$HOME/.local/share}/gedit/plugins}"
test -f build/libgit-status.so || { echo 'Run ./build.sh first.' >&2; exit 1; }
mkdir -p "$target"
install -m 644 build/libgit-status.so build/git-status.plugin "$target/"
echo "Installed to $target. Restart gedit, then enable File Browser and Git Status in Preferences → Plugins."
