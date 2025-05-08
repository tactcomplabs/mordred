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
 * - Network (topology) - mostly python to do the linkage; but router has to be aware for initialization
 * - Router - basic implementation - probably want this to be pretty generic/flexible
 *    - Routing function - impacted by topology; include pipeline model - initialization(?)
 *    - Buffer/port interface - may differ on input and output (and virtual channels)
 *    - Allocator - VC and internal xbar
 *      - Arbiter - manage who gets what and when
 *    - Credit tracker - how managed; paper shows as flexible, but would have to dig through code to compare
 */

// Standard headers
#include <cstdint>
#include <vector>

// Local SST headers
#include "sst_config.h"
#include "RtrPortControlAPI.h"
#include "TopologyAPI.h"

// TODO: Configure verbosity control

namespace SST::Mordred {

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
    {"id", "ID of the router", nullptr},
    {"clock", "Clock frequency of the router", "1GHz"},
    {"num_ports", "Number of ports on the router", "3"},
    {"num_local_ports", "Number of local ports", "1"},
    {"num_vcs",            "Number of virtual channels.", "1"},
    {"flit_width", "Number of bits per flit", "32"},
    {"channel_bus_width", "Number of bits per channel/link", "32",}
    // {"frequency",          "Frequency of the router in Hz (can include SI prefix."},
    // {"link_bw",            "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    // {"flit_size",          "Flit size specified in either b or B (can include SI prefix)."},
    // {"input_buf_size",     "Size of input buffers in either b or B (can use SI prefix).  Default is 2*flit_size."},
    // {"port_priority_equal","Set to true to have all port have equal priority (usually endpoint ports have higher priority).","false"},
    // {"use_dense_map",      "Set to true to have a dense network id map instead of the sparse map normally used.","false"},
    // {"network_inspectors", "Comma separated list of network inspectors to put on output ports.", ""},
  )

  // Create a topology subcomponent
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    {"topology", "Topology and routing subcomponent", "SST::Mordred::TopologyAPI"},
    {"portcontrol", "PortControl blocks; loaded anonymously", "SST::Mordred::RtrPortControlAPI"}
    //{"arbitration", "Arbitration scheme/model", "SST::Mordred::RtrArbitrationAPI"}
  )

  SST_ELI_DOCUMENT_PORTS(
    // TODO: Add message types as appropriate
    { "port%(portnum)d", "Port id.", { "basicMordredEvent" } },
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

  // Clock Handler
  bool clockTick( Cycle_t cycle );


private:
  // event handlers
  void handleInEvent( SST::Event* ev, int32_t linknum );

private:
  SST::Output             output;
  uint32_t                id;
  TimeConverter*          timeConverter;
  uint32_t                numPorts{};
  uint32_t                numLocalPorts{};

  // Major components
  TopologyAPI* topology{nullptr};
  std::vector<RtrPortControlAPI*>  portsVec;

};  // SimpleRTR

} // namespace SST::Mordred
