#!/usr/bin/env bash
#
# Name: build.sh
# Description: Build the plugin, run its tests and make the Debian package.
#              The package ends up in the build directory.
# Usage: ./build.sh
#        BUILD_DIR=/some/dir ./build.sh    to build somewhere else than build/
# Author: Olivier Booklage
# Date: October 2026
# License: GPL-3.0-or-later
#
set -euo pipefail

# Debian wants the folders of a package in 0755: a umask of 0002 would make them 0775.
umask 022

# Packages to install before building, under their KDE neon names. Other
# distributions name them differently.
readonly BUILD_PACKAGES="g++ cmake extra-cmake-modules gettext dpkg-dev poppler-utils \
qt6-webengine-dev kf6-ktexteditor-dev kf6-kcoreaddons-dev kf6-ki18n-dev \
kf6-kconfig-dev kf6-kxmlgui-dev kf6-kwidgetsaddons-dev kf6-syntax-highlighting-dev"

# Programs the steps below call.
readonly BUILD_TOOLS="cmake cpack ctest g++ msgfmt dpkg-shlibdeps"

# The folder of this script, whatever folder it is called from.
source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-${source_dir}/build}"

# Stop with a message that says what went wrong.
fail() {
    echo "build.sh: $1" >&2
    exit 1
}

# Tell how to get what a failed step was missing, then stop.
fail_with_install_hint() {
    echo "build.sh: $1" >&2
    echo "Install the build packages, then run this script again:" >&2
    echo "  sudo apt install ${BUILD_PACKAGES}" >&2
    exit 1
}

check_tools() {
    local tool
    for tool in ${BUILD_TOOLS}; do
        if ! command -v "${tool}" > /dev/null; then
            fail_with_install_hint "the program '${tool}' is missing."
        fi
    done
}

configure() {
    # --fresh forgets what an earlier configuration found, so a build
    # directory left by another setup cannot mislead this one.
    if ! cmake --fresh -S "${source_dir}" -B "${build_dir}" \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_TESTING=ON; then
        fail_with_install_hint "CMake could not find everything it needs."
    fi
}

compile() {
    cmake --build "${build_dir}" --parallel "$(nproc)"
}

# A package is only made from a build whose tests pass. A bare ctest succeeds when it
# finds no test to run, hence --no-tests=error.
run_tests() {
    if ! ctest --test-dir "${build_dir}" --output-on-failure --no-tests=error; then
        fail "the tests failed; no package was made."
    fi
}

make_package() {
    # Packages of earlier runs would make it unclear which one is new.
    rm -f "${build_dir}"/*.deb
    (cd "${build_dir}" && cpack -G DEB)
}

show_result() {
    local package
    package="$(find "${build_dir}" -maxdepth 1 -name '*.deb' | head -n 1)"
    if [ -z "${package}" ]; then
        fail "cpack did not produce a package."
    fi
    echo
    echo "Package: ${package}"
    echo "Install it with: sudo apt install ${package}"
}

check_tools
configure
compile
run_tests
make_package
show_result
