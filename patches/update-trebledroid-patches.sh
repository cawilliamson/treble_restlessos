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
		echo "Script path: $PATCHES_DIR/update-trebledroid-patches.sh"
		ls -la "$PATCHES_DIR/update-trebledroid-patches.sh"
		
		# Add debugging: list all repositories that will be processed
		echo "=== DEBUG: Listing all repositories that will be processed ==="
		repo forall -c "echo 'REPO_DEBUG: Processing repository:' \$(pwd | sed 's|.*/||')"
		echo "=== DEBUG: End of repository list ==="
		
		echo "=== Starting patch extraction with detailed logging ==="
		repo forall -v -j1 -c "echo 'REPO_START: \$(pwd | sed \"s|.*/||\")' && bash -x $PATCHES_DIR/update-trebledroid-patches.sh extract && echo 'REPO_END: \$(pwd | sed \"s|.*/||\")'"
		echo "Patch extraction completed"
	popd
	
	echo "Cleaning up temporary directory..."
	rm -Rfv $PATCHES_DIR/tmp
	echo "TrebleDroid patches update completed. Patches saved to: $PATCHES_DIR/trebledroid/"
fi

# run if "extract" argument
if [ "$1" = "extract" ]; then
	# PATCHES_DIR is passed in via environment, don't recalculate it
	current_repo="$(pwd | sed "s|.*/||")"
	echo "Processing $current_repo..."
	echo "PATCHES_DIR=$PATCHES_DIR"
	echo "DEBUG: Current working directory: $(pwd)"
	echo "DEBUG: Checking for TrebleDroid remote..."
	
	if ! git remote get-url td 2>/dev/null; then
		echo "Skipping $current_repo (no TrebleDroid remote)"
		echo "DEBUG: Exiting extract mode for $current_repo"
		exit 0
	fi
	
	echo "Found TrebleDroid remote, fetching..."
	echo "DEBUG: About to fetch from TrebleDroid remote..."
	git fetch --unshallow td $REPO_RREV
	echo "DEBUG: TrebleDroid fetch completed"

	# get remote urls
	compact_remote="$(git remote get-url td|cut -d / -f 5)"
	original_remote=https://android.googlesource.com/"$(tr _ /  <<<$compact_remote)"
	echo "AOSP remote: $original_remote"

	# fetch from original aosp remote with retry logic
	echo "Fetching tags from AOSP..."
	echo "DEBUG: Starting AOSP fetch loop..."
	fetch_attempts=0
	while ! git fetch --tags $original_remote 2>/dev/null; do
		fetch_attempts=$((fetch_attempts + 1))
		echo "Fetch failed (attempt $fetch_attempts), retrying in 30 seconds..."
		echo "DEBUG: Fetch attempt $fetch_attempts failed, sleeping..."
		sleep 30
		if [ $fetch_attempts -gt 10 ]; then
			echo "ERROR: Too many fetch attempts, giving up on $current_repo"
			exit 1
		fi
	done
	echo "Fetch successful after $fetch_attempts attempts"
	echo "DEBUG: AOSP fetch completed successfully"

	# generate patches
	echo "Looking for Android tags..."
	echo "DEBUG: Running git describe to find Android tags..."
	lastTag="$(git describe --abbrev=0 --match=android-* 2>/dev/null || echo "")"
	echo "DEBUG: Found tag: '$lastTag'"
	
	if [ -n "$lastTag" ]; then
		echo "Found tag: $lastTag"
		patches_out=$PATCHES_DIR/trebledroid/$compact_remote/
		echo "DEBUG: Creating patches directory: $patches_out"
		mkdir -p "$patches_out"
		echo "DEBUG: Generating patches from $lastTag..HEAD"
		git format-patch "$lastTag..HEAD" -o "$patches_out"
		echo "DEBUG: Patch generation completed"
	else
		echo "No Android tag found, skipping patch generation"
	fi
	echo "Completed processing $current_repo"
	echo "DEBUG: Extract mode completed for $current_repo"
fi
