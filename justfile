# variables
ANDROID_VERSION := env_var_or_default("ANDROID_VERSION", "16.0.0")
ANDROID_VERSION_TAG := env_var_or_default("ANDROID_VERSION_TAG", "bp4a")
GRAPHENEOS_BRANCH := env_var_or_default("GRAPHENEOS_BRANCH", "16-qpr2")
BUILD_DATETIME := `date "+%s"`
BUILD_NUMBER := `date "+%Y%m%d%H%M"`
REPO_HOST := env_var_or_default("REPO_HOST", "https://github.com")
REPO_PATH := env_var_or_default("REPO_PATH", "cawilliamson/treble_grapheneos")
WEB_DIR := env_var_or_default("WEB_DIR", "/var/www/build.chrisaw.io")

# common container parameters
CONTAINER_RUN := "podman run --rm --privileged" + \
    " --pids-limit=0" + \
    " -v \"$(pwd):/repo:Z\"" + \
    " -v \"" + WEB_DIR + ":/web:Z\"" + \
    " -v \"$HOME/.ssh:/root/.ssh:Z\"" + \
    " -e ANDROID_VERSION=\"" + ANDROID_VERSION + "\"" + \
    " -e ANDROID_VERSION_TAG=\"" + ANDROID_VERSION_TAG + "\"" + \
    " -e GRAPHENEOS_BRANCH=\"" + GRAPHENEOS_BRANCH + "\"" + \
    " -e BUILD_DATETIME=\"" + BUILD_DATETIME + "\"" + \
    " -e BUILD_NUMBER=\"" + BUILD_NUMBER + "\""

# default target - runs the full build process
default: build-all

# clean all build directories to start fresh
clean:
    rm -rfv out/ src/ tmp/

# full build process - simple linear chain
build-all: build-container sync-grapheneos-sources apply-patches build-treble-app build-arm64 build-arm32 copy-to-webdir upload-to-github

# build the container image used for all build operations
build-container:
    podman build -t gsi-builder -f Containerfile .

# sync grapheneos sources with manifests (hard limit to 4 concurrent to avoid limiting)
sync-grapheneos-sources: build-container
    mkdir -p out/ src/ tmp/
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Syncing GrapheneOS sources..." && \
            repo init -u https://github.com/GrapheneOS/platform_manifest.git -b ${GRAPHENEOS_BRANCH} --depth=1 --git-lfs && \
            echo "${ANDROID_VERSION}" > /repo/tmp/.android_version && \
            echo "${ANDROID_VERSION_TAG}" > /repo/tmp/.android_version_tag && \
            mkdir -p .repo/local_manifests && \
            cp -v /repo/configs/manifests/*.xml .repo/local_manifests/ && \
            while ! repo sync -j4 --force-sync --no-clone-bundle --no-tags; do sleep 30; done'

# apply patches in correct order: trebledroid -> staging -> personal
apply-patches: build-container
    {{CONTAINER_RUN}} -w /repo/src gsi-builder \
        /bin/bash -e -c ' \
            echo "Applying patches..." && \
            cp -Rv /repo/patches . && \
            patches/apply.sh . trebledroid && \
            patches/apply.sh . staging && \
            patches/apply.sh . personal'

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
                cp -fv "/repo/configs/rom/grapheneos.mk" . && \
                bash generate.sh grapheneos && \
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
            bash vendor/cawilliamson-priv/keys/sign.sh && \
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
            VERSION_TAG="${ANDROID_VERSION}-{{BUILD_NUMBER}}" && \
            src="system_{{arch}}.img" && \
            dest="GrapheneOS-{{arch}}-ab-${VERSION_TAG}.img" && \
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
            VERSION_TAG="${ANDROID_VERSION}-{{BUILD_NUMBER}}"; \
            RELEASE_NAME="GrapheneOS-ab-${VERSION_TAG}"; \
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
        RELEASE_TAG="${ANDROID_VERSION}-{{BUILD_NUMBER}}" && \
        gh repo set-default "{{REPO_PATH}}" && \
        RELEASE_NAME="GrapheneOS-ab-${RELEASE_TAG}" && \
        RELEASE_DESCRIPTION="Download mirror: https://build.chrisaw.io/${RELEASE_NAME}/" && \
        gh release create -d -n "${RELEASE_DESCRIPTION}" -t "GrapheneOS ${RELEASE_TAG}" "${RELEASE_TAG}" && \
        gh release upload "${RELEASE_TAG}" --clobber -- *.img.xz && \
        rm -rf .git/'
