#!/usr/bin/env bash
set -euo pipefail

app=$1
output=$2
qt_root=${QT_ROOT_DIR:?QT_ROOT_DIR is required}
sql_drivers="$qt_root/plugins/sqldrivers"
sqlite_plugin="$sql_drivers/libqsqlite.dylib"
test -f "$sqlite_plugin"

stash=$(mktemp -d)
restore_plugins() {
  for plugin in "$stash"/*.dylib; do
    if [[ -f "$plugin" ]]; then mv "$plugin" "$sql_drivers/"; fi
  done
  rmdir "$stash"
}
trap restore_plugins EXIT

for plugin in "$sql_drivers"/libqsql*.dylib; do
  if [[ -f "$plugin" && "$plugin" != "$sqlite_plugin" ]]; then
    mv "$plugin" "$stash/"
  fi
done

app_dir=$(cd "$(dirname "$app")" && pwd -P)
app_name=$(basename "$app")
(
  cd "$app_dir"
  macdeployqt "$app_name" -always-overwrite -dmg
)

bundled_sqlite=$(find "$app_dir/$app_name/Contents" -name libqsqlite.dylib -print -quit)
test -n "$bundled_sqlite"
dmg=$(find "$app_dir" -maxdepth 1 -name '*.dmg' -print -quit)
test -n "$dmg"
mkdir -p "$(dirname "$output")"
mv "$dmg" "$output"
