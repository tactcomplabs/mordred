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

  numVcs = params.find<uint32_t>( "num_vcs", 1 );
  flitWidth = params.find<uint32_t>( "flit_width", 32 );
  channelBusWidth = params.find<uint32_t>( "channel_bus_width", 32 );

  // This constructor should only ever be activated for connected ports (per SimpleRtr constructor)
  // so not checking connectedness here
  const std::string pname = "port" + std::to_string(port_num);
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

  switch( phase ) {
  case 0: {
    auto *init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::REPORT_ROUTER;
    link->sendUntimedData( init_ev );
    output->verbose( CALL_INFO, 5, 0, " REPORT_ROUTER init phase=%" PRIu32 "\n", phase );

    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::ROUTER_ID;
    init_ev->value = rtrId;
    link->sendUntimedData( init_ev );
    output->verbose( CALL_INFO, 5, 0, " REPORT_RTR_ID init phase=%" PRIu32 "\n", phase );

    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::PORT_NUM;
    init_ev->value = portId;
    link->sendUntimedData( init_ev );
    initState = RECV_ID;
    output->verbose( CALL_INFO, 5, 0, " REPORT_PORT_NUM init phase=%" PRIu32 "\n", phase );
    break;
    }

  case 1: {
    Event* ev = link->recvUntimedData();
    if ( ev == nullptr ) {
      output->fatal( CALL_INFO, -1, "Error in %s: unable to recv init event\n", getName().c_str() );
    }
    auto init_ev = static_cast<MordredInitEvent*>(ev);
    if ( init_ev->command == MordredInitEvent::REPORT_ROUTER ) {
      connectionType = ROUTER;
    } else if ( init_ev->command == MordredInitEvent::REPORT_ENDPOINT ) {
      connectionType = ENDPT;
    } else {
      output->fatal( CALL_INFO, -1, "Received packet with unexpected command=%d \n", (int)connectionType );
    }
    delete ev;

    if ( connectionType == ROUTER ) {
      ev = link->recvUntimedData();
      if ( ev == nullptr ) {
        output->fatal( CALL_INFO, -1, "Error in %s: unable to recv init event\n", getName().c_str() );
      }
      init_ev = static_cast<MordredInitEvent*>(ev);
      if ( init_ev->command != MordredInitEvent::ROUTER_ID ) {
        output->fatal( CALL_INFO, -1, "Incoming init event command != ROUTER_ID; =%d\n", (int)init_ev->command );
      }
      connectedRtrId = init_ev->value;
      output->verbose( CALL_INFO, 5, 0, "Received packet with router_id=%" PRIu32 ", phase=%u\n", connectedRtrId, phase );
      delete ev;

      ev = link->recvUntimedData();
      if ( ev == nullptr ) {
        output->fatal( CALL_INFO, -1, "Error in %s: unable to recv second init event\n", getName().c_str() );
      }
      init_ev = static_cast<MordredInitEvent*>(ev);
      if ( init_ev->command != MordredInitEvent::PORT_NUM ) {
        output->fatal( CALL_INFO, -1, "Incoming init event command != PORT_NUM; =%d\n", (int)init_ev->command );
      }
      connectedPortId = init_ev->value;
      output->verbose( CALL_INFO, 5, 0, "Received packet with router_port=%" PRIu32 ", phase=%u\n", connectedPortId, phase );
      delete ev;
    } else if ( connectionType == ENDPT ) {
      init_ev = new MordredInitEvent();
      init_ev->command = MordredInitEvent::NUM_VCS;
      init_ev->value = numVcs;
      link->sendUntimedData( init_ev );

      init_ev = new MordredInitEvent();
      init_ev->command = MordredInitEvent::FLIT_WIDTH;
      init_ev->value = flitWidth;
      link->sendUntimedData( init_ev );

      init_ev = new MordredInitEvent();
      init_ev->command = MordredInitEvent::BUS_WIDTH;
      init_ev->value = channelBusWidth;
      link->sendUntimedData( init_ev );

      output->verbose( CALL_INFO, 5, 0, " Send flit and bus widths init_phase=%" PRIu32 "\n", phase );
    }

    break;
  }

  case 2: {
    if ( connectionType != ENDPT ) {
      output->verbose( CALL_INFO, 5, 0, " connected to non-endpoint; init_phase=%" PRIu32 "\n", phase );
      break;
    }
    auto *init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::ENDPOINT_ID;
    init_ev->value = (uint32_t)topo->getEndpointId( portId );
    link->sendUntimedData( init_ev );
    output->verbose( CALL_INFO, 5, 0, " SEND IDs init phase=%" PRIu32 "\n", phase );
    break;
  }

  default:
    break;
  }
}

void RtrPortControl::setup() {
  output->verbose(CALL_INFO, 5, 0, "RtrPortControl SETUP rtrId=%" PRIu32 ", rtrPort=%" PRIu32 ", connected Rtr ID=%" PRIu32 ", connected Port ID=%" PRIu32 "\n",
    rtrId, portId, connectedRtrId, connectedPortId);
  output->verbose( CALL_INFO, 5, 0, "flitWidth=%" PRIu32 ", channelBusWidth=%" PRIu32 "\n", flitWidth, channelBusWidth );
  output->flush();
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
