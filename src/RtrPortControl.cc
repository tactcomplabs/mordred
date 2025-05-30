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
  std::vector<RtrOwnedVnObj>* vn_objs, uint32_t rtr_num, uint32_t port_num ) :
  RtrPortControlAPI( id ),
  topo( topology ),
  rtrId( rtr_num ),
  portId( port_num ),
  perVnObjs( vn_objs )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new Output("RtrPortControl[[" + std::to_string( rtrId ) + "." + std::to_string( portId ) + "]:@p:@t]: ",
    verbosity, 0, Output::STDOUT);

  if ( perVnObjs == nullptr )
    output->fatal(CALL_INFO_LONG, 1, "RtrPortControl: vn_objs must be specified\n");

  numVns = perVnObjs->size();
  if ( numVns != 1 )
    output->fatal(CALL_INFO_LONG, 1, "RtrPortControl: num_vns must be 1\n");
  numVcs = perVnObjs->at(0).vcHeads.size();

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
  inBufSize = static_cast<uint32_t>( buf_size_ua.getRoundedValue() );

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
  outBufSize = static_cast<uint32_t>( buf_size_ua.getRoundedValue() );

  const std::string pname = "port" + std::to_string(port_num);
  link = configureLink( pname, new Event::Handler2<RtrPortControl, &RtrPortControl::inHandler>( this ) );
  if (!link)
    output->fatal( CALL_INFO, -1, "Error in %s: unable to configure link %s\n", getName().c_str(), pname.c_str() );
  else
    output->verbose( CALL_INFO, 9, 0, "Configured link %s\n", pname.c_str() );

  allocateBuffers();

  output->verbose( CALL_INFO, 1, 0, "Constructor complete; [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "], inbuf_size=%" PRIu32 ", outbuf_size=%" PRIu32 "\n",
    rtrId, portId, inBufSize, outBufSize);
}

// Note: It may be "better" to do this in the init phase once we know what the other end of the link is
void RtrPortControl::allocateBuffers() {
  auto credits = outBufSize / flitSize ;

  inStates.resize( numVns );
  outStates.resize( numVns );
  inBuf.resize( numVns );
  outBuf.resize( numVns );
  destCredits.resize( numVns );
  outBufCredits.resize( numVns );
  inRetCredits.resize( numVns );

  // Allocate/init the second dimension
  for ( uint32_t i = 0; i < numVcs; i++ ) {
    inStates.at(i).resize( numVcs, IN_IDLE );
    outStates.at(i).resize( numVcs, OUT_IDLE );
    inBuf.at(i).resize( numVcs );
    outBuf.at(i).resize( numVcs );
    destCredits.at(i).resize( numVcs, 0 );
    outBufCredits.at(i).resize( numVcs, static_cast<int32_t>( credits ) );
    inRetCredits.at(i).resize( numVcs, 0 );
  }
}

void RtrPortControl::init( unsigned int phase ) {
  //output->verbose( CALL_INFO, 5, 0, " init phase=%" PRIu32 "\n", phase );

  // Similar to MordredNIC, could set this up to use sendUntimedData here instead of using the
  // link directly
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
      connectionType = ENDPOINT;
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
    } else if ( connectionType == ENDPOINT ) {
      init_ev = new MordredInitEvent();
      init_ev->command = MordredInitEvent::NUM_VNS;
      init_ev->value = numVns;
      link->sendUntimedData( init_ev );

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
    if ( connectionType != ENDPOINT ) {
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
    auto credits    = static_cast<int32_t>( inBufSize / flitSize );
    // Need to do things a little differently here depending on if I'm going to an endpoint or
    // another router
    uint32_t max_vc = ( connectionType == ROUTER ) ? numVcs : 1;
    for( uint32_t i = 0; i < numVns; i++ ) {
      for( uint32_t j = 0; j < max_vc; j++ ) {
        auto* credit_ev = new MordredCreditEvent( i, j, credits );
        link->sendUntimedData( credit_ev );
      }
    }
  } break;

  default: {
    // receive credits and anything else
    Event* ev = nullptr;
    while( ( ev = link->recvUntimedData() ) != nullptr ) {
      auto base_ev = static_cast<baseMordredEvent*>( ev );
      if( base_ev->getType() == baseMordredEvent::CREDIT ) {
        auto credit_ev = static_cast<MordredCreditEvent*>( ev );
        destCredits.at( credit_ev->vn ).at ( credit_ev->vc ) += credit_ev->credits;
        output->verbose( CALL_INFO, 5, 0, "Received credit event vc=%d, credits=%d; cur_credits=%d\n", credit_ev->vc, credit_ev->credits, destCredits.at( credit_ev->vn ).at( credit_ev->vc ) );
      } else {
        output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
      }
      delete ev;
    }
  }  // end default
  }
}

void RtrPortControl::setup() {
#if 0
  output->verbose(CALL_INFO, 5, 0, "RtrPortControl SETUP rtrId=%" PRIu32 ", rtrPort=%" PRIu32 ", connected Rtr ID=%" PRIu32 ", connected Port ID=%" PRIu32 "\n",
    rtrId, portId, connectedRtrId, connectedPortId);
  output->verbose( CALL_INFO, 5, 0, "flitWidth=%" PRIu32 ", channelBusWidth=%" PRIu32 "\n", flitSize, channelBusWidth );
  output->flush();
#endif
}

void RtrPortControl::sendUntimedData( Event* ev ) {
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
}

SST::Event* RtrPortControl::recvUntimedData() {
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return nullptr;
}

void RtrPortControl::ClockTick( Cycle_t cycle ) {
  //output->verbose( CALL_INFO, 3, 0, "Tick; cycle=%" PRIu64 "\n", cycle );
  //output->flush();

  // Fill all possible vcHeads from the input buffer
  for ( uint32_t vn = 0; vn < numVns; vn++ ) {
    for ( uint32_t vc = 0; vc < numVcs; vc++ ) {
      if ( ( perVnObjs->at(vn).vcHeads[vc] == nullptr ) && ( !inBuf.at( vn ).at( vc ).empty() ) ) {
        perVnObjs->at(vn).vcHeads[vc] = inBuf.at( vn ). at( vc ).front();
        //output->verbose( CALL_INFO, 5, 0, "Filling vcHeads[vc=%d] for vn=%d\n", vc, vn );
      }
    }
  }

  // Send a flit on the link from the output buffer
  bool sent = false;
  for( uint32_t i = 0, vn = flit_vn_rr; i < numVns; i++, vn = ( ( vn != ( numVns - 1 ) ) ? vn + 1 : 0 ) ) {
    for( uint32_t j = 0, vc = flit_vc_rr; j < numVcs; j++, vc = ( ( vc != ( numVcs - 1 ) ) ? vc + 1 : 0 ) ) {
      if ( ( !outBuf.at( vn ).at( vc ).empty() ) &&
           ( destCredits.at( vn ).at( vc ) > 0 ) ) { // ensure there are dest credits
        auto flit = outBuf.at( vn ).at( vc ).front();
        outBuf.at( vn ).at( vc ).pop();
        link->send( flit );
        sent = true;
        destCredits.at( vn ).at( vc )--;
        //output->verbose( CALL_INFO, 5, 0, "Sending output flit; remaining_credits=%" PRId32 "\n", destCredits.at( vn ).at(vc) );
        break; // can only send one flit out on the link
      }
    }
  }
  flit_vc_rr = ( flit_vc_rr + 1 ) % numVcs;
  flit_vn_rr = ( flit_vn_rr + 1 ) % numVns;

  // Try returning credits if we haven't used the link
  if ( !sent )
    returnCredit();
}

