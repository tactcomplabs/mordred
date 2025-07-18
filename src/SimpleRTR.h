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
#include "VcAllocAPI.h"
#include "sst_config.h"

// TODO: Configure verbosity control (use constants in MordredEvents)

// TODO: This doesn't account for concentration yet

/**
 * Currently, we assume that all ports linked to the router (so both from endpoints and other routers),
 * have the same number of virtual networks and virtual channels.  Since we're only using 1 of each for
 * now, we can get away with this.
 */

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

  /*
   * For now, the {input,output}_buf_size parameters are handed down to the ports and the ports
   * will do some math and distribute credits equally across each of the VN,VC buffers.
   */

  SST_ELI_DOCUMENT_PARAMS(
    { "verbose",       "Sets the output verbosity",                    "5" },
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
    {"vc_alloc", "VC allocator", "SST::Mordred::VcAllocAPI"},
    {"arbiter", "Arbitration scheme/model", "SST::Mordred::ArbAPI"}, // TODO: This becomes the switch allocator
  )

  SST_ELI_DOCUMENT_PORTS(
    // TODO: Add message types as appropriate
    { "port%(portnum)d", "Port id.", { "basicMordredEvent" } },
  )

  SST_ELI_DOCUMENT_STATISTICS(
    {"tick10_cnt", "Number of cycles/10", "unitless", 3},
  )

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
  Output                  output;
  uint32_t                id;
  TimeConverter*          timeConverter;
  uint32_t                numPorts{};
  uint32_t                numLocalPorts{};
  uint32_t                numVns{};
  uint32_t                numVcs{};

  // Major components
  TopologyAPI* topology{nullptr};
  ArbAPI* arbiter{nullptr};
  VcAllocAPI* vcAlloc{nullptr};
  // If a port is unconnected, we push_back a nullptr for that port index
  std::vector<RtrPortControlAPI*>  portsVec;

  // Shared between components
  // perPortSharedObjs[port_id]
  std::vector<RtrOwnedSharedObjs> perPortSharedObjs;

  // Per (input) port structure; the pair is the VN,VC input pair that won arbitration
  // to send something through the crossbar to an output port
  // use UINT32_MAX, UINT32_MAX to identify idle/unassigned;
  // TODO: TBD if this will be deleted or absorbed into perPortSharedObjs
  std::vector<std::pair<uint32_t,uint32_t>> arbWinners;

  // Per port structure, the output state is used to identify if a port is already
  // sending to this output port
  // TODO: Delete this - look into the ports as needed
  std::vector<OutVcStateE> outVcStates;

  Statistic<uint64_t>* tickCounter;

};  // SimpleRTR

} // namespace SST::Mordred
