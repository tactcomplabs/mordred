//
// RtrPortControl.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//
//

#include <string>
#include <cinttypes>

// Local SST config
#include "sst_config.h"

#include "MordredEvents.h"
#include "RtrPortControl.h"
#include "TopologyAPI.h"

using namespace SST::Mordred;

RtrPortControl::RtrPortControl( ComponentId_t id, Params& params, TopologyAPI* topology,
  uint32_t rtr_num, uint32_t port_num ) :
  RtrPortControlAPI( id ),
  topo( topology ),
  rtrId( rtr_num ),
  portId( port_num )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new SST::Output("RtrPortControl[" + getName() + ":@p:@t]: ", verbosity, 0, Output::STDOUT);

  // This constructor should only ever be activated for connected ports (per SimpleRtr constructor)
  // so not checking connectedness here
  std::string pname = "port" + std::to_string(port_num);
  link = configureLink( pname, new Event::Handler2<RtrPortControl, &RtrPortControl::inHandler>( this ) );
  if (!link)
    output->fatal( CALL_INFO, -1, "Error in %s: unable to configure link %s\n", getName().c_str(), pname.c_str() );
  else
    output->verbose( CALL_INFO, 9, 0, "Configured link %s\n", pname.c_str() );

  output->verbose( CALL_INFO, 1, 0, "Constructor complete; rtr_id=%" PRIu32 ", port_num=%" PRIu32 "\n",
    rtrId, portId);
}

void RtrPortControl::init( unsigned int phase ) {
  output->verbose( CALL_INFO, 5, 0, " init phase=%" PRIu32 "\n", phase );
  // In the early phases, we want to recvUntimedPackets from the endpoints to know that we're connected
  // to an endpoint

  // Then we can send a packet to our neighbor to identify ourself (or at least the router id)

  // At a higher level, we'll have to start adding up the number of endpoints and then assigning endpt ids
  switch( initState ) {
  case REPORT_RTR_ID:{
    auto *init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::ROUTER_ID;
    init_ev->value = (int32_t)rtrId;
    link->sendUntimedData( init_ev );
    initState = RECV_ID;
    output->verbose( CALL_INFO, 5, 0, " REPORT_RTR_ID init phase=%" PRIu32 "\n", phase );
    break;
  }

  case RECV_ID: {
    Event* ev = link->recvUntimedData();
    if (ev == nullptr) break;
    auto *init_ev = static_cast<MordredInitEvent*>(ev);
    if ( init_ev->command == MordredInitEvent::ROUTER_ID ) {
      connectionType = TopologyAPI::PortConnectionE::ROUTER;
    } else if ( init_ev->command == MordredInitEvent::REPORT_ENDPOINT ) {
      connectionType = TopologyAPI::PortConnectionE::ENDPT;
    } else {
      output->fatal( CALL_INFO, -1, "Received packet with unexpected command=%d \n", (int)connectionType );
    }
    connectionId = (uint32_t)init_ev->value;
    output->verbose( CALL_INFO, 5, 0, "Received packet with command=%d and id=%" PRIu32 "\n", (int)init_ev->command, connectionId );
    output->verbose( CALL_INFO, 5, 0, " RECV_ID init phase=%" PRIu32 "\n", phase );
    delete ev;
    initState = SEND_ENDPT_IDS;
    if ( connectionType != TopologyAPI::PortConnectionE::ENDPT )
      break;
    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::ENDPOINT_ID;
    init_ev->value = topo->getEndpointId( portId );
    link->sendUntimedData( init_ev );
    initState = NUM_STATES;
    output->verbose( CALL_INFO, 5, 0, " SEND IDs init phase=%" PRIu32 "\n", phase );
    break;
  }

  case SEND_ENDPT_IDS: {
    if ( connectionType != TopologyAPI::PortConnectionE::ENDPT )
      break;
    auto *init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::ENDPOINT_ID;
    init_ev->value = topo->getEndpointId( portId );
    link->sendUntimedData( init_ev );
    initState = NUM_STATES;
    output->verbose( CALL_INFO, 5, 0, " SEND IDs init phase=%" PRIu32 "\n", phase );
    break;
  }

  default:
    break;
  }
}

void RtrPortControl::sendUntimedData( Event* ev ) {
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
}

SST::Event* RtrPortControl::recvUntimedData() {
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return nullptr;
}

void RtrPortControl::inHandler( SST::Event* ev ) {
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
}
