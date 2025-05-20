//
// TestEP.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

// Standard headers
#include <cinttypes>
#include <vector>

// Local SST header
#include "MordredNIC.h"
#include "sst_config.h"

namespace SST::Mordred {

class TestEP : public SST::Component {

public:
  SST_ELI_REGISTER_COMPONENT(
    TestEP, "mordred", "test_ep", SST_ELI_ELEMENT_VERSION( 0, 1, 0 ), "Simple endpoint", COMPONENT_CATEGORY_NETWORK
  )

  SST_ELI_DOCUMENT_PARAMS(
    { "verbose",       "Sets the output verbsoity",                    "5" },
    {"clock", "Clock frequency of the endpoint", "1GHz"},
    )

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    {"noc_iface", "NoC interface", "SST::Interfaces::SimpleNetwork"}
  )

  SST_ELI_DOCUMENT_PORTS()

  SST_ELI_DOCUMENT_STATISTICS()

public:
  TestEP( ComponentId_t cid, Params& params );
  ~TestEP() { /* empty destructor */ }

  /// SST Required
  void init(uint32_t phase) override;
  void setup() override;
  void complete(uint32_t phase) override { /* empty */ }
  void finish() override { /* empty */ }

  bool clockTick( Cycle_t cycle );

private:
  // event handlers
  //void handleIncomingPacket( SST::Event* ev );

private:
  Output  output;
  TimeConverter*          timeConverter;
  Interfaces::SimpleNetwork*  nocIface;
};  // TestEP

}  // namespace SST::Mordred
