# variables
BUILD_NUMBER := `date "+%Y%m%d%H%M"`
BUILD_DATETIME := `date "+%s"`
REPO_HOST := env_var_or_default("REPO_HOST", "https://github.com")
REPO_PATH := env_var_or_default("REPO_PATH", "LeOS-LeanOS/treble_leanos")
WEB_DIR := env_var_or_default("WEB_DIR", "/var/www/build.chrisaw.io")

# common container parameters
CONTAINER_RUN := "podman run --rm --privileged" + \
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
build-all: clean build-container sync-sources apply-patches build-treble-app build-arm64 build-arm32 copy-to-webdir upload-to-github

# build the container image used for all build operations
build-container:
    podman build -t gsi-builder -f Containerfile .

# sync grapheneos sources with manifests
sync-sources: build-container
    mkdir -p out/ src/ tmp/
    echo "android-16.0" > tmp/.android_version
    echo "ap3a" > tmp/.android_version_tag
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Syncing grapheneos sources..." && \
            ANDROID_VERSION=$(cat /repo/tmp/.android_version) && \
            repo init -u https://android.googlesource.com/platform/manifest -b ${ANDROID_VERSION} --depth=1 --git-lfs && \
            mkdir -p .repo/local_manifests && \
            cp -v /repo/configs/local_manifests/*.xml .repo/local_manifests/ && \
            while ! repo sync -j$(nproc --all) --force-sync --no-clone-bundle --no-tags; do sleep 30; done'

# apply patches in correct order
apply-patches: build-container
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Applying patches..." && \
            cp -Rv /repo/patches/* patches/ && \
            patches/apply.sh . leanos'

# build treble app
build-treble-app: build-container
    {{CONTAINER_RUN}} -w /repo/src/treble_app gsi-builder \
        /bin/bash -e -c ' \
            echo "Building TrebleApp..." && \
            bash build.sh release \
        '

# build rom image for specific architecture
build-rom-image arch:
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Building ROM image..." && \
            ANDROID_VERSION_TAG_VAL=$(cat /repo/tmp/.android_version_tag) && \
            pushd device/phh/treble && \
                cp -fv "/repo/configs/rom/leanos.mk" . && \
                bash generate.sh leanos && \
            popd && \
            rm -rfv out/target/product/tdgsi_{{arch}}_ab/ && \
            . build/envsetup.sh && \
            lunch treble_{{arch}}_bvN-${ANDROID_VERSION_TAG_VAL}-userdebug && \
            make systemimage -j$(nproc --all) && \
            make target-files-package otatools -j$(nproc --all)'

# sign rom image for specific architecture
sign-rom-image arch:
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Signing ROM image..." && \
            ANDROID_VERSION_TAG_VAL=$(cat /repo/tmp/.android_version_tag) && \
            . build/envsetup.sh && \
            lunch treble_{{arch}}_bvN-${ANDROID_VERSION_TAG_VAL}-userdebug && \
            bash vendor/chrisaw-priv/keys/sign.sh && \
            rm -fv ${OUT}/system.img && \
            unzip -joq ${OUT}/signed-target_files.zip IMAGES/system.img -d ${OUT}/ && \
            rm -fv ${OUT}/signed-target_files.zip && \
            mv -v ${OUT}/system.img /repo/tmp/system_{{arch}}.img'

# compress rom image for specific architecture
compress-rom-image arch:
    {{CONTAINER_RUN}} -w /repo/tmp gsi-builder \
        /bin/bash -e -c ' \
            echo "Compressing ROM image..." && \
            ANDROID_VERSION=$(cat /repo/tmp/.android_version) && \
            VERSION_TAG="${ANDROID_VERSION#android-}-{{BUILD_NUMBER}}" && \
            src="system_{{arch}}.img" && \
            dest="LeanOS-{{arch}}-ab-${VERSION_TAG}.img" && \
            mv -v "${src}" "${dest}" && \
            xz -9 -T0 -v -z "${dest}" && \
            cp -fv "${dest}.xz" /repo/out/'

# build arm64 architecture
build-arm64:
    @just build-rom-image "arm64"
    @just sign-rom-image "arm64"
    @just compress-rom-image "arm64"

# build arm32_binder64 architecture
build-arm32:
    @just build-rom-image "a64"
    @just sign-rom-image "a64"
    @just compress-rom-image "a64"

# step 6: copy images to web directory
copy-to-webdir: build-container
    {{CONTAINER_RUN}} -w /repo/tmp gsi-builder \
        /bin/bash -e -c ' \
            echo "Copying to webdir..." && \
            ANDROID_VERSION=$(cat /repo/tmp/.android_version); \
            VERSION_TAG="${ANDROID_VERSION#android-}-{{BUILD_NUMBER}}"; \
            RELEASE_NAME="LeanOS-ab-${VERSION_TAG}"; \
            mkdir -p "/web/${RELEASE_NAME}" && \
            cp -fv *.img.xz "/web/${RELEASE_NAME}/" && \
            echo "Images copied to /web/${RELEASE_NAME}/"'

# step 7: upload images to github
upload-to-github:
    /usr/bin/env bash -c 'cd "$(pwd)/out/" && \
        echo "Uploading to GitHub..." && \
        git init && \
        git remote add origin "{{REPO_HOST}}/{{REPO_PATH}}.git" && \
        ANDROID_VERSION=$(cat "../tmp/.android_version") && \
        RELEASE_TAG="${ANDROID_VERSION#android-}-{{BUILD_NUMBER}}" && \
        gh repo set-default "{{REPO_PATH}}" && \
        RELEASE_NAME="LeanOS-ab-${RELEASE_TAG}" && \
        RELEASE_DESCRIPTION="Download mirror: https://build.chrisaw.io/${RELEASE_NAME}/" && \
        gh release create -d -n "${RELEASE_DESCRIPTION}" -t "LeanOS ${RELEASE_TAG}" "${RELEASE_TAG}" && \
        gh release upload "${RELEASE_TAG}" --clobber -- *.img.xz && \
        rm -rf .git/'
