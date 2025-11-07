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
  RtrOwnedSharedObjs* rtr_shared_objs, uint32_t rtr_num, uint32_t port_num ) :
  RtrPortControlAPI( id ),
  topo( topology ),
  rtrId( rtr_num ),
  portId( port_num ),
  rtrSharedObjs( rtr_shared_objs )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new Output("RtrPortControl[[" + std::to_string( rtrId ) + "." + std::to_string( portId ) + "]:@p:@t]: ",
    verbosity, 0, Output::STDOUT);
  output->setVerboseMask( DEBUG_INIT_PHASE );

  if ( rtrSharedObjs == nullptr )
    output->fatal(CALL_INFO_LONG, 1, "RtrPortControl: vn_objs must be specified\n");

  numVns = rtrSharedObjs->needVcAlloc.size();
  if ( numVns != 1 )
    output->fatal(CALL_INFO_LONG, 1, "RtrPortControl: num_vns must be 1\n");
  numVcs = rtrSharedObjs->needVcAlloc.at(0).size();

  auto flit_size_ua = params.find<UnitAlgebra>( "flit_size", "32b" );
  if ( !flit_size_ua.hasUnits("b") && !flit_size_ua.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"PortControl: flit_size must be specified in either "
                       "bits (b) or bytes (B): %s\n",flit_size_ua.toStringBestSI().c_str());
  }
  if ( flit_size_ua.hasUnits("B") ) {
    flit_size_ua *= UnitAlgebra("8b");
  }
  flitSize = static_cast<uint32_t>( flit_size_ua.getRoundedValue() );
  channelWidth = params.find<uint32_t>( "channel_width", flitSize );

  if ( channelWidth > flitSize ) {
    output->fatal( CALL_INFO, 1, "RtrPortControl: flit_size must be greater than or equal to channel_width\n" );
  }

  if ( (flitSize % channelWidth) != 0) {
    channelWidth = flitSize / channelWidth; // force to round down
    output->output( CALL_INFO, "WARNING: flit_size%%channel_width != 0; new channel_width=%" PRIu32 "\n",
      channelWidth);
    output->flush();
  }

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
  //TODO: An other validations/size checks?
  if ( flitSize > inBufSize ) {
    output->fatal( CALL_INFO, 1, "Invalid configuration; flit_size=%" PRIu32 "b > input_buf_size=%" PRIu32 "b (buf cannot hold a flit)\n",
      flitSize, inBufSize);
  }

  buf_size_ua = params.find<UnitAlgebra>("output_buf_size",found);
  if ( !found ) {
    output->fatal(CALL_INFO_LONG, 1, "RtrPortControl: output_buf_size must be specified\n");
  }
  if ( !buf_size_ua.hasUnits("b") && !buf_size_ua.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"RtrPortControl: outbuf_size must be specified in either "
                       "bits (b) or bytes (B): %s\n",buf_size_ua.toStringBestSI().c_str());
  }
  if ( buf_size_ua.hasUnits("B") ) {
    buf_size_ua *= UnitAlgebra("8b/B");
  }
  outBufSize = static_cast<uint32_t>( buf_size_ua.getRoundedValue() );
  //TODO: An other validations/size checks?
  if ( flitSize > outBufSize ) {
    output->fatal( CALL_INFO, 1, "Invalid configuration; flit_size=%" PRIu32 "b > output_buf_size=%" PRIu32 "b (buf cannot hold a flit)\n",
      flitSize, outBufSize);
  }

  const std::string pname = "port" + std::to_string(port_num);
  link = configureLink( pname, new Event::Handler2<RtrPortControl, &RtrPortControl::inHandler>( this ) );
  if (!link)
    output->fatal( CALL_INFO, -1, "Error in %s: unable to configure link %s\n", getName().c_str(), pname.c_str() );
  else
    output->verbose( CALL_INFO, 9, 0, "Configured link %s\n", pname.c_str() );

  allocateBuffers();

  output->verbose( CALL_INFO, 1, 0, "Constructor complete; [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "], inbuf_size=%" PRIu32 "b, outbuf_size=%" PRIu32 "b\n",
    rtrId, portId, inBufSize, outBufSize);
}

// Note: It may be "better" to do this in the init phase once we know what the other end of the link is
void RtrPortControl::allocateBuffers() {
  auto total_credits = outBufSize / flitSize ;

  inStateVec.resize( numVns );
  outStateVec.resize( numVns );
  statLinkRecvFlitCnt.resize( numVns );
  statLinkSentFlitCnt.resize( numVns );
  statLinkSentPacketCnt.resize( numVns );

  // Allocate/init the second dimension
  for ( uint32_t i = 0; i < numVns; i++ ) {
    inStateVec.at( i ).resize( numVcs );
    outStateVec.at( i ).resize( numVcs );
    statLinkRecvFlitCnt.at(i).resize( numVcs, nullptr );
    statLinkSentFlitCnt.at(i).resize( numVcs, nullptr );
    statLinkSentPacketCnt.at(i).resize( numVcs, nullptr );
  }

  // Init stuff as needed
  for ( uint32_t i = 0; i < numVns; i++ ) {
    for ( uint32_t j = 0; j < numVcs; j++ ) {
      inStateVec.at(i).at(j).reset();
      outStateVec.at(i).at(j).reset( static_cast<int16_t>( total_credits ) );
    }
  }

  // Register stats
  for (uint32_t i = 0; i < numVns; i++ ) {
    for (uint32_t j = 0; j < numVcs; j++ ) {
      std::string str = std::to_string( rtrId ) + "_" + std::to_string( portId ) + "_" + std::to_string(i) + "_" + std::to_string(j);
      statLinkRecvFlitCnt.at( i ).at( j ) = registerStatistic<uint64_t>( "recv_flit_cnt", str.c_str() );
      statLinkSentFlitCnt.at( i ).at( j ) = registerStatistic<uint64_t>( "sent_flit_cnt", str.c_str() );
      statLinkSentPacketCnt.at( i ).at( j ) = registerStatistic<uint64_t>( "sent_packet_cnt", str.c_str() );
    }
  }
  statLinkOutputStalledCnt = registerStatistic<uint64_t>( "output_stalls" );
}

void RtrPortControl::init( unsigned int phase ) {
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
      //output->verbose( CALL_INFO, 5, 0, "Received init packets from [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "]\n", connectedRtrId, connectedPortId );
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
      init_ev->command = MordredInitEvent::CHANNEL_WIDTH;
      init_ev->value = channelWidth;
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
    // IDLE
  } break;

  case 4: {
    if ( connectionType == ENDPOINT ) {
      auto *init_ev = getInitEvent( MordredInitEvent::AGREED_CHANNEL_WIDTH );
      channelWidth = init_ev->value;
      delete init_ev;
    }

    // Send router credits
    auto total_credits    = static_cast<int32_t>( inBufSize / flitSize );
    // Need to do things a little differently here depending on if I'm going to an endpoint or
    // another router
    uint32_t max_vc = ( connectionType == ROUTER ) ? numVcs : 1;
    for( uint32_t i = 0; i < numVns; i++ ) {
      for( uint32_t j = 0; j < max_vc; j++ ) {
        auto* credit_ev = new MordredCreditEvent( i, j, total_credits );
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
        outStateVec.at( credit_ev->vn ).at ( credit_ev->vc ).destCredits += credit_ev->credits;
        //output->verbose( CALL_INFO, 5, 0, "Received initCredit event vc=%d, credits=%d; cur_credits=%d\n", credit_ev->vc, credit_ev->credits, outStateVec.at( credit_ev->vn ).at ( credit_ev->vc ).destCredits );
        delete ev;
      } else if ( base_ev->getType() == baseMordredEvent::PACKET ) {
        initEvents.push( ev );
        //auto init_ev = static_cast<MordredInitEvent*>( ev );
        //output->verbose( CALL_INFO, 5, 0, "Received startup packet; src=%" PRIu64 "; dest=%" PRIu64 "\n",
        //  init_ev->req->src, init_ev->req->dest );
        //output->flush();
        //delete ev;
      } else {
        output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
        delete ev;
      }
    }
  }  // end default
  }
  //output->verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, " END init phase=%" PRIu32 "\n", phase );
  //output->flush();
}

void RtrPortControl::setup() {
#if 0
  output->verbose(CALL_INFO, 5, 0, "RtrPortControl SETUP rtrId=%" PRIu32 ", rtrPort=%" PRIu32 ", connected Rtr ID=%" PRIu32 ", connected Port ID=%" PRIu32 "\n",
    rtrId, portId, connectedRtrId, connectedPortId);
  output->verbose( CALL_INFO, 5, 0, "flitWidth=%" PRIu32 ", channelBusWidth=%" PRIu32 "\n", flitSize, channelWidth );
  output->flush();
#endif
}

void RtrPortControl::complete( unsigned int phase ) {
  Event* ev = nullptr;
  while( ( ev = link->recvUntimedData() ) != nullptr ) {
    auto base_ev = static_cast<baseMordredEvent*>( ev );
    if( base_ev->getType() == baseMordredEvent::CREDIT ) {
      auto credit_ev = static_cast<MordredCreditEvent*>( ev );
      outStateVec.at( credit_ev->vn ).at ( credit_ev->vc ).destCredits += credit_ev->credits;
      //output->verbose( CALL_INFO, 5, 0, "Received initCredit event vc=%d, credits=%d; cur_credits=%d\n", credit_ev->vc, credit_ev->credits, outStateVec.at( credit_ev->vn ).at ( credit_ev->vc ).destCredits );
      delete ev;
    } else if ( base_ev->getType() == baseMordredEvent::PACKET ) {
      initEvents.push( ev );
      //auto init_ev = static_cast<MordredInitEvent*>( ev );
      //output->verbose( CALL_INFO, 5, 0, "Received startup packet; src=%" PRIu64 "; dest=%" PRIu64 "\n",
      //  init_ev->req->src, init_ev->req->dest );
      //output->flush();
      //delete ev;
    } else {
      output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
      delete ev;
    }
  }
}


