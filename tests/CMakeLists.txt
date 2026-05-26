# Copyright (C) 2026-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details

# mordred/tests/CMakeLists.txt
# this is triggered via the top-level CMakeLists.txt in mordred

set(MORDRED_TEST_SRCS
  flatbutterfly_k2n4_testnic.py
  mesh3x3_testnic.py
  torus3D_3x3x3_2vc_testnic.py
  torus5x5_2vc_testnic.py
  ipdps25tutorial_demo7.py
  mordred_memNICFour.py
  mordred_testBridge.py
)

if(MORDRED_ENABLE_PHYS_CHANNEL)
  list(APPEND MORDRED_TEST_SRCS
    mesh2x1_rtrportcontrolpc.py
    mesh2x1_uciePhysChannel.py
    mesh3x3_uciePhysChannel.py
    flatbutterfly_k2n4_uciePhysChannel.py
    torus5x5_2vc_uciePhysChannel.py
    torus3D_3x3x3_2vc_uciePhysChannel.py
  )
endif()

set (passRegex "Simulation is complete")

foreach(testSrc ${MORDRED_TEST_SRCS})
  get_filename_component(testName ${testSrc} NAME_WE)
  add_test(NAME ${testName}
          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
          COMMAND sst --add-lib-path=${CMAKE_BINARY_DIR}/src ${testSrc})
  set_tests_properties(${testName}
          PROPERTIES
          TIMEOUT 30
          LABELS "all"
          PASS_REGULAR_EXPRESSION "${passRegex}")
endforeach(testSrc)

# EOF