#!/usr/bin/env bash

# variables
ANDROID_VERSION="android-16.0"
AOSP_TAG="android-16.0.0_r2"
export ANDROID_VERSION AOSP_TAG

# set PATCHES_DIR only once
if [ -z "$PATCHES_DIR" ]; then
	PATCHES_DIR="$(dirname "$(readlink -f -- "$0")")"
	export PATCHES_DIR
fi

# run if no arguments
if [ $# -eq 0 ]; then
	echo "Starting TrebleDroid patches update..."
	echo "Android Version: $ANDROID_VERSION, AOSP Tag: $AOSP_TAG"

	# clean existing trebledroid patches and start extraction
	echo "Cleaning existing patches..."
	rm -Rfv "$PATCHES_DIR/tmp" "$PATCHES_DIR/trebledroid"
	mkdir -p "$PATCHES_DIR/tmp" "$PATCHES_DIR/trebledroid"

	pushd "$PATCHES_DIR/tmp" || exit 1
		# initialize repo with aosp manifest
		echo "Initializing repo with AOSP manifest ($AOSP_TAG)..."
		repo init -u "https://android.googlesource.com/platform/manifest" -b "$AOSP_TAG" --depth=1

		# clone trebledroid manifest
		echo "Cloning TrebleDroid manifest ($ANDROID_VERSION)..."
		git clone https://github.com/TrebleDroid/treble_manifest .repo/local_manifests -b "$ANDROID_VERSION"

		# sync repositories
		echo "Syncing repositories (limit to 4 concurrent to avoid rate limiting)..."
		while ! repo sync -c -j4 --force-sync; do
			echo "Sync failed, retrying in 30 seconds..."
			sleep 30
		done
		echo "Repository sync completed"

		# extract patches
		echo "Extracting patches from repositories..."
		repo forall -j1 -c "bash $PATCHES_DIR/update-trebledroid-patches.sh extract"
		echo "Patch extraction completed"
	popd || exit 1

	echo "Cleaning up temporary directory..."
	rm -Rfv "$PATCHES_DIR/tmp"
	echo "TrebleDroid patches update completed. Patches saved to: $PATCHES_DIR/trebledroid/"
fi

# run if "extract" argument
if [ "$1" = "extract" ]; then
	# PATCHES_DIR is passed in via environment, don't recalculate it
	current_repo="$(pwd | sed "s|.*/||")"

	if ! git remote get-url td 2>/dev/null; then
		echo "Skipping $current_repo (no TrebleDroid remote)"
		exit 0
	fi

	echo "Processing $current_repo..."
	git fetch --unshallow td "$REPO_RREV"

	# get remote urls
	compact_remote="$(git remote get-url td|cut -d / -f 5)"
	original_remote=https://android.googlesource.com/"$(tr _ / <<<"$compact_remote")"

	# fetch from original aosp remote with retry logic
	echo "Fetching tags from AOSP..."
	fetch_attempts=0
	while ! git fetch --tags "$original_remote" 2>/dev/null; do
		fetch_attempts=$((fetch_attempts + 1))
		echo "Fetch failed (attempt $fetch_attempts), retrying in 30 seconds..."
		sleep 30
		if [ $fetch_attempts -gt 10 ]; then
			echo "ERROR: Too many fetch attempts, giving up on $current_repo"
			exit 1
		fi
	done

	# generate patches
	lastTag="$(git describe --abbrev=0 --match=android-* 2>/dev/null || echo "")"

	if [ -n "$lastTag" ]; then
		echo "Found tag: $lastTag"
		patches_out="$PATCHES_DIR/trebledroid/$compact_remote/"
		mkdir -p "$patches_out"
		git format-patch "$lastTag..HEAD" -o "$patches_out"
	else
		echo "No Android tag found, skipping patch generation"
	fi
	echo "Completed processing $current_repo"
fi