void RtrPortControl::sendUntimedData( Event* ev ) {
  link->sendUntimedData( ev );
  //output->verbose( CALL_INFO, 5, 0, "Sent untimed data\n");
  //output->flush();
}

SST::Event* RtrPortControl::recvUntimedData() {
  if ( initEvents.empty() )
    return nullptr;
  auto event = initEvents.front();
  initEvents.pop();
  return event;
}

void RtrPortControl::ClockTick( Cycle_t cycle ) {
  //output->verbose( CALL_INFO, 3, 0, "Tick; cycle=%" PRIu64 "\n", cycle );
  //output->flush();

  // TODO: This also assumes one cycle each for VC and switch allocation - might need a method for speeding that up
  // probably going to make more sense to break this up into multiple functions, then the router can handle that

  // TODO: Figure out where to do the state transition in/out of WAIT_CREDITS; do we need??
  for( uint32_t vn = 0; vn < numVns; vn++ ) {
    for( uint32_t vc = 0; vc < numVcs; vc++ ) {
      // Going to throw some extra sanity checks in for now
      if ( inStateVec.at( vn ).at( vc ).inVcState == IN_IDLE ) {
        // Ready to start the next packet through the pipeline
        if ( rtrSharedObjs->needVcAlloc.at(vn).at(vc) != nullptr )
          output->fatal( CALL_INFO, -1, "Expected nullptr\n" );
        if ( !inStateVec.at( vn ).at( vc ).inBuf.empty() ) {
          auto *flit = inStateVec.at( vn ).at( vc ).inBuf.front();
          if ( flit->ftype != MordredFlit::HEAD )
            output->fatal( CALL_INFO, -1, "Expected head flit\n" );
          inStateVec.at(vn).at(vc).inVcState = ROUTING;
          inStateVec.at(vn).at(vc).outPort = topo->routePacket( (uint32_t)flit->req->dest );
          //output->verbose( CALL_INFO, 5, 0, "Flit %s routed\n", flit->pktIdStr().c_str() );
        }
      } else if ( inStateVec.at( vn ).at( vc ).inVcState == ROUTING ) {
        // Ready for VC allocation
        auto *flit = inStateVec.at( vn ).at( vc ).inBuf.front();
        //output->verbose( CALL_INFO, 5, 0, "Flit %s ready for VC alloc\n", flit->pktIdStr().c_str() );
        inStateVec.at(vn).at(vc).inVcState = WAIT_VC;
        rtrSharedObjs->needVcAlloc.at(vn).at(vc) = flit;
      } else if ( ( inStateVec.at( vn ).at( vc ).inVcState == WAIT_VC ) &&
                  ( inStateVec.at( vn ).at( vc ).outVc != UINT32_MAX ) ) {
        // Ready for switch allocation
        auto *flit = inStateVec.at( vn ).at( vc ).inBuf.front();
        //output->verbose( CALL_INFO, 5, 0, "Flit %s ready for switch alloc\n", flit->pktIdStr().c_str() );
        inStateVec.at(vn).at(vc).inVcState = IN_ACTIVE;
        rtrSharedObjs->needSwitchAlloc.at(vn).at(vc) = flit;
      }
    }
  }

  // Send a flit on the link from the output buffer
  // - this should probably send from whichever VC is already sending since the
  // switch allocation will hold it high
  // TODO: Determine how to add output_stalls to this logic
  // output_stall is if every non-empty buffer is prevented from sending
  // might want a couple of other booleans to help figure it out, but that's just random musing
  bool sent = false;
  for( uint32_t i = 0, vn = flit_vn_rr; i < numVns; i++, vn = ( ( vn != ( numVns - 1 ) ) ? vn + 1 : 0 ) ) {
    for( uint32_t j = 0, vc = flit_vc_rr; j < numVcs; j++, vc = ( ( vc != ( numVcs - 1 ) ) ? vc + 1 : 0 ) ) {
      if ( ( !outStateVec.at( vn ).at( vc ).outBuf.empty() ) &&
           ( outStateVec.at( vn ).at ( vc ).destCredits > 0 ) ) { // ensure there are dest credits
        auto flit = outStateVec.at( vn ).at( vc ).outBuf.front();
        outStateVec.at( vn ).at( vc ).outBuf.pop();
        outStateVec.at( vn ).at( vc ).outBufCredits++;
        link->send( flit );
        sent = true;
        outStateVec.at( vn ).at ( vc ).destCredits--;
        if ( flit->ftype == MordredFlit::TAIL ) {
          statLinkSentFlitCnt.at( vn ).at( vc )->addData( flit->flit_id + 1 );
          statLinkSentPacketCnt.at( vn ).at( vc )->addData( 1 );
        }
        //output->verbose( CALL_INFO, 5, 0, "Sending output flit %s; outBufCredits=%" PRId32 ", destCredits=%" PRId32 "\n",
        //  flit->pktIdStr().c_str(), outStateVec.at( vn ).at( vc ).outBufCredits, outStateVec.at(vn).at(vc).destCredits);
        //output->flush();
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
    validateVnVc( credit->vn, credit->vc );
    outStateVec.at( credit->vn ).at ( credit->vc ).destCredits += credit->credits;
    //output->verbose( CALL_INFO, 5, 0, "Received %" PRId32 " credits to vc=%" PRIu32 ", cur_credits=%" PRIu32 "\n",
    //  credit->credits, credit->vc, outStateVec.at( credit->vn ).at( credit->vc ).destCredits );
    //output->flush();
    delete bev;
    break;
  } // end CREDIT
  case baseMordredEvent::FLIT: {
    auto *flit = static_cast<MordredFlit*>( ev );
    if ( flit == nullptr )
      output->fatal( CALL_INFO, -1, "Invalid flit \n" );
    validateVnVc( flit->vn, flit->cur_vc );

    //output->verbose( CALL_INFO, 5, 0, "Recv flit %s, src=%" PRIu64 ", dst=%" PRIu64 ", vn=%" PRIu32 ", vc=%" PRIu32 ", type=%u\n",
    //  flit->pktIdStr().c_str(), flit->req->src, flit->req->dest, flit->vn, flit->cur_vc, (uint32_t)flit->ftype );
    //output->flush();

    inStateVec.at( flit->vn ).at( flit->cur_vc ).inBuf.push( flit );
    if ( flit->ftype == MordredFlit::TAIL )
      statLinkRecvFlitCnt.at( flit->vn ).at( flit->cur_vc )->addData( flit->flit_id + 1 );
    break;
  } // end FLIT
  default:
    output->fatal( CALL_INFO, -1, "Unknown/unimplemented event type=%d\n", (int) bev->getType() );
  }  // end switch
}

MordredFlit* RtrPortControl::getInBufFlit() {

  validateVnVc( switch_alloc_sendfrom_vn, switch_alloc_sendfrom_vc );

  if ( inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).inBuf.empty() ) {
    // This could happen if we've run out of credits and the sender couldn't deliver yet, so not a fatal error
    return nullptr;
  }
  MordredFlit* flit = inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).inBuf.front();
  inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).inBuf.pop();

  if ( flit == nullptr )
    output->fatal( CALL_INFO, -1, "Invalid flit \n" );

  // Can return a credit to the sender
  inStateVec.at( switch_alloc_sendfrom_vn ).at( switch_alloc_sendfrom_vc ).retCredits++;

  //output->verbose( CALL_INFO, 5, 0, "Get flit %s from inBuf\n", flit->pktIdStr().c_str() );
  //output->flush();
  return flit;
}

