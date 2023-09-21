#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

while getopts v:p: flag
do
    case "${flag}" in
        v) PROJECT_VERSION=${OPTARG};;
        p) PLATFORMS=${OPTARG};;
    esac
done

die () {
    echo >&2 "$@"
    exit 1
}

apple () {
    echo Creating apple release...
    APPLE_DIR=${SCRIPT_DIR}/build/apple
    cd ${APPLE_DIR}
    rm -rf Based.xcframework
    xcodebuild -create-xcframework \
        -framework catalyst/Release/based.framework \
        -framework ios/Release-iphoneos/based.framework \
        -framework ios-sim/Release-iphonesimulator/based.framework \
        -framework darwin/Release/based.framework \
        -output Based.xcframework
    echo zipping...
    zip -rq based-universal-v${PROJECT_VERSION}-xcframework.zip Based.xcframework
}

android () {
    ANDROID_TARGETS="armeabi-v7a arm64-v8a x86 x86_64"
    ANDROID_DIR=${SCRIPT_DIR}/build/android
    echo Creating android release...
    cd ${ANDROID_DIR}
    HEADER_FILE=${SCRIPT_DIR}/include/based.h
    rm -rf ${SCRIPT_DIR}/build/android/release

    for TARGET in ${ANDROID_TARGETS}
    do
        TARGET_DIR=${SCRIPT_DIR}/build/android/release/${TARGET}
        BUILD_DIR=${SCRIPT_DIR}/build/android/${TARGET}
        mkdir -p ${TARGET_DIR}
        cp ${HEADER_FILE} ${TARGET_DIR}/based.h
        cp ${BUILD_DIR}/libbased.so ${TARGET_DIR}/libbased.so
    done

    cd ${SCRIPT_DIR}/build/android
    echo zipping...
    zip -rq based-universal-v${PROJECT_VERSION}-android.zip release
}

linux () {
    echo Creating linux release...

    cd ${SCRIPT_DIR}/build/linux
    echo zipping...
    zip -rq based-universal-v${PROJECT_VERSION}-linux.zip libbased.so based.h
}


[ ! -z ${PROJECT_VERSION+x} ] || die "Please provide project version with flag -v"


if [ ! -z ${PLATFORMS+x} ];
    then
        if [[ ! " ${PLATFORMS[*]} " =~ " android " ]];
            then
                echo "Skipping android";
            else
                android
        fi
        if [[ ! " ${PLATFORMS[*]} " =~ " apple " ]];
            then
                echo "Skipping apple";
            else
                apple
        fi
        if [[ ! " ${PLATFORMS[*]} " =~ " linux " ]];
            then
                echo "Skipping linux";
            else
                linux
        fi
    else
        apple
        android
        linux
fi