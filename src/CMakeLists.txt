# mordred/src/CMakeLists.txt
# this is not meant to be standalone
# this is triggered via the top-level CMakeLists.txt in mordred

set(MordredSrcs
        MordredEvents.h
        MordredNIC.h
        MordredNIC.cc
        SimpleRTR.h
        SimpleRTR.cc
        TestEP.h
        TestEP.cc
        TopologyAPI.h
        MeshTopology.h
        MeshTopology.cc
        RtrPortControlAPI.h
        RtrPortControl.h
        RtrPortControl.cc
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
