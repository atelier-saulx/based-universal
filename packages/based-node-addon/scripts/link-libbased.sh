#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )


ln -sF ${SCRIPT_DIR}/../../cpp-client/libbased.dylib ${SCRIPT_DIR}/../based-lib/libbased.dylib
ln -sF ${SCRIPT_DIR}/../../cpp-client/libbased.dylib ${SCRIPT_DIR}/../libbased.dylib
ln -sF ${SCRIPT_DIR}/../../cpp-client/include/based.h ${SCRIPT_DIR}/../based-lib/based.h

echo "Done...."