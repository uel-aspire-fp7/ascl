#!/bin/bash
set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

# Get the repo and build directories, go to the build directory
repo_dir=$(dirname $0)
cd $repo_dir

# Create extra symlink
ln -s src include

# Create the object
cd $repo_dir/src
obj_dir=$repo_dir/obj
make -f Makefile clean all > /dev/null
mkdir -p ${obj_dir}
mv ascl.o ${obj_dir}
