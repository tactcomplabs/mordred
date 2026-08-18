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
#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <sstream>
#include <string>

// Local SST header
#include "sst_config.h"

// Local headers
#include "GatewayTopology.h"

using namespace SST::Mordred;

namespace {

// Parse a comma-separated list of non-negative integers, e.g. "4,8,12".
// An empty string parses to an empty vector (no remote domains configured).
std::vector<uint32_t> parseUintList( const std::string& s ) {
  std::vector<uint32_t> result;
  std::istringstream    ss( s );
  std::string           tok;
  while( std::getline( ss, tok, ',' ) ) {
    auto first = tok.find_first_not_of( " \t" );
    if( first == std::string::npos )
      continue;
    tok = tok.substr( first, tok.find_last_not_of( " \t" ) - first + 1 );
    result.push_back( static_cast<uint32_t>( std::stoul( tok ) ) );
  }
  return result;
}

}  // namespace

GatewayTopology::GatewayTopology(
  ComponentId_t          id,
  Params&                params,
  uint32_t               rtr_id,
  uint32_t               num_ports,
  uint32_t               num_local_ports,
  std::vector<uint32_t>* connected_ports
)
  : TopologyAPI( id ), rtrId( rtr_id ), numLocalPorts( num_local_ports ), perPortConnectedRtr( connected_ports ) {
  const auto verbosity = params.find<uint32_t>( "verbose", MORDRED_VERBOSE_MED );
  output               = new Output( "GatewayTopology [" + getName() + ":@p:@t]:", verbosity, 0, Output::STDOUT );

  idBase = params.find<uint32_t>( "id_base", 0 );

  localRangeSize = params.find<uint32_t>( "local_range_size", UINT32_MAX );
  if( localRangeSize == UINT32_MAX )
    output->fatal( CALL_INFO, -1, "GatewayTopology requires local_range_size to be specified\n" );

  auto remote_id_bases        = parseUintList( params.find<std::string>( "remote_id_bases", "" ) );
  auto remote_range_sizes     = parseUintList( params.find<std::string>( "remote_range_sizes", "" ) );
  auto remote_gateway_rtr_ids = parseUintList( params.find<std::string>( "remote_gateway_rtr_ids", "" ) );
  auto remote_gateway_ports   = parseUintList( params.find<std::string>( "remote_gateway_ports", "" ) );

  if( ( remote_id_bases.size() != remote_range_sizes.size() ) || ( remote_id_bases.size() != remote_gateway_rtr_ids.size() )
      || ( remote_id_bases.size() != remote_gateway_ports.size() ) )
    output->fatal(
      CALL_INFO, -1,
      "GatewayTopology: remote_id_bases (%zu), remote_range_sizes (%zu), remote_gateway_rtr_ids (%zu), and "
      "remote_gateway_ports (%zu) must all be the same length\n",
      remote_id_bases.size(), remote_range_sizes.size(), remote_gateway_rtr_ids.size(), remote_gateway_ports.size()
    );

  remoteRoutes.reserve( remote_id_bases.size() );
  for( size_t i = 0; i < remote_id_bases.size(); i++ )
    remoteRoutes.push_back( RemoteRoute{ remote_id_bases[i], remote_range_sizes[i], remote_gateway_rtr_ids[i], remote_gateway_ports[i] } );

  // Distinct physical ports this router itself hosts. Several remote routes
  // can name the same gateway_rtr_id/gateway_port -- e.g. in a chain A-B-C,
  // A has separate entries for B's range and C's range, but both point at
  // the one physical link to B -- so dedupe by port value, not raw entry
  // count, or a shared port would get counted twice and corrupt
  // inner_num_ports below. The inner topology only ever needs to know about
  // its own direction set plus local ports -- it stays completely unaware
  // it is being wrapped, and never sees (or routes to) these extra ports.
  for( const auto& r : remoteRoutes )
    if( r.gatewayRtrId == rtrId )
      myGatewayPorts.push_back( r.gatewayPort );
  std::sort( myGatewayPorts.begin(), myGatewayPorts.end() );
  myGatewayPorts.erase( std::unique( myGatewayPorts.begin(), myGatewayPorts.end() ), myGatewayPorts.end() );

  const uint32_t inner_num_ports = num_ports - static_cast<uint32_t>( myGatewayPorts.size() );

  // routeUntimedBroadcastPacket() relies on a gateway port being outside the
  // range the inner topology knows about (see its use of a gateway port as
  // an out-of-range sentinel) -- enforce that myGatewayPorts is exactly the
  // trailing contiguous block [inner_num_ports, num_ports), rather than let
  // a misconfigured port silently corrupt the inner topology's own
  // local-port bookkeeping.
  for( size_t i = 0; i < myGatewayPorts.size(); i++ ) {
    if( myGatewayPorts[i] != inner_num_ports + i )
      output->fatal(
        CALL_INFO, -1,
        "GatewayTopology: gateway ports on rtr_id=%" PRIu32 " must be the trailing contiguous block of port "
        "indices [%" PRIu32 ", %" PRIu32 ") -- they must be extra, additive ports, never repurposed "
        "cardinal/local ones (got port=%" PRIu32 " at position %zu)\n",
        rtrId, inner_num_ports, num_ports, myGatewayPorts[i], i
      );
  }

  inner = loadUserSubComponent<TopologyAPI>(
    "inner_topology", ComponentInfo::SHARE_NONE, rtr_id, inner_num_ports, num_local_ports, connected_ports
  );
  if( !inner )
    output->fatal( CALL_INFO, -1, "GatewayTopology: no inner_topology subcomponent in slot 'inner_topology'\n" );

  output->verbose(
    CALL_INFO, MORDRED_VERBOSE_MIN, 0,
    "GatewayTopology constructed; rtr_id=%" PRIu32 ", id_base=%" PRIu32 ", local_range_size=%" PRIu32 ", "
    "remote_routes=%zu, my_gateway_ports=%zu\n",
    rtrId, idBase, localRangeSize, remoteRoutes.size(), myGatewayPorts.size()
  );
}

