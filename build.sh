#! /bin/bash

# shellcheck disable=SC2154

set -eE -u -o pipefail
shopt -s inherit_errexit

DEB_BUILD_GNU_TYPE=$(dpkg-architecture -q DEB_BUILD_GNU_TYPE)

meson setup --prefix ${OPT_SWAYFX} "wlroots-${WLROOTSVERSION}/build/" "wlroots-${WLROOTSVERSION}"
ninja -C "wlroots-${WLROOTSVERSION}/build/"
ninja -C "wlroots-${WLROOTSVERSION}/build/" install
ls -l ${OPT_SWAYFX}/lib/${DEB_BUILD_GNU_TYPE}/pkgconfig
echo "wlroots DONE"

PKG_CONFIG_PATH="${OPT_SWAYFX}/lib/${DEB_BUILD_GNU_TYPE}/pkgconfig/${PKG_CONFIG_PATH+:${PKG_CONFIG_PATH}}" \
meson setup --prefix ${OPT_SWAYFX} --cmake-prefix-path ${OPT_SWAYFX} "scenefx-${SCENEFXVERSION}/build/" "scenefx-${SCENEFXVERSION}"
ninja -C "scenefx-${SCENEFXVERSION}/build/"
ninja -C "scenefx-${SCENEFXVERSION}/build/" install
echo "scenefx DONE"

PKG_CONFIG_PATH="${OPT_SWAYFX}/lib/${DEB_BUILD_GNU_TYPE}/pkgconfig/${PKG_CONFIG_PATH+:${PKG_CONFIG_PATH}}" \
meson setup --prefix ${OPT_SWAYFX} --cmake-prefix-path ${OPT_SWAYFX} "swayfx-${SWAYFXVERSION}/build/" "swayfx-${SWAYFXVERSION}"
ninja -C "swayfx-${SWAYFXVERSION}/build/"
ninja -C "swayfx-${SWAYFXVERSION}/build/" install
echo "swayfx DONE"

mv ${OPT_SWAYFX}/bin/sway ${OPT_SWAYFX}/bin/swayfx

cat >${OPT_SWAYFX}/bin/swayfx.sh <<EOF
#!/bin/sh
export PATH=${OPT_SWAYFX}/bin:\${PATH}
export LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}:${OPT_SWAYFX}/lib/${DEB_BUILD_GNU_TYPE}/
exec ${OPT_SWAYFX}/bin/swayfx
EOF

chmod 755 ${OPT_SWAYFX}/bin/swayfx.sh

echo
echo "Successfully build swayfx. Find it in your hosts bind directory (default: .${OPT_SWAYFX})."
