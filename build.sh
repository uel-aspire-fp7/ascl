#!/bin/bash

set -o errexit
set -o pipefail

CDEFAULT='\e[39m'
CRED='\e[31m'
CYELLOW='\e[33m'
CGREEN='\e[32m'

cd $(dirname $0)
SCRIPT_DIR=$(pwd)
TARGET_DIR=$1

echo -e "${CGREEN} ASCL build process STARTED${CDEFAULT}"

if [ ! -e /opt/3rd_party ]; then
    echo "Please run FRAMEWORK/select-tool-version.sh -t 3rd_party -v VERSION"
    exit
fi

cd $(dirname $0)/src

# library build process
for platform in linux android linux_x86; do
    make -f Makefile.${platform} clean all > /dev/null
    mkdir -p ${TARGET_DIR}/${platform}/
    mv ascl.o ${TARGET_DIR}/${platform}/
done

echo -e "${CGREEN} ASCL build process COMPLETED${CDEFAULT}\n"

TREE_OK=$(which tree)
[ "${TREE_OK}" != '' ] && tree -h ${TARGET_DIR}
