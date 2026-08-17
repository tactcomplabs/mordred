//
// GatewayTopology.h
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDRED_GATEWAYTOPOLOGY_H
#define MORDRED_GATEWAYTOPOLOGY_H

// Standard headers
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Local headers
#include "MordredEvents.h"
#include "TopologyAPI.h"

namespace SST::Mordred {

/**
 * GatewayTopology — decorates any TopologyAPI implementation (Mesh, Torus2d,
 * Torus3d, FlatButterfly, ...) with cross-domain gateway redirection.
 *
 * Two or more independently-addressed topology domains (e.g. two meshes,
 * or a mesh and a torus) can be joined by a single router-router link if
 * every router in each domain loads GatewayTopology as its "topology"
 * subcomponent, wrapping that domain's real topology in the "inner_topology"
 * slot. Each domain owns a contiguous slice [id_base, id_base+local_range_size)
 * of one shared global id space; a destination outside that slice is
 * "foreign" and gets redirected toward the domain's one designated gateway
 * router, which forwards it out its own extra, dedicated port unchanged
 * (no address translation — the dest field is already globally meaningful).
 *
 * GatewayTopology must be loaded on EVERY router in the domain, not just the
 * gateway router — MordredRouter calls topology->routePacket() at every hop,
 * including the router where a foreign-destined packet is first injected, so
 * every router needs to know how to steer such traffic toward the gateway.
 * Only the router configured with gateway_rtr_id == its own rtr_id is "the"
 * gateway; it alone reads gateway_port and gets an extra, additive port
 * beyond its inner topology's normal direction set (never a repurposed
 * cardinal/wraparound port — several inner topologies, e.g. Torus2dTopo and
 * Torus3dTopo, fatal in init() if any of their own router-router ports is
 * left unconnected).
 */
class GatewayTopology : public TopologyAPI {

public:
  // register with the SST Core
  SST_ELI_REGISTER_SUBCOMPONENT(
    GatewayTopology,
    "mordred",           // component library
    "GatewayTopology",   // component name
    SST_ELI_ELEMENT_VERSION( 0, 0, 1 ),
    "Cross-domain gateway decorator for any TopologyAPI implementation",
    SST::Mordred::TopologyAPI
  )

  // register the parameters
  SST_ELI_DOCUMENT_PARAMS(
    { "verbose", "Sets the output verbosity", "5" },
    { "id_base", "Offset of this domain's slice in the shared global id space", "0" },
    { "gateway_rtr_id", "Local rtr_id (within this domain) of the designated gateway router", nullptr },
    { "gateway_port", "Port index the gateway router uses for the cross-domain link "
                       "(required on the gateway router only)", nullptr },
    { "local_range_size", "Total endpoints this domain's inner topology owns "
                           "(e.g. xDim*yDim*num_local_ports)", nullptr }
  )

  // register the ports
  SST_ELI_DOCUMENT_PORTS()

  // register the subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    { "inner_topology", "The real topology being wrapped", "SST::Mordred::TopologyAPI" }
  )

  /// GatewayTopology: constructor
  GatewayTopology(
    ComponentId_t          id,
    Params&                params,
    uint32_t               rtr_id,
    uint32_t               num_ports,
    uint32_t               num_local_ports,
    std::vector<uint32_t>* connected_ports
  );

  /// GatewayTopology: destructor
  ~GatewayTopology() override { delete output; }

  int32_t getEndpointId( uint32_t portnum ) override;

  /// Get the output port for a packet
  uint32_t routePacket( uint32_t dest ) final;

  /// Do routing for untimed broadcast packets. Delegates to the inner
  /// topology for intra-domain flooding; on the gateway router, additionally
  /// relays every broadcast across the cross-domain link -- except the one
  /// just received FROM that link, so it never bounces back.
  void routeUntimedBroadcastPacket( uint32_t receive_port_id, MordredInitEvent* ev, std::vector<Event*>& output_events ) final;

  /// Pass through to the inner topology; the gateway port itself is never a wraparound link.
  bool isWrapAroundOutput( uint32_t output_port ) const override;

  /// default constructor
  GatewayTopology() : SST::Mordred::TopologyAPI() {}

  /// serialization
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    SST_SER( output );
    SST_SER( inner );
    SST_SER( rtrId );
    SST_SER( numLocalPorts );
    SST_SER( idBase );
    SST_SER( gatewayRtrId );
    SST_SER( localRangeSize );
    SST_SER( gatewayPort );
    SST_SER( amGateway );
    SST_SER( perPortConnectedRtr );
  }

  /// serialization implementations
  ImplementSerializable( SST::Mordred::GatewayTopology );

private:
  Output* output;

  TopologyAPI* inner{ nullptr };

  uint32_t rtrId;
  uint32_t numLocalPorts;

  uint32_t idBase;
  uint32_t gatewayRtrId;
  uint32_t localRangeSize;
  uint32_t gatewayPort{ 0 };
  bool     amGateway{ false };

  // Needed to check whether the gateway port is actually wired before
  // relaying a broadcast out it (see routeUntimedBroadcastPacket()).
  std::vector<uint32_t>* perPortConnectedRtr;
};

}  // namespace SST::Mordred

#endif  //MORDRED_GATEWAYTOPOLOGY_H
