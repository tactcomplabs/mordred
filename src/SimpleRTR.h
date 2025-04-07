//
// SimpleRTR.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

/**
 * Components of booksim with multiple implementations:
 * - Network (topology) - mostly python to do the linkage
 * - Router - basic implementation - probably want this to be pretty generic/flexible
 *    - Routing function - impacted by topology; include pipeline model
 *    - Buffer - may differ on input and output (and virtual channels)
 *    - Allocator - VC and internal xbar
 *      - Arbiter - manage who gets what and when
 *    - Credit tracker - how managed; paper shows as flexible, but would have to dig through code to compare
 */

// Standard headers
#include <cinttypes>
#include <vector>

// Local SST header
#include "sst_config.h"

// TODO: Configure verbosity control

namespace SST {
namespace Mordred {

class SimpleRTR : public SST::Component {

public:
  SST_ELI_REGISTER_COMPONENT(
    SimpleRTR,
    "mordred",
    "simple_rtr",
    SST_ELI_ELEMENT_VERSION( 0, 1, 0 ),
    "Simple NoC router component",
    COMPONENT_CATEGORY_NETWORK
  )

  SST_ELI_DOCUMENT_PARAMS(
    { "verbose",       "Sets the output verbsoity",                    "5" },
    //{"local_ports",        "Number of ports that are dedicated to endpoints.","1"},
    //{"topo_ports",         "Number of ports that connect to other routers.", "1"}
    // {"frequency",          "Frequency of the router in Hz (can include SI prefix."},
    // {"link_bw",            "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    // {"flit_size",          "Flit size specified in either b or B (can include SI prefix)."},
    // {"input_buf_size",     "Size of input buffers in either b or B (can use SI prefix).  Default is 2*flit_size."},
    // {"port_priority_equal","Set to true to have all port have equal priority (usually endpoint ports have higher priority).","false"},
    // {"route_y_first",      "Set to true to rout Y-dimension first.","false"},
    // {"use_dense_map",      "Set to true to have a dense network id map instead of the sparse map normally used.","false"},
    // {"network_inspectors", "Comma separated list of network inspectors to put on output ports.", ""},
  )

  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  SST_ELI_DOCUMENT_PORTS(
    { "local_port%(portnum)d", "Ports which connect to endpoints.", { "basicMordredEvent" } },
    { "topo_port%(portnum)d", "Ports which connect to other routers.", { "basicMordredEvent" } },
  )

  SST_ELI_DOCUMENT_STATISTICS()

public:
  SimpleRTR( ComponentId_t cid, Params& params );
  ~SimpleRTR() { /* empty destructor */ }

  /// SST Required
  void init(uint32_t phase) override;
  void setup() override;
  void complete(uint32_t phase) override;
  void finish() override;

private:
  // event handlers
  void handleLocalInWithID( SST::Event* ev, int32_t linknum );
  void handleTopoInWithID( SST::Event* ev, int32_t linknum );

private:
  SST::Output             output;
  uint32_t                num_local_ports{};
  uint32_t                num_topo_ports{};
  std::vector<SST::Link*> LocalPortsVec;
  std::vector<SST::Link*> TopoPortsVec;

};  // SimpleRTR

}  // namespace Mordred
}  // namespace SST