SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Android targets

TARGETS="armeabi-v7a arm64-v8a x86 x86_64"
NDK_FOLDER=${HOME}/Library/Android/sdk/ndk/25.2.9519653
ANDROID_API_VERSION=33


for TARGET in ${TARGETS}
do    
    # create one build dir per target architecture
    mkdir -p ${SCRIPT_DIR}/${TARGET}
    cd ${SCRIPT_DIR}/${TARGET}

    cmake -DCMAKE_SYSTEM_NAME=Android -DCMAKE_ANDROID_ARCH_ABI=${TARGET} -DARCH_FOLDER=Android-${TARGET} -DCMAKE_ANDROID_NDK=${NDK_FOLDER} -DCMAKE_SYSTEM_VERSION=${ANDROID_API_VERSION} -DCMAKE_ANDROID_NDK_DEPRECATED_HEADERS=TRUE -S ../.. -B .

    cmake --build .

    # make -j32

    cd -
done

# TODO: Apple targets