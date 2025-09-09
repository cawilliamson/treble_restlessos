#!/usr/bin/env bash

# variables
ANDROID_VERSION="android-16.0"
AOSP_TAG="android-16.0.0_r2"
PATCHES_DIR="$(dirname "$(readlink -f -- "$0")")"
export ANDROID_VERSION AOSP_TAG PATCHES_DIR

# run if no arguments
if [ $# -eq 0 ]; then
	# clean existing trebledroid patches and start extraction
	rm -Rf $PATCHES_DIR/trebledroid $PATCHES_DIR/tmp
	mkdir -p $PATCHES_DIR/tmp
	pushd $PATCHES_DIR/tmp

		# initialize repo with aosp manifest
		repo init -u "https://android.googlesource.com/platform/manifest" -b $AOSP_TAG --depth=1

		# clone trebledroid manifest
		git clone https://github.com/TrebleDroid/treble_manifest .repo/local_manifests -b $ANDROID_VERSION

		# sync repositories
		while ! repo sync -c -j$(nproc --all) --force-sync; do
			sleep 30
		done

		# extract patches
		repo forall -j1 -c "bash $(readlink -f -- $0) extract"

	# clean up tmp directory
	popd
	rm -Rf $PATCHES_DIR/tmp
fi

# run if "extract" argument
if [ "$1" = "extract" ]; then
	# skip repos without trebledroid remote
	git remote get-url td 2>/dev/null || exit 0
	git fetch --unshallow td $REPO_RREV 2>/dev/null || true

	# get remote urls
	compact_remote="$(git remote get-url td|cut -d / -f 5)"
	original_remote=https://android.googlesource.com/"$(tr _ /  <<<$compact_remote)"

	# fetch from original aosp remote with retry logic
	while ! git fetch --tags $original_remote 2>/dev/null; do
		sleep 30
	done

	# generate patches
	lastTag="$(git describe --abbrev=0 --match=android-*)"
	
	if [ -n "$lastTag" ]; then
		patches_out=$PATCHES_DIR/trebledroid/$compact_remote/
		mkdir -p "$patches_out"
		git format-patch "$lastTag..HEAD" -o "$patches_out" 2>/dev/null || true
		
		# clean up empty directories
		if [ -z "$(ls -A "$patches_out" 2>/dev/null)" ]; then
			rmdir "$patches_out" 2>/dev/null || true
		fi
	fi
fi