void RtrPortControl::recvOutBufFlit( MordredFlit* flit ) {
  validateVnVc( switch_alloc_rcvto_vn, switch_alloc_rcvto_vc );
  flit->cur_vc = switch_alloc_rcvto_vc;
  outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).outBuf.push( flit );
  outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).outBufCredits--;
  //output->verbose( CALL_INFO, 5, 0, "Put flit %s in outBuf; flit vn,vc=%" PRIu32 ", %" PRIu32 "; outBufCredits=%" PRId32 "\n",
  //  flit->pktIdStr().c_str(), flit->vn, flit->cur_vc, outStateVec.at( switch_alloc_rcvto_vn ).at( switch_alloc_rcvto_vc ).outBufCredits);
  //output->flush();
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
      if( inStateVec.at( vn ).at ( vc ).retCredits != 0 ) {
        auto credit = new MordredCreditEvent( vn, vc, inStateVec.at( vn ).at ( vc ).retCredits );
        //output->verbose( CALL_INFO, 5, 0, "Sending credit event with %d credits\n",
        //  inStateVec.at( vn ).at ( vc ).retCredits );
        //output->flush();
        link->send( credit );
        inStateVec.at( vn ).at ( vc ).retCredits = 0;
        break;  // only send one packet on the link
      }
    }
  }
  credit_ret_vc_rr = ( credit_ret_vc_rr + 1 ) % numVcs;
  credit_ret_vn_rr = ( credit_ret_vn_rr + 1 ) % numVns;
}
