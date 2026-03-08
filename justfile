# variables
GRAPHENEOS_RELEASE_CHANNEL := env_var_or_default("GRAPHENEOS_RELEASE_CHANNEL", "stable")
GRAPHENEOS_TAG := env_var_or_default("GRAPHENEOS_TAG", "")
GRAPHENEOS_BRANCH := env_var_or_default("GRAPHENEOS_BRANCH", "")
BUILD_DATETIME := `date "+%s"`
BUILD_NUMBER := `date "+%Y%m%d%H%M"`
REPO_HOST := env_var_or_default("REPO_HOST", "https://github.com")
REPO_PATH := env_var_or_default("REPO_PATH", "cawilliamson/treble_grapheneos")
WEB_DIR := env_var_or_default("WEB_DIR", "/var/www/build.chrisaw.io")

# common container parameters
CONTAINER_RUN := "podman run --rm" + \
    " --pids-limit=0" + \
    " -v \"$(pwd):/repo:Z\"" + \
    " -v \"" + WEB_DIR + ":/web:Z\"" + \
    " -v \"$HOME/.ssh:/root/.ssh:Z\"" + \
    " -e BUILD_DATETIME=\"" + BUILD_DATETIME + "\"" + \
    " -e BUILD_NUMBER=\"" + BUILD_NUMBER + "\""

# default target - runs the full build process
default: build-all

# clean all build directories to start fresh
clean:
    rm -rfv out/ src/ tmp/

# full build process - simple linear chain
build-all: build-container resolve-grapheneos-tag sync-grapheneos-sources apply-patches build-treble-app build-rom sign-rom compress-rom

# resolve which grapheneos tag to build from
# priority: GRAPHENEOS_TAG env var > auto-detect from releases API > GRAPHENEOS_BRANCH env var
resolve-grapheneos-tag:
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p tmp/
    # clean stale state from previous runs so incremental builds
    # never accidentally reuse metadata from an earlier build
    rm -f tmp/.grapheneos_tag tmp/.grapheneos_branch
    rm -f tmp/.android_version tmp/.android_version_tag tmp/.build_number
    # persist fresh build identifiers for this run
    echo "{{BUILD_DATETIME}}" > tmp/.build_datetime
    echo "{{BUILD_NUMBER}}" > tmp/.build_number
    if [[ -n "{{GRAPHENEOS_TAG}}" ]]; then
        echo "Using manually specified tag: {{GRAPHENEOS_TAG}}"
        echo "{{GRAPHENEOS_TAG}}" > tmp/.grapheneos_tag
    elif [[ -n "{{GRAPHENEOS_BRANCH}}" ]]; then
        echo "WARNING: Building from branch '{{GRAPHENEOS_BRANCH}}' (HEAD) - not a stable release!"
        echo "Set GRAPHENEOS_TAG or unset GRAPHENEOS_BRANCH to auto-detect the latest stable tag."
        echo "{{GRAPHENEOS_BRANCH}}" > tmp/.grapheneos_branch
    else
        CHANNEL="{{GRAPHENEOS_RELEASE_CHANNEL}}"
        echo "Fetching latest ${CHANNEL} GrapheneOS tag from releases page..."
        # Extract tags directly from the releases page HTML. Each device/channel row
        # has the structure: id={device}-{channel}><td>...<a href=#{tag}>{tag}</a>
        # We grab all tags for the requested channel and take the highest one.
        RELEASES_HTML=$(curl -sf "https://grapheneos.org/releases")
        if [[ -z "${RELEASES_HTML}" ]]; then
            echo "ERROR: Failed to fetch https://grapheneos.org/releases"
            exit 1
        fi
        TAG=$(echo "${RELEASES_HTML}" \
            | grep -oP "id=[a-z]+-${CHANNEL}><td>[^<]+</td><td><a href=#\K[0-9]{10}" \
            | sort -rn \
            | head -1)
        if [[ -z "${TAG}" ]]; then
            echo "ERROR: Failed to extract a valid ${CHANNEL} tag from releases page"
            exit 1
        fi
        echo "Resolved latest ${CHANNEL} tag: ${TAG}"
        echo "${TAG}" > tmp/.grapheneos_tag
    fi

# build the container image used for all build operations
build-container:
    podman build -t gsi-builder -f Containerfile .

# sync grapheneos sources with manifests (hard limit to 4 concurrent to avoid limiting)
sync-grapheneos-sources: build-container
    mkdir -p out/ src/ tmp/
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Syncing GrapheneOS sources..." && \
            GRAPHENEOS_TAG=$(cat /repo/tmp/.grapheneos_tag 2>/dev/null || true) && \
            GRAPHENEOS_BRANCH=$(cat /repo/tmp/.grapheneos_branch 2>/dev/null || true) && \
            if [[ -n "${GRAPHENEOS_TAG}" ]]; then \
                echo "Initializing from stable tag: ${GRAPHENEOS_TAG}" && \
                repo init -u https://github.com/GrapheneOS/platform_manifest.git -b "refs/tags/${GRAPHENEOS_TAG}" --depth=1 --git-lfs; \
            elif [[ -n "${GRAPHENEOS_BRANCH}" ]]; then \
                echo "Initializing from branch: ${GRAPHENEOS_BRANCH}" && \
                repo init -u https://github.com/GrapheneOS/platform_manifest.git -b "${GRAPHENEOS_BRANCH}" --depth=1 --git-lfs; \
            else \
                echo "ERROR: No tag or branch resolved. Run resolve-grapheneos-tag first." && exit 1; \
            fi && \
            mkdir -p .repo/local_manifests && \
            cp -v /repo/configs/manifests/*.xml .repo/local_manifests/ && \
            while ! repo sync -j4 --force-sync --no-clone-bundle --no-tags; do sleep 30; done && \
            echo "Extracting ANDROID_VERSION from source tree..." && \
            AOSP_TAG=$(grep -m1 "aosp_revision:" .repo/manifests/config.yml | sed "s/.*: *//") && \
            ANDROID_VERSION=$(echo "${AOSP_TAG}" | sed "s/android-//;s/_r.*//") && \
            echo "Detected ANDROID_VERSION: ${ANDROID_VERSION}" && \
            echo "${ANDROID_VERSION}" > /repo/tmp/.android_version && \
            echo "Extracting ANDROID_VERSION_TAG from source tree..." && \
            ANDROID_VERSION_TAG=$(grep -m1 "target:" build/release/release_config_map.textproto | sed "s/.*\"\([^\"]*\)\".*/\1/") && \
            echo "Detected ANDROID_VERSION_TAG: ${ANDROID_VERSION_TAG}" && \
            echo "${ANDROID_VERSION_TAG}" > /repo/tmp/.android_version_tag'

