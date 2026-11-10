#!/usr/bin/env bash
# Builds and runs the Unreal-free interface tests with the system C++ compiler.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
pub="$here/../../../GranTheftPraline/Source/GranTheftPraline/Public"
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -I "$pub" "$here/cpp/ui_core_tests.cpp" -o "$out/ui_core_tests"
"$out/ui_core_tests"
