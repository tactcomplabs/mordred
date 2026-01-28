# Copyright (C) 2026-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details
#
# jenkins.sh
#
# Executes jenkins builds
#

echo "---- MORDRED TEST HARNESS ----"

if command -v sst &> /dev/null; then
  echo "sst executable found at: $(command -v sst)"
  echo "sst version: $(command -v sst --version)"
else
  echo "sst executable not found"
  exit 50
fi


mkdir build || exit 51
cd build || exit 52
cmake ../ || exit 53
make -j || exit 55
make test || ctest --rerun-failed --output-on-failure || exit 56

echo "---- MORDRED TESTS ARE SUCCESSFUL ----"
