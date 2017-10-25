#!/bin/bash
set -o errexit
set -o pipefail
set -o nounset
#set -o xtrace

# Get the repo and build directories, go to the build directory
repo_dir=$(dirname $0)
build_dir=$1
mkdir -p $build_dir
cd $build_dir

# Create extra symlinks
ln -s $repo_dir/src $build_dir/include
ln -s $repo_dir/src $build_dir/src
ln -s $repo_dir/src/aspire-portal $build_dir/

# Create the objects
cd $build_dir/src
obj_dir=$build_dir/obj
make -f Makefile clean all > /dev/null
mkdir -p ${obj_dir}/serverlinux
mv ascl.o ${obj_dir}/serverlinux
