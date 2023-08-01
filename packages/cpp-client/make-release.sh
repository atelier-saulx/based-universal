#!/bin/bash

die () {
    echo >&2 "$@"
    exit 1
}

[ "$#" -eq 1 ] || die "Please provide project version as first argument"

PROJECT_VERSION=$1

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

ANDROID_TARGETS="armeabi-v7a arm64-v8a x86 x86_64"

APPLE_DIR=${SCRIPT_DIR}/build/apple
ANDROID_DIR=${SCRIPT_DIR}/build/android

echo Creating apple release...

cd ${APPLE_DIR}
rm -rf Based.xcframework
xcodebuild -create-xcframework \
    -framework catalyst/Release/based.framework \
    -framework ios/Release-iphoneos/based.framework \
    -framework ios-sim/Release-iphonesimulator/based.framework \
    -framework darwin/Release/based.framework \
    -output Based.xcframework
echo zipping...
zip -q based-universal-v${PROJECT_VERSION}-xcframework.zip Based.xcframework

cd ${ANDROID_DIR}

HEADER_FILE=${SCRIPT_DIR}/include/based.h

rm -rf ${SCRIPT_DIR}/build/android/release

echo Creating android release...

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
zip -q based-universal-v${PROJECT_VERSION}-android.zip release


echo Creating linux release...

cd ${SCRIPT_DIR}/build/linux
echo zipping...
zip -rq based-universal-v${PROJECT_VERSION}-linux.zip libbased.so based.h

echo Done.
