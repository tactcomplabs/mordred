# Copyright (C) 2026-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details

# mordred/tests/CMakeLists.txt
# this is triggered via the top-level CMakeLists.txt in mordred

install(CODE "execute_process(COMMAND sst-register SST_ELEMENT_TESTS mordred=${CMAKE_CURRENT_SOURCE_DIR})")

# EOF