# apply patches in correct order: trebledroid -> staging -> grapheneos -> personal
apply-patches: build-container
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Applying patches..." && \
            rm -rf patches/ && \
            cp -Rv /repo/patches . && \
            patches/apply.sh . trebledroid && \
            patches/apply.sh . staging && \
            patches/apply.sh . grapheneos && \
            patches/apply.sh . personal'

# build treble app
build-treble-app: build-container
    {{CONTAINER_RUN}} -w /repo/src/treble_app gsi-builder \
        /bin/bash -e -c ' \
            echo "Building TrebleApp..." && \
            bash build.sh release \
        '

# build arm64 rom image
build-rom: build-container
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Building ROM image..." && \
            ANDROID_VERSION_TAG_VAL=$(cat /repo/tmp/.android_version_tag) && \
            pushd device/phh/treble && \
                cp -fv "/repo/configs/rom/grapheneos.mk" . && \
                bash generate.sh grapheneos && \
            popd && \
            rm -rfv out/.lock out/soong/.intermediates/prebuilts/ out/target/product/tdgsi_arm64_ab/ && \
            rm -fv /repo/tmp/*.img* /repo/out/*.img* && \
            . build/envsetup.sh && \
            lunch treble_arm64_bvN-${ANDROID_VERSION_TAG_VAL}-userdebug && \
            make systemimage -j$(nproc --all) && \
            make target-files-package otatools -j$(nproc --all)'

# sign arm64 rom image
sign-rom: build-container
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Signing ROM image..." && \
            ANDROID_VERSION_TAG_VAL=$(cat /repo/tmp/.android_version_tag) && \
            . build/envsetup.sh && \
            lunch treble_arm64_bvN-${ANDROID_VERSION_TAG_VAL}-userdebug && \
            bash vendor/cawilliamson-priv/keys/sign.sh && \
            rm -fv ${OUT}/system.img && \
            unzip -joq ${OUT}/signed-target_files.zip IMAGES/system.img -d ${OUT}/ && \
            rm -fv ${OUT}/signed-target_files.zip && \
            mv -v ${OUT}/system.img /repo/tmp/system.img'

# compress arm64 rom image
compress-rom: build-container
    {{CONTAINER_RUN}} -w /repo/tmp gsi-builder \
        /bin/bash -e -c ' \
            echo "Compressing ROM image..." && \
            ANDROID_VERSION=$(cat /repo/tmp/.android_version) && \
            BUILD_NUM=$(cat /repo/tmp/.build_number) && \
            VERSION_TAG="${ANDROID_VERSION}-${BUILD_NUM}" && \
            src="system.img" && \
            dest="GrapheneOS-arm64-ab-${VERSION_TAG}.img" && \
            mv -v "${src}" "${dest}" && \
            xz -9 -T0 -v -z "${dest}" && \
            cp -fv "${dest}.xz" /repo/out/'

# copy images to web directory
copy-to-webdir: build-container
    {{CONTAINER_RUN}} -w /repo/tmp gsi-builder \
        /bin/bash -e -c ' \
            echo "Copying to webdir..." && \
            ANDROID_VERSION=$(cat /repo/tmp/.android_version) && \
            BUILD_NUM=$(cat /repo/tmp/.build_number) && \
            VERSION_TAG="${ANDROID_VERSION}-${BUILD_NUM}" && \
            RELEASE_NAME="GrapheneOS-ab-${VERSION_TAG}" && \
            mkdir -p "/web/${RELEASE_NAME}" && \
            cp -fv /repo/out/*.img.xz "/web/${RELEASE_NAME}/" && \
            echo "Images copied to /web/${RELEASE_NAME}/"'

# upload images to github
upload-to-github:
    /usr/bin/env bash -c 'cd "$(pwd)/out/" && \
        echo "Uploading to GitHub..." && \
        git init && \
        git remote add origin "{{REPO_HOST}}/{{REPO_PATH}}.git" && \
        ANDROID_VERSION=$(cat "../tmp/.android_version") && \
        BUILD_NUM=$(cat "../tmp/.build_number") && \
        RELEASE_TAG="${ANDROID_VERSION}-${BUILD_NUM}" && \
        gh repo set-default "{{REPO_PATH}}" && \
        RELEASE_NAME="GrapheneOS-ab-${RELEASE_TAG}" && \
        RELEASE_DESCRIPTION="Download mirror: https://build.chrisaw.io/${RELEASE_NAME}/" && \
        gh release create -d -n "${RELEASE_DESCRIPTION}" -t "GrapheneOS ${RELEASE_TAG}" "${RELEASE_TAG}" && \
        gh release upload "${RELEASE_TAG}" --clobber -- *.img.xz && \
        rm -rf .git/'
