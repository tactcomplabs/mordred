# Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
# All Rights Reserved
# contact@tactcomplabs.com
# See LICENSE in the top level directory for licensing details

# mordred/src/CMakeLists.txt
# this is triggered via the top-level CMakeLists.txt in mordred

set(MordredSrcs
        MordredEvents.h
        MordredNIC.h
        MordredNIC.cc
        SimpleRTR.h
        SimpleRTR.cc
        TopologyAPI.h
        MeshTopology.h
        MeshTopology.cc
        RtrPortControlAPI.h
        RtrPortControl.h
        RtrPortControl.cc
        RtrPortControlSN.h
        RtrPortControlSN.cc
        MordredPortLinkSN.h
        MordredPortLinkSN.cc
        MordredPassthroughSN.h
        MordredPassthroughSN.cc
        MordredNicSN.h
        MordredNicSN.cc
        XbarArbAPI.h
        XbarArbRR.h
        XbarArbRR.cc
        VcAllocAPI.h
        VcAllocRR.h
        VcAllocRR.cc
        FlatButterflyTopo.h
        FlatButterflyTopo.cc
        Torus2dTopo.h
        Torus2dTopo.cc
        Torus3dTopo.h
        Torus3dTopo.cc
)

add_library(mordred SHARED ${MordredSrcs})
target_include_directories(mordred PUBLIC ${SST_INSTALL_DIR}/include)

install(TARGETS mordred DESTINATION ${CMAKE_CURRENT_SOURCE_DIR})
install(CODE "execute_process(COMMAND sst-register mordred mordred_LIBDIR=${CMAKE_CURRENT_SOURCE_DIR})")

# EOF