int32_t GatewayTopology::getEndpointId( uint32_t portnum ) {
  for( const auto p : myGatewayPorts )
    if( p == portnum )
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
  const bool local = ( dest >= idBase ) && ( ( dest - idBase ) < localRangeSize );

  if( local ) {
    // In range: the inner topology only ever knows LOCAL addressing --
    // strip this domain's id_base before delegating. (This is the offset
    // that made an earlier version of this code easy to get wrong: id_base=0
    // makes "local == global", which is exactly the case a first domain
    // often happens to use.)
    return inner->routePacket( dest - idBase );
  }

  for( const auto& r : remoteRoutes ) {
    if( !r.contains( dest ) )
      continue;

    if( r.gatewayRtrId == rtrId ) {
      output->verbose( CALL_INFO, MORDRED_VERBOSE_HIGH, 0,
                       "Routing: dest=%" PRIu32 " belongs to remote domain [%" PRIu32 ",%" PRIu32
                       "); I am its gateway -> port=%" PRIu32 "\n",
                       dest, r.idBase, r.idBase + r.rangeSize, r.gatewayPort );
      return r.gatewayPort;  // hand off to the cross-domain link; dest is untouched
    }

    // Not that domain's gateway: pretend the destination IS the gateway
    // router's own first local endpoint, so the inner topology's own
    // routing algorithm -- whatever it is -- carries us there like any
    // other in-domain packet. This is a LOCAL address for the inner
    // topology's consumption -- no id_base offset here, same as local_dest
    // above.
    const uint32_t proxy_dest = r.gatewayRtrId * numLocalPorts;
    output->verbose( CALL_INFO, MORDRED_VERBOSE_HIGH, 0,
                     "Routing: dest=%" PRIu32 " belongs to remote domain [%" PRIu32 ",%" PRIu32
                     "); redirecting toward gateway_rtr_id=%" PRIu32 " (proxy_dest=%" PRIu32 ")\n",
                     dest, r.idBase, r.idBase + r.rangeSize, r.gatewayRtrId, proxy_dest );
    return inner->routePacket( proxy_dest );
  }

  output->fatal( CALL_INFO, -1,
                 "GatewayTopology: dest=%" PRIu32 " is not local to this domain and does not match any "
                 "configured remote_id_bases/remote_range_sizes entry\n",
                 dest );
  return 0;  // unreachable
}

void GatewayTopology::routeUntimedBroadcastPacket(
  uint32_t receive_port_id, MordredInitEvent* init_ev, std::vector<Event*>& output_events
) {
  for( const auto p : myGatewayPorts ) {
    if( p == receive_port_id ) {
      // Arrived from that remote domain via the cross-domain link -- flood
      // this ENTIRE domain as if it originated at one of my own local
      // endpoints, and do not relay it further (see the class doc comment:
      // relaying on to another remote domain here would double-deliver in
      // a domain graph with a cycle, e.g. a triangle -- the originating
      // domain already relays directly to every domain it's linked to).
      // "p" doubles as the sentinel here: it is guaranteed to be >= the
      // inner topology's own local-port range (by the trailing-contiguous-
      // block check in the constructor) yet never equal to any of its real
      // port indices, so the inner topology's "send to all local endpoints
      // except sender" exclusion never matches a real port -- every real
      // local endpoint in this domain still gets the broadcast -- while its
      // "receive_port_id >= <cardinal-port-count>" check still correctly
      // treats this as an endpoint-style injection, flooding all cardinal
      // directions too.
      inner->routeUntimedBroadcastPacket( p, init_ev, output_events );
      return;
    }
  }

  inner->routeUntimedBroadcastPacket( receive_port_id, init_ev, output_events );

  // This broadcast is circulating in MY domain -- whether it originated at
  // one of my own local endpoints or is arriving from a neighboring router
  // as part of the normal intra-domain flood -- and hasn't been seen on any
  // remote domain yet (arrival FROM a gateway port returned above). Relay it
  // out every gateway port this router hosts so each directly-linked remote
  // domain gets it too. A given broadcast can only reach this router once
  // via the intra-domain flood (each inner topology's flood algorithm is a
  // spanning tree over its own mesh/torus/etc.), so each relay fires
  // exactly once per distinct broadcast, not once per hop.
  for( const auto p : myGatewayPorts ) {
    if( perPortConnectedRtr->at( p ) != UINT32_MAX ) {
      output_events.at( p ) = init_ev->clone();
    }
  }
}

bool GatewayTopology::isWrapAroundOutput( uint32_t output_port ) const {
  return inner->isWrapAroundOutput( output_port );
}
