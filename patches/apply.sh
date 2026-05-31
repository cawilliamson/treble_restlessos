#!/usr/bin/env bash

set -e

patches=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
tree="$2"

echo "Applying ${tree} patches:"
[ -d "$patches/$tree" ] || { echo "  (no patches for tier '$tree', skipping)"; exit 0; }

for project in $(cd "$patches"/"$tree"; echo *); do
    echo "> ${project}"
    p="$(tr _ / <<<"$project" |sed -e 's;platform/;;g')"
    [ "$p" == build ] && p=build/make
    [ "$p" == testing ] && p=platform_testing
    [ "$p" == treble/app ] && p=treble_app
    [ "$p" == vendor/hardware/overlay ] && p=vendor/hardware_overlay
    [ "$p" == vendor/partner/gms ] && p=vendor/partner_gms
    [ -d "$p" ] || { echo "  (directory $p not found, skipping)"; continue; }
    pushd "$p" &>/dev/null
    for patch in "$patches"/"$tree"/"$project"/*.patch; do
        echo ">> ${patch}"
        if git rev-parse --git-dir &>/dev/null; then
            git am "$patch" || { git am --abort; exit 1; }
        else
            patch -p1 < "$patch" || exit 1
        fi
    done
    popd &>/dev/null
done
