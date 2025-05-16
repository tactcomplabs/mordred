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

/*
RtrPortControl::RtrPortControl( ComponentId_t id, Params& params, TopologyAPI* topology,
  InVcHeads *vc_heads, uint32_t rtr_num, uint32_t port_num ) :
  RtrPortControlAPI( id ),
  topo( topology ),
  rtrId( rtr_num ),
  portId( port_num ),
  vcHeads( vc_heads )
  */
RtrPortControl::RtrPortControl( ComponentId_t id, Params& params, TopologyAPI* topology,
  std::vector<MordredFlit*>* vc_heads, uint32_t rtr_num, uint32_t port_num ) :
  RtrPortControlAPI( id ),
  topo( topology ),
  rtrId( rtr_num ),
  portId( port_num ),
  vcHeads( vc_heads )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new SST::Output("RtrPortControl[[" + std::to_string( rtrId ) + "." + std::to_string( portId ) + "]:@p:@t]: ", verbosity, 0, Output::STDOUT);

  numVcs = params.find<uint32_t>( "num_vcs", 1 );
  auto flit_size_ua = params.find<UnitAlgebra>( "flit_size", "32b" );
  if ( !flit_size_ua.hasUnits("b") && !flit_size_ua.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"PortControl: flit_size must be specified in either "
                       "bits (b) or bytes (B): %s\n",flit_size_ua.toStringBestSI().c_str());
  }
  if ( flit_size_ua.hasUnits("B") ) {
    flit_size_ua *= UnitAlgebra("8b");
  }
  flitSize = static_cast<uint32_t>( flit_size_ua.getRoundedValue() );
  channelBusWidth = params.find<uint32_t>( "channel_bus_width", flitSize );

  // Get buffer sizes
  bool found = false;
  auto buf_size_ua = params.find<UnitAlgebra>("input_buf_size",found);
  if ( !found ) {
    output->fatal(CALL_INFO_LONG, 1, "RtrPortControl: input_buf_size must be specified\n");
  }
  if ( !buf_size_ua.hasUnits("b") && !buf_size_ua.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"RtrPortControl: input_buf_size must be specified in either "
                       "bits (b) or bytes (B): %s\n",buf_size_ua.toStringBestSI().c_str());
  }
  if ( buf_size_ua.hasUnits("B") ) {
    buf_size_ua *= UnitAlgebra("8b/B");
  }
  inbuf_size = static_cast<uint32_t>( buf_size_ua.getRoundedValue() );

  buf_size_ua = params.find<UnitAlgebra>("output_buf_size",found);
  if ( !found ) {
    output->fatal(CALL_INFO_LONG, 1, "RtrPortControl: output_buf_size must be specified\n");
  }
  if ( !buf_size_ua.hasUnits("b") && !buf_size_ua.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"RtrPortControl: inbuf_size must be specified in either "
                       "bits (b) or bytes (B): %s\n",buf_size_ua.toStringBestSI().c_str());
  }
  if ( buf_size_ua.hasUnits("B") ) {
    buf_size_ua *= UnitAlgebra("8b/B");
  }
  outbuf_size = static_cast<uint32_t>( buf_size_ua.getRoundedValue() );


  // This constructor should only ever be activated for connected ports (per SimpleRtr constructor)
  // so not checking connectedness here
  const std::string pname = "port" + std::to_string(port_num);
  link = configureLink( pname, new Event::Handler2<RtrPortControl, &RtrPortControl::inHandler>( this ) );
  if (!link)
    output->fatal( CALL_INFO, -1, "Error in %s: unable to configure link %s\n", getName().c_str(), pname.c_str() );
  else
    output->verbose( CALL_INFO, 9, 0, "Configured link %s\n", pname.c_str() );

  // Size perVC structures
  auto credits = outbuf_size / flitSize ;
  in_buf.resize( numVcs );
  out_buf.resize( numVcs );
  dest_credits.resize( numVcs, 0 );
  outbuf_credits.resize( numVcs, static_cast<int32_t>( credits ) );
  in_ret_credits.resize( numVcs, 0 );
  inStates.resize( numVcs, IN_IDLE );
  outStates.resize( numVcs, OUT_IDLE );

  output->verbose( CALL_INFO, 1, 0, "Constructor complete; [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "], inbuf_size=%" PRIu32 ", outbuf_size=%" PRIu32 "\n",
    rtrId, portId, inbuf_size, outbuf_size);
}

void RtrPortControl::init( unsigned int phase ) {
  //output->verbose( CALL_INFO, 5, 0, " init phase=%" PRIu32 "\n", phase );

  switch( phase ) {
  case 0: {
    auto *init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::REPORT_ROUTER;
    link->sendUntimedData( init_ev );

    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::ROUTER_ID;
    init_ev->value = rtrId;
    link->sendUntimedData( init_ev );

    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::PORT_NUM;
    init_ev->value = portId;
    link->sendUntimedData( init_ev );
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
      init_ev = getInitEvent( MordredInitEvent::ROUTER_ID );
      connectedRtrId = init_ev->value;
      delete init_ev;

      init_ev = getInitEvent( MordredInitEvent::PORT_NUM );
      connectedPortId = init_ev->value;
      output->verbose( CALL_INFO, 5, 0, "Received init packets from [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "]\n", connectedRtrId, connectedPortId );
      delete init_ev;
    } else if ( connectionType == ENDPT ) {
      init_ev = new MordredInitEvent();
      init_ev->command = MordredInitEvent::NUM_VCS;
      init_ev->value = numVcs;
      link->sendUntimedData( init_ev );

      init_ev = new MordredInitEvent();
      init_ev->command = MordredInitEvent::FLIT_WIDTH;
      init_ev->value = flitSize;
      link->sendUntimedData( init_ev );

      init_ev = new MordredInitEvent();
      init_ev->command = MordredInitEvent::BUS_WIDTH;
      init_ev->value = channelBusWidth;
      link->sendUntimedData( init_ev );

      output->verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, " Send flit and bus widths init_phase=%" PRIu32 "\n", phase );
    }
    break;
  }

  case 2: {
    if ( connectionType != ENDPT ) {
      output->verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, " connected to non-endpoint; init_phase=%" PRIu32 "\n", phase );
      break;
    }
    auto *init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::ENDPOINT_ID;
    init_ev->value = (uint32_t)topo->getEndpointId( portId );
    link->sendUntimedData( init_ev );
    output->verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, " SEND IDs init phase=%" PRIu32 "\n", phase );
    break;
  }

  case 3: {
    // Send router credits equal to num_flits in_buf can hold
    auto credits = static_cast<int32_t>( inbuf_size / flitSize );
    for( uint32_t i = 0; i < numVcs; i++ ) {
      auto* credit_ev = new MordredCreditEvent( i, credits );
      link->sendUntimedData( credit_ev );
    }
  }

  default: {
    // receive credits and anything else
    Event* ev = nullptr;
    while( ( ev = link->recvUntimedData() ) != nullptr ) {
      auto base_ev = static_cast<baseMordredEvent*>( ev );
      if( base_ev->getType() == baseMordredEvent::CREDIT ) {
        auto credit_ev = static_cast<MordredCreditEvent*>( ev );
        dest_credits.at( credit_ev->vc ) += credit_ev->credits;
        output->verbose( CALL_INFO, 5, 0, "Received credit event vc=%d, credits=%d; cur_credits=%d\n", credit_ev->vc, credit_ev->credits, dest_credits.at( credit_ev->vc ) );
      } else {
        output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
      }
      delete ev;
    }
  }  // end default
  }
}

void RtrPortControl::setup() {
  //output->verbose(CALL_INFO, 5, 0, "RtrPortControl SETUP rtrId=%" PRIu32 ", rtrPort=%" PRIu32 ", connected Rtr ID=%" PRIu32 ", connected Port ID=%" PRIu32 "\n",
  //  rtrId, portId, connectedRtrId, connectedPortId);
  //output->verbose( CALL_INFO, 5, 0, "flitWidth=%" PRIu32 ", channelBusWidth=%" PRIu32 "\n", flitSize, channelBusWidth );
  //output->flush();
}

void RtrPortControl::sendUntimedData( Event* ev ) {
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
}

SST::Event* RtrPortControl::recvUntimedData() {
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return nullptr;
}

void RtrPortControl::inHandler( SST::Event* ev ) {
  auto *flit = static_cast<MordredFlit*>( ev );
  if ( flit == nullptr )
    output->fatal( CALL_INFO, -1, "Invalid flit \n" );

  auto *simple = static_cast<simpleTestEvent*>( flit->req->inspectPayload() );
  flit->next_port = topo->routePacket( (uint32_t)flit->dest );
  output->verbose( CALL_INFO, 5, 0, "Recv Flit; str=%s, src=%" PRIu64 ", dst=%" PRIu64 ", size=%zu, dest_port=%" PRIu32 "\n",
    simple->str.c_str(), flit->src, flit->dest, flit->req->size_in_bits, flit->next_port );
  if ( vcHeads->at(0) == nullptr )
    vcHeads->at(0) = flit;
}

MordredInitEvent* RtrPortControl::getInitEvent( MordredInitEvent::Commands cmd ) {
  Event *ev;
  ev = link->recvUntimedData();
  if ( ev == nullptr ) {
    output->fatal( CALL_INFO, -1, "Error in %s: unable to recv init event\n", getName().c_str() );
  }
  auto init_ev = static_cast<MordredInitEvent*>(ev);
  if ( init_ev->getType() != baseMordredEvent::INITIALIZATION ) {
    output->fatal( CALL_INFO, -1, "Incoming event type != %d; =%d\n",
      baseMordredEvent::INITIALIZATION, (int)init_ev->getType() );
  }
  if ( init_ev->command != cmd ) {
    output->fatal( CALL_INFO, -1, "Incoming init event command != %d; =%d\n", (int)cmd, (int)init_ev->command );
  }
  return init_ev;
}
