# Copyright (C) 20 26-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details

# mordred/tests/CMakeLists.txt
# this is triggered via the top-level CMakeLists.txt in mordred

set(MORDRED_TEST_SRCS
  flatbutterfly_k2n4_testnic.py
  mesh3x3_testnic.py
  mesh3x3_untimed_broadcast.py
  mesh3x3_2local_testnic.py
  mesh3x3_single_flit.py
  mesh3x3_large_message.py
  torus3D_3x3x3_2vc_testnic.py
  torus3D_3x3x3_2vc_untimed_broadcast.py
  torus5x5_2vc_testnic.py
  torus5x5_2vc_untimed_broadcast.py
  torus5x5_3vc_testnic.py
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
    torus5x5_3vc_uciePhysChannel.py
    torus5x5_4vc_uciePhysChannel.py
    torus3D_3x3x3_2vc_uciePhysChannel.py
    mesh2x1_uciePhysChannel_flit_format2.py
    mesh3x3_uciePhysChannel_2module.py
    mesh3x3_uciePhysChannel_2stack.py
    mesh3x3_uciePhysChannel_large_message.py
    mesh3x3_uciePhysChannel_large_message_lowcredits.py
    mesh2x1_ucie_rtrlink_testnic.py
    mesh4x2_ucie_rtrlink_testnic.py
    mesh2x2_gateway_ucie_testnic.py
    mesh2x2_gateway_untimed_broadcast.py
    torus3D_3x3x3_2vc_gateway_ucie_testnic.py
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
          TIMEOUT 120
          LABELS "all"
          PASS_REGULAR_EXPRESSION "${passRegex}")
endforeach(testSrc)

# EOF
