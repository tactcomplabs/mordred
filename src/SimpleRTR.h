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
#include "ArbAPI.h"
#include "RtrPortControlAPI.h"
#include "TopologyAPI.h"
#include "sst_config.h"

// TODO: Configure verbosity control (use constants in MordredEvents)

// TODO: This doesn't account for concentration yet

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
    //{"num_vns", "Number of virtual networks", "1"},
    {"num_vcs",            "Number of virtual channels.", "1"},
    {"flit_size",          "Flit size specified in either b or B (can include SI prefix).", "32b"},
    {"input_buf_size",     "Size of input buffers specified in b or B (can include SI prefix).", nullptr},
    {"output_buf_size",    "Size of output buffers specified in b or B (can include SI prefix).", nullptr},
    // {"channel_bus_width", "Number of bits per channel/link", "32",}
    // {"frequency",          "Frequency of the router in Hz (can include SI prefix."},
    // {"link_bw",            "Bandwidth of the links specified in either b/s or B/s (can include SI prefix)."},
    // {"input_buf_size",     "Size of input buffers in either b or B (can use SI prefix).  Default is 2*flit_size."},
    // {"port_priority_equal","Set to true to have all port have equal priority (usually endpoint ports have higher priority).","false"},
    // {"use_dense_map",      "Set to true to have a dense network id map instead of the sparse map normally used.","false"},
    // {"network_inspectors", "Comma separated list of network inspectors to put on output ports.", ""},
  )

  // Create a topology subcomponent
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    {"topology", "Topology and routing subcomponent", "SST::Mordred::TopologyAPI"},
    {"portcontrol", "PortControl blocks; loaded anonymously", "SST::Mordred::RtrPortControlAPI"},
    {"arbiter", "Arbitration scheme/model", "SST::Mordred::ArbAPI"}
  )

  SST_ELI_DOCUMENT_PORTS(
    // TODO: Add message types as appropriate
    { "port%(portnum)d", "Port id.", { "basicMordredEvent" } },
  )

  SST_ELI_DOCUMENT_STATISTICS()

public:
  SimpleRTR( ComponentId_t cid, Params& params );
  ~SimpleRTR();

  /// SST Required
  void init(uint32_t phase) override;
  void setup() override;
  void complete(uint32_t phase) override { /* empty */ }
  void finish() override { /* empty */ }

  // Clock Handler
  bool clockTick( Cycle_t cycle );

private:
  SST::Output             output;
  uint32_t                id;
  TimeConverter*          timeConverter;
  uint32_t                numPorts{};
  uint32_t                numLocalPorts{};
  uint32_t                numVns{};
  uint32_t                numVcs{};

  // Major components
  TopologyAPI* topology{nullptr};
  ArbAPI* arbiter{nullptr};
  std::vector<RtrPortControlAPI*>  portsVec;

  // Shared between components
  // perPortVnObjs[port_id][vn]
  std::vector<std::vector<RtrOwnedVnObj>> perPortVnObjs;

  std::vector<std::pair<uint32_t,uint32_t>> arbWinners; // use UINT32_MAX to identify idle/unassigned; id VN,VC of port that won arbitration

  // These would need to be 3D - port.vn.vc
  std::vector<std::vector<RtrPortControlAPI::InVcStateE>> inVcStates; // TODO: candidate for InVcHeads?
  std::vector<std::vector<RtrPortControlAPI::OutVcStateE>> outVcStates;

};  // SimpleRTR

} // namespace SST::Mordred
