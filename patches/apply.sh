#!/usr/bin/env bash

set -e

patches=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
tree="$2"
# snapshot root: used as a git discovery ceiling so `git apply --no-index` in
# vendored snapshots never resolves paths against a parent work tree
srcroot=$(pwd)

echo "Applying ${tree} patches:"
[ -d "$patches/$tree" ] || {
        echo "  (no patches for tier '$tree', skipping)"
        exit 0
}

for project in $(
        cd "$patches"/"$tree"
        echo *
); do
        echo "> ${project}"
        p="$(tr _ / <<<"$project" | sed -e 's;platform/;;g')"
        [ "$p" == build ] && p=build/make
        [ "$p" == testing ] && p=platform_testing
        [ "$p" == treble/app ] && p=treble_app
        [ "$p" == system/fs/fs/mgr ] && p=system/fs/fs_mgr
        [ "$p" == vendor/hardware/overlay ] && p=vendor/hardware_overlay
        [ "$p" == vendor/partner/gms ] && p=vendor/partner_gms
        pushd "$p" &>/dev/null
        for patch in "$patches"/"$tree"/"$project"/*.patch; do
                echo ">> ${patch}"
                if test -d .git; then
                        git am "$patch" || {
                                git am --abort
                                exit 1
                        }
                else
                        # vendored snapshots under upstream/ have no .git; git apply handles binary hunks.
                        # --no-index plus a ceiling at the snapshot dir prevents git from discovering a
                        # parent work tree (e.g. when src/ lives inside this repo locally), where path
                        # resolution would silently skip patches
                        GIT_CEILING_DIRECTORIES="$srcroot" git apply --no-index -p1 "$patch" || exit 1
                fi
        done
        popd &>/dev/null
done
