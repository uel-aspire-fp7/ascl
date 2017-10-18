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

if [ ! -e /opt/3rd_party ]; then
    echo "Directory 3rd_party not present!"
    exit
fi

# Create extra symlinks
ln -s $repo_dir/src $build_dir/include
ln -s $repo_dir/src $build_dir/src
ln -s $repo_dir/src/aspire-portal $build_dir/

# Create the objects
cd $build_dir/src
obj_dir=$build_dir/obj
for platform in linux android linux_x86; do
    make -f Makefile.${platform} clean all > /dev/null
    mkdir -p ${obj_dir}/${platform}/
    mv ascl.o ${obj_dir}/${platform}/
done

# Select the x86 server
ln -s $obj_dir/linux_x86 $obj_dir/serverlinux
