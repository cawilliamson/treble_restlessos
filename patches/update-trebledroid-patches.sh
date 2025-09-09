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
		echo "Syncing repositories (using $(nproc --all) jobs, this may take 30+ minutes)..."
		sync_attempt=1
		while ! repo sync -c -j$(nproc --all) --force-sync; do
			echo "Sync attempt $sync_attempt failed, retrying in 30 seconds..."
			sleep 30
			((sync_attempt++))
		done
		echo "Repository sync completed after $sync_attempt attempt(s)"

		# extract patches
		echo "Extracting patches from repositories..."
		repo forall -j1 -c "bash $(readlink -f -- $0) extract"
		echo "Patch extraction completed"
	popd
	
	echo "Cleaning up temporary directory..."
	rm -Rf $PATCHES_DIR/tmp
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
	
	git fetch --unshallow td $REPO_RREV 2>/dev/null || true

	# get remote urls
	compact_remote="$(git remote get-url td|cut -d / -f 5)"
	original_remote=https://android.googlesource.com/"$(tr _ /  <<<$compact_remote)"

	# fetch from original aosp remote with retry logic
	echo "Fetching tags from AOSP..."
	fetch_attempt=1
	while ! git fetch --tags $original_remote 2>/dev/null; do
		echo "Fetch attempt $fetch_attempt failed, retrying in 30 seconds..."
		sleep 30
		((fetch_attempt++))
	done

	# generate patches
	lastTag="$(git describe --abbrev=0 --match=android-*)"
	
	if [ -n "$lastTag" ]; then
		patches_out=$PATCHES_DIR/trebledroid/$compact_remote/
		mkdir -p "$patches_out"
		
		patch_count=$(git rev-list --count "$lastTag..HEAD" 2>/dev/null || echo "0")
		if [ "$patch_count" -gt 0 ]; then
			git format-patch "$lastTag..HEAD" -o "$patches_out" 2>/dev/null || true
			actual_patches=$(ls -1 "$patches_out"/*.patch 2>/dev/null | wc -l || echo "0")
			echo "Generated $actual_patches patches"
		else
			echo "No patches to generate"
		fi
		
		# clean up empty directories
		if [ -z "$(ls -A "$patches_out" 2>/dev/null)" ]; then
			rmdir "$patches_out" 2>/dev/null || true
		fi
	fi
fi
