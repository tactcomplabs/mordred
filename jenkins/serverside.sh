# Copyright (C) 2026-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details

## NOTE: Script assumes that we are starting in {some-path}/sst-core/

## Configure some environment variables
# export SST_INSTALL=
# export PATH
## Set compiler settings?

## Remove any prior versions of sst-elements and get the repo
rm -rf ./sst-elements
git clone https://github.com/sstsimulator/sst-elements.git
cd sst-elements || exit 5
git checkout $SSTELEMENTSBRANCH
cd ..

## Remove any prior versions of mordred and get the repo
rm -rF ./mordred
git clone https://github.com/tactcomplabs/mordred.git
cd mordred || exit 10
git checkout $MORDREDBRANCH
cd ..

# Run target specific script(s)
runscript=$"mordred/jenkins/${JOB_BASE_NAME}.sh"
if [[ -x ${runscript} ]]; then
  ${runscript}
  echo "Completed ${JOB_BASE_NAME} using ${runscript}"
  exit 0
fi

# Run common run script if it exists
runscript="mordred/jenkins/rundev.sh"
if [[ ! -x ${runscript} ]]; then
  echo "Warning: ${runscript} not found. Skipping this run"
  exit 0
fi

${runscript}
echo "Completed ${JOB_BASE_NAME} using ${runscript}"