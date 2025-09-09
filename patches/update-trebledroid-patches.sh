#!/usr/bin/env bash

# variables
ANDROID_VERSION="android-16.0"
AOSP_TAG="android-16.0.0_r2"
PATCHES_DIR="$(dirname "$(readlink -f -- "$0")")"
export ANDROID_VERSION AOSP_TAG PATCHES_DIR

# run if no arguments
if [ $# -eq 0 ]; then
	echo "Starting TrebleDroid patches update..."
	echo "Android Version: $ANDROID_VERSION, AOSP Tag: $AOSP_TAG"
	
	# clean existing trebledroid patches and start extraction
	echo "Cleaning existing patches..."
	rm -Rfv $PATCHES_DIR/tmp $PATCHES_DIR/trebledroid
	mkdir -p $PATCHES_DIR/tmp $PATCHES_DIR/trebledroid
	
	pushd $PATCHES_DIR/tmp
		# initialize repo with aosp manifest
		echo "Initializing repo with AOSP manifest ($AOSP_TAG)..."
		repo init -u "https://android.googlesource.com/platform/manifest" -b $AOSP_TAG --depth=1

		# clone trebledroid manifest
		echo "Cloning TrebleDroid manifest ($ANDROID_VERSION)..."
		git clone https://github.com/TrebleDroid/treble_manifest .repo/local_manifests -b $ANDROID_VERSION

		# sync repositories
		echo "Syncing repositories..."
		while ! repo sync -c -j$(nproc --all) --force-sync; do
			echo "Sync failed, retrying in 30 seconds..."
			sleep 30
		done
		echo "Repository sync completed"

		# extract patches
		echo "Extracting patches from repositories..."
		repo forall -j1 -c "bash $(readlink -f -- $0) extract"
		echo "Patch extraction completed"
	popd
	
	echo "Cleaning up temporary directory..."
	rm -Rfv $PATCHES_DIR/tmp
	echo "TrebleDroid patches update completed. Patches saved to: $PATCHES_DIR/trebledroid/"
fi

# run if "extract" argument
if [ "$1" = "extract" ]; then
	# skip repos without trebledroid remote
	current_repo="$(pwd | sed "s|.*/||")"
	echo "Processing $current_repo..."
	
	if ! git remote get-url td 2>/dev/null; then
		echo "Skipping (no TrebleDroid remote)"
		exit 0
	fi
	
	echo "Found TrebleDroid remote, fetching..."
	git fetch --unshallow td $REPO_RREV

	# get remote urls
	compact_remote="$(git remote get-url td|cut -d / -f 5)"
	original_remote=https://android.googlesource.com/"$(tr _ /  <<<$compact_remote)"
	echo "AOSP remote: $original_remote"

	# fetch from original aosp remote with retry logic
	echo "Fetching tags from AOSP..."
	while ! git fetch --tags $original_remote 2>/dev/null; do
		echo "Fetch failed, retrying in 30 seconds..."
		sleep 30
	done
	echo "Fetch successful"

	# generate patches
	echo "Looking for Android tags..."
	lastTag="$(git describe --abbrev=0 --match=android-* 2>/dev/null || echo "")"
	
	if [ -n "$lastTag" ]; then
		echo "Found tag: $lastTag"
		patches_out=$PATCHES_DIR/trebledroid/$compact_remote/
		mkdir -p "$patches_out"
		git format-patch "$lastTag..HEAD" -o "$patches_out"
	else
		echo "No Android tag found, skipping patch generation"
	fi
	echo "Completed processing $current_repo"
fi
