#!/bin/sh
set -eu
target="${1:-${XDG_DATA_HOME:-$HOME/.local/share}/gedit/plugins}"
rm -f -- "$target/libgit-status.so" "$target/git-status.plugin"
echo 'Git Status removed. Restart gedit if it is running.'