void RtrPortControl::inHandler( SST::Event* ev ) {

  //output->verbose( CALL_INFO, 5, 0, "Handling an input event" );
  //output->flush();
  auto bev = static_cast<baseMordredEvent*>( ev );
  if ( bev == nullptr ) {
    output->fatal( CALL_INFO, -1, "Null event\n" );
  }
  switch( bev->getType() ) {
  case baseMordredEvent::CREDIT: {
    auto credit = static_cast<MordredCreditEvent*>( bev );
    destCredits.at( credit->vn ).at( credit->vc ) += credit->credits;
    output->verbose( CALL_INFO, 5, 0, "Received %" PRId32 " credits to vc=%" PRIu32 ", cur_credits=%" PRIu32 "\n",
      credit->credits, credit->vc, destCredits.at( credit->vn ).at( credit->vc ) );
    delete bev;
    break;
  } // end CREDIT
  case baseMordredEvent::FLIT: {
    auto *flit = static_cast<MordredFlit*>( ev );
    if ( flit == nullptr )
      output->fatal( CALL_INFO, -1, "Invalid flit \n" );

    auto *simple = static_cast<simpleTestEvent*>( flit->req->inspectPayload() ); // only needed for the print statement
    flit->next_port = topo->routePacket( (uint32_t)flit->req->dest );
    output->verbose( CALL_INFO, 5, 0, "Recv Flit; str=%s, src=%" PRIu64 ", dst=%" PRIu64 ", size=%zu, dest_port=%" PRIu32 "\n",
      simple->str.c_str(), flit->req->src, flit->req->dest, flit->req->size_in_bits, flit->next_port );

    inBuf.at( flit->vn ).at( flit->cur_vc ).push( flit );
    break;
  } // end FLIT
  default:
    output->fatal( CALL_INFO, -1, "Unknown/unimplemented event type=%d\n", (int) bev->getType() );
  }  // end switch
}

MordredFlit* RtrPortControl::getInBufFlit( std::pair<uint32_t, uint32_t> vn_vc ) {

  // Get the flit to return
  uint32_t vn = vn_vc.first;
  uint32_t vc = vn_vc.second;
  if ( inBuf.at( vn ).at( vc ).empty() ) {
    output->flush();
    output->fatal( CALL_INFO, 5, "InBuf empty; vc=%d\n", vc );
  }
  MordredFlit* flit = inBuf.at( vn ).at( vc ).front();
  inBuf.at( vn ).at( vc ).pop();

  if ( flit == nullptr )
    output->fatal( CALL_INFO, -1, "Invalid flit \n" );

  // If we're at the tail, clear for the next packet
  if ( flit->ftype == MordredFlit::TAIL ) {
    perVnObjs->at( vn ).vcHeads[ vc ] = nullptr;
    // The output state for this port is reset in SimpleRTR rather than here
  }

  // Can return a credit to the sender
  inRetCredits.at( vn ).at( vc )++;

  auto test_ev = static_cast<simpleTestEvent*>( flit->req->inspectPayload() );
  output->verbose( CALL_INFO, 5, 0, "Get Flit from inBuf; str=%s, src=%" PRIu64 ", dst=%" PRIu64 ", size=%zu, dest_port=%" PRIu32 "\n",
    test_ev->str.c_str(), flit->req->src, flit->req->dest, flit->req->size_in_bits, flit->next_port );

  return flit;
}

void RtrPortControl::sendOutBufFlit( MordredFlit* flit, std::pair<uint32_t, uint32_t> vn_vc ) {
  uint32_t vn = vn_vc.first;
  uint32_t vc = vn_vc.second;
  outBuf.at( vn ).at( vc ).push( flit );
  // Below here is for debug purposes
  SST::Event* ev = flit->req->inspectPayload();
  auto test_ev = static_cast<simpleTestEvent*>( ev );
  output->verbose( CALL_INFO, 5, 0, "Put Flit in outBuf; str=%s, src=%" PRIu64 ", dst=%" PRIu64 ", size=%zu, dest_port=%" PRIu32 "\n",
    test_ev->str.c_str(), flit->req->src, flit->req->dest, flit->req->size_in_bits, flit->next_port );
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

void RtrPortControl::returnCredit() {
  for( uint32_t i = 0, vn = credit_ret_vn_rr; i < numVns; i++, vn = ( ( vn != ( numVns - 1 ) ) ? vn + 1 : 0 ) ) {
    for( uint32_t j = 0, vc = credit_ret_vc_rr; j < numVcs; j++, vc = ( ( vc != ( numVcs - 1 ) ) ? vc + 1 : 0 ) ) {
      if( inRetCredits.at( vn ).at ( vc ) != 0 ) {
        auto credit = new MordredCreditEvent( vn, vc, inRetCredits.at( vn ).at( vc ) );
        link->send( credit );
        inRetCredits.at( vn ).at( vc ) = 0;
        //output->verbose( CALL_INFO, 5, 0, "Sending credit event\n" );
        break;  // only send one packet on the link
      }
    }
  }
  credit_ret_vc_rr = ( credit_ret_vc_rr + 1 ) % numVcs;
  credit_ret_vn_rr = ( credit_ret_vn_rr + 1 ) % numVns;
}
