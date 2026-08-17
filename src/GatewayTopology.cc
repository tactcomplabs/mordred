//
// GatewayTopology.cc
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

// Standard headers
#include <cinttypes>
#include <cstdint>

// Local SST header
#include "sst_config.h"

// Local headers
#include "GatewayTopology.h"

using namespace SST::Mordred;

GatewayTopology::GatewayTopology(
  ComponentId_t          id,
  Params&                params,
  uint32_t               rtr_id,
  uint32_t               num_ports,
  uint32_t               num_local_ports,
  std::vector<uint32_t>* connected_ports
)
  : TopologyAPI( id ), rtrId( rtr_id ), numLocalPorts( num_local_ports ) {
  const auto verbosity = params.find<uint32_t>( "verbose", MORDRED_VERBOSE_MED );
  output               = new Output( "GatewayTopology [" + getName() + ":@p:@t]:", verbosity, 0, Output::STDOUT );

  idBase = params.find<uint32_t>( "id_base", 0 );

  gatewayRtrId = params.find<uint32_t>( "gateway_rtr_id", UINT32_MAX );
  if( gatewayRtrId == UINT32_MAX )
    output->fatal( CALL_INFO, -1, "GatewayTopology requires gateway_rtr_id to be specified\n" );

  localRangeSize = params.find<uint32_t>( "local_range_size", UINT32_MAX );
  if( localRangeSize == UINT32_MAX )
    output->fatal( CALL_INFO, -1, "GatewayTopology requires local_range_size to be specified\n" );

  amGateway = ( rtrId == gatewayRtrId );

  // The inner topology only ever needs to know about its own direction set
  // plus local ports -- it stays completely unaware it is being wrapped, and
  // never sees (or routes to) the extra gateway port. Only the designated
  // gateway router carves one port out for that purpose; every other router
  // in this domain passes its normal, unmodified num_ports straight through.
  uint32_t inner_num_ports = num_ports;
  if( amGateway ) {
    gatewayPort = params.find<uint32_t>( "gateway_port", UINT32_MAX );
    if( gatewayPort == UINT32_MAX )
      output->fatal( CALL_INFO, -1, "GatewayTopology: gateway router (rtr_id=%" PRIu32 ") requires gateway_port\n", rtrId );
    inner_num_ports = num_ports - 1;
  }

  inner = loadUserSubComponent<TopologyAPI>(
    "inner_topology", ComponentInfo::SHARE_NONE, rtr_id, inner_num_ports, num_local_ports, connected_ports
  );
  if( !inner )
    output->fatal( CALL_INFO, -1, "GatewayTopology: no inner_topology subcomponent in slot 'inner_topology'\n" );

  output->verbose(
    CALL_INFO, MORDRED_VERBOSE_MIN, 0,
    "GatewayTopology constructed; rtr_id=%" PRIu32 ", id_base=%" PRIu32 ", local_range_size=%" PRIu32 ", "
    "gateway_rtr_id=%" PRIu32 ", amGateway=%d\n",
    rtrId, idBase, localRangeSize, gatewayRtrId, (int) amGateway
  );
}

int32_t GatewayTopology::getEndpointId( uint32_t portnum ) {
  if( amGateway && ( portnum == gatewayPort ) )
    return -1;  // not a local endpoint, same convention as any router-facing port

  // This is the value RtrPortControlBase hands to a connected NIC as its
  // network address during init (see RtrPortControlBase.cc's use of
  // topo->getEndpointId()), which the NIC then uses for both its own "src"
  // on send and to validate incoming "dest" on receive. It has to be
  // expressed in the same global id space as everything else routePacket()
  // deals with, so -- like local_dest in routePacket() -- the inner
  // topology's local id needs id_base added back before returning it.
  const int32_t local_id = inner->getEndpointId( portnum );
  if( local_id < 0 )
    return local_id;  // not a local endpoint (cardinal port, etc.) -- pass through
  return static_cast<int32_t>( idBase ) + local_id;
}

uint32_t GatewayTopology::routePacket( uint32_t dest ) {
  const bool foreign = ( dest < idBase ) || ( ( dest - idBase ) >= localRangeSize );

  if( foreign ) {
    if( amGateway ) {
      output->verbose( CALL_INFO, MORDRED_VERBOSE_HIGH, 0,
                       "Routing: dest=%" PRIu32 " is foreign; I am the gateway -> port=%" PRIu32 "\n",
                       dest, gatewayPort );
      return gatewayPort;  // hand off to the cross-domain link; dest is untouched
    }

    // Not the gateway: pretend the destination IS the gateway router's own
    // first local endpoint, so the inner topology's own routing algorithm --
    // whatever it is -- carries us there like any other in-domain packet.
    // NOTE: this is a LOCAL address for the inner topology's consumption --
    // no id_base offset here, same as the local_dest computed just below.
    const uint32_t proxy_dest = gatewayRtrId * numLocalPorts;
    output->verbose( CALL_INFO, MORDRED_VERBOSE_HIGH, 0,
                     "Routing: dest=%" PRIu32 " is foreign; redirecting toward gateway_rtr_id=%" PRIu32
                     " (proxy_dest=%" PRIu32 ")\n",
                     dest, gatewayRtrId, proxy_dest );
    return inner->routePacket( proxy_dest );
  }

  // In range: the inner topology only ever knows LOCAL addressing -- strip
  // this domain's id_base before delegating. (This is the offset that made
  // the bug above easy to miss: id_base=0 makes "local == global", which is
  // exactly the case the first domain in the test happens to use.)
  return inner->routePacket( dest - idBase );
}

void GatewayTopology::routeUntimedBroadcastPacket(
  uint32_t receive_port_id, MordredInitEvent* init_ev, std::vector<Event*>& output_events
) {
  inner->routeUntimedBroadcastPacket( receive_port_id, init_ev, output_events );
}

bool GatewayTopology::isWrapAroundOutput( uint32_t output_port ) const {
  return inner->isWrapAroundOutput( output_port );
}
