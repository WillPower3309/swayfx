#! /bin/bash

# shellcheck disable=SC2154

set -eE -u -o pipefail
shopt -s inherit_errexit

meson setup --prefix /opt "wlroots-${WLROOTSVERSION}/build/" "wlroots-${WLROOTSVERSION}"
ninja -C "wlroots-${WLROOTSVERSION}/build/"
ninja -C "wlroots-${WLROOTSVERSION}/build/" install
ls -l /opt/lib/x86_64-linux-gnu/pkgconfig
echo "wlroots DONE"

PKG_CONFIG_PATH="/opt/lib/x86_64-linux-gnu/pkgconfig/${PKG_CONFIG_PATH+:${PKG_CONFIG_PATH}}" \
meson setup --prefix /opt --cmake-prefix-path /opt "scenefx-${SCENEFXVERSION}/build/" "scenefx-${SCENEFXVERSION}"
ninja -C "scenefx-${SCENEFXVERSION}/build/"
ninja -C "scenefx-${SCENEFXVERSION}/build/" install
echo "scenefx DONE"

PKG_CONFIG_PATH="/opt/lib/x86_64-linux-gnu/pkgconfig/${PKG_CONFIG_PATH+:${PKG_CONFIG_PATH}}" \
meson setup --prefix /opt --cmake-prefix-path /opt "swayfx-${SWAYFXVERSION}/build/" "swayfx-${SWAYFXVERSION}"
ninja -C "swayfx-${SWAYFXVERSION}/build/"
ninja -C "swayfx-${SWAYFXVERSION}/build/" install
echo "swayfx DONE"

mv /opt/bin/sway /opt/bin/swayfx

cat >/opt/bin/swayfx.sh <<"EOF"
#!/bin/sh
export PATH=/opt/bin:${PATH}
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/opt/lib/x86_64-linux-gnu/
exec /opt/bin/swayfx
EOF

chmod 755 /opt/bin/swayfx.sh

echo
echo "Successfully build swayfx. Find it in your hosts bind directory (default: ./opt)."
