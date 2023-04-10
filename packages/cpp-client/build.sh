while getopts t: flag
do
    case "${flag}" in
        t) USER_TARGETS=${OPTARG};;
    esac
done


SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Android targets

TARGETS="armeabi-v7a arm64-v8a x86 x86_64"
NDK_FOLDER=${HOME}/Library/Android/sdk/ndk/25.2.9519653
ANDROID_API_VERSION=33


for TARGET in ${TARGETS}
do    
    if [ ! -z ${USER_TARGETS+x} ];
        then
            if [[ ! " ${USER_TARGETS[*]} " =~ " ${TARGET} " ]]; 
            then
                echo "Skipping ${TARGET}";
                continue;
            else echo "Building ${TARGET}"
            fi
    fi
    # create one build dir per target architecture
    TARGET_DIR=${SCRIPT_DIR}/build/android/${TARGET}
    mkdir -p ${TARGET_DIR}
    cd ${TARGET_DIR}

    cmake -DCMAKE_SYSTEM_NAME=Android \
    -DCMAKE_ANDROID_ARCH_ABI=${TARGET} \
    -DARCH_FOLDER=Android-${TARGET} \
    -DCMAKE_ANDROID_NDK=${NDK_FOLDER} \
    -DCMAKE_SYSTEM_VERSION=${ANDROID_API_VERSION} \
    -DCMAKE_ANDROID_NDK_DEPRECATED_HEADERS=TRUE \
    -S ../../.. -B .

    cmake --build .
    cd -
done


# TODO: Apple targets

TARGETS="catalyst darwin ios ios-sim"

for TARGET in ${TARGETS}
do    
    if [ ! -z ${USER_TARGETS+x} ];
        then
            if [[ ! " ${USER_TARGETS[*]} " =~ " ${TARGET} " ]]; 
            then
                echo "Skipping ${TARGET}";
                continue;
            else echo "Building ${TARGET}"
            fi
    fi
    # create one build dir per target architecture
    TARGET_DIR=${SCRIPT_DIR}/build/apple/${TARGET}
    mkdir -p ${TARGET_DIR}
    cd ${TARGET_DIR}

    cmake -G"Xcode" -DCMAKE_TOOLCHAIN_FILE=../../../toolchains/${TARGET}.cmake \
    -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="36CCRKC437" \
    -S ../../.. -B .
    
    cmake --build . --config Release

    # make -j32

    cd -
done
