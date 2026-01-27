#!/bin/bash

# Copyright (C) 2026-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details

## Standard versions to test against:
# Latest SST release and devel
# Reasonable versions of llvm and gcc

## Devel version of mordred should be enough for now; actually, may want to
## do PR branches as well

## NOTE: Script assumes that we are starting in {some-path}/sst-core/

# ensure non-zero exit code in pipe will propagate
set -o pipefail

# Startup info
echo "BUILDNAME=$BUILDNAME"
echo "WORKSPACE=$WORKSPACE"
cd $WORKSPACE || exit 1

echo "#---> $0 started in $PWD"

# Print various environment variables, etc
echo "SST_INSTALL=$SST_INSTALL"
echo "PATH=$PATH"

# Need both sst-elements and mordred branches
echo "SST_ELEMENTS_BRANCH=$SSTELEMENTSBRANCH"
echo "MORDRED_BRANCH=$MORDREDBRANCH"

# config params
echo "SST_TEST_CORE=$SST_TEST_CORE"

# Platform specific things to fix up
if [[ "$(uname)" != "Darwin" ]]; then
	# detect_leaks is not supported on Mac
	export ASAN_OPTIONS=detect_leaks=0
else
	# currently not running MPI on Mac
	export DISABLE_MPI="--disable-mpi"
fi

# Clear out and rebuild SST core
rm -Rf $SST_INSTALL/
./autogen.sh || exit 10
./configure --prefix=$SST_INSTALL ${DISABLE_MPI} || exit 12
make -j || exit 14
make install || exit 16
export PATH=$PATH:$SST_INSTALL/bin

# Run core tests
if [ "$SST_TEST_CORE" = true ]; then
  which sst-test-core || exit 18
  sst-test-core || exit 19
fi

# Rebuild sst-elements
cd sst-elements || exit 30
./autogen || exit 32
./configure --prefix=$SST_INSTALL/sst-elements --with-sst-core=$SST_INSTALL || exit 34
mkdir build || exit 36
cd build || exit 37
make -j || exit 38
make install || exit 39
cd ../.. || exit 40 # return us to {some-path}/sst-core/

# Run the merlin test suite esp since it's needed for mordred testing
which sst-test-elements || exit 42
sst-test-elements -w "*merlin*" | exit 44

# Build/install/test mordred
cd mordred || exit 50
mkdir build || exit 52
cd build || exit 53
cmake .. || exit 54
make -j || exit 56
make install || exit 57
make test || exit 58

echo "#---> $0 finished"