#!/bin/bash

set -e

if [ -d /usr/lib/jvm/java-17-openjdk-amd64/bin/ ];then
export PATH=/usr/lib/jvm/java-17-openjdk-amd64/bin/:$PATH
fi

if [ -z "$ANDROID_HOME" ];then
    export ANDROID_HOME=$PWD/sdk
fi

gradleTarget=assembleDebug
target=debug
file=app-debug
if [ "$1" == "release" ];then
    gradleTarget=assembleRelease
    target=release
    file=app-release-unsigned
fi
./gradlew $gradleTarget
LD_LIBRARY_PATH=./signapk/ java -jar signapk/signapk.jar keys/platform.x509.pem keys/platform.pk8 ./app/build/outputs/apk/$target/${file}.apk TrebleApp.apk
