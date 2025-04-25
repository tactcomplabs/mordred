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
#include "sst_config.h"

// TODO: Configure verbosity control

namespace SST {
namespace Mordred {

class TestEP : public SST::Component {

public:
  SST_ELI_REGISTER_COMPONENT(
    TestEP, "mordred", "test_ep", SST_ELI_ELEMENT_VERSION( 0, 1, 0 ), "Simple endpoint", COMPONENT_CATEGORY_NETWORK
  )

  SST_ELI_DOCUMENT_PARAMS(
    { "verbose",       "Sets the output verbsoity",                    "5" },
    )

  SST_ELI_DOCUMENT_PORTS( { "port", "Port which connects to a router.", { "basicMordredEvent" } }, )

  SST_ELI_DOCUMENT_STATISTICS()

public:
  TestEP( ComponentId_t cid, Params& params );
  ~TestEP() { /* empty destructor */ }

  /// SST Required
  void init(uint32_t phase) override;
  void setup() override;
  void complete(uint32_t phase) override;
  void finish() override;

private:
  // event handlers
  void handleIncomingPacket( SST::Event* ev );

private:
  SST::Output  output;
  SST::Link*   localPort;
};  // TestEP

}  // namespace Mordred
}  // namespace SST