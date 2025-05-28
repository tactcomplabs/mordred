//
// MordredNIC.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "MordredNIC.h"

#include "MordredEvents.h"
#include "sst_config.h"

using namespace SST::Mordred;

MordredNIC::MordredNIC( ComponentId_t cid, Params& params, int vns = 1 ) :
   SimpleNetwork(cid),
   netID(-1),
   bw("1GB/s")
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new SST::Output("MordredNIC[" + getName() + ":@p:@t]: ", verbosity, 0, Output::STDOUT);

  // Validate vns
  if ( vns != 1 ) {
    output->fatal( CALL_INFO, -1, "Invalid number of vns=%" PRId32 "; must be == 1\n", vns );
  }
  numVns = static_cast<uint32_t>(vns);

  // Set up buffers (partially borrowed from Kingsley)
  inbufSize = params.find<UnitAlgebra>("input_buf_size", "1kiB");
  if ( !inbufSize.hasUnits("b") && !inbufSize.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"input_buf_size must be specified in either "
                       "bits or bytes: %s\n",inbufSize.toStringBestSI().c_str());
  }
  if ( inbufSize.hasUnits("B") )
    inbufSize *= UnitAlgebra("8b/B");

  outbufSize = params.find<UnitAlgebra>("output_buf_size", "1kiB");
  if ( !outbufSize.hasUnits("b") && !outbufSize.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"output_buf_size must be specified in either "
                       "bits or bytes: %s\n",outbufSize.toStringBestSI().c_str());
  }
  if ( outbufSize.hasUnits("B") )
    outbufSize *= UnitAlgebra("8b/B");

  // Configure the links
  // For now give it a fake timebase.  Will give it the real timebase during init
  std::string port_name("port");
  //if ( isAnonymous())
  //  port_name = params.find<std::string>("port_name");
  link = configureLink(port_name, std::string("1GHz"),
      new Event::Handler<MordredNIC>(this,&MordredNIC::handleIncomingPacket));

  // Configure clock handler
  std::string clock_freq("1GHz");
  registerClock( clock_freq, new Clock::Handler2<MordredNIC, &MordredNIC::clockTick>(this) );

  output->verbose(CALL_INFO, 5, 0, "MordredNIC constructed\n");
  output->flush();
}

void MordredNIC::init( uint32_t phase ) {
  Event *ev;
  MordredInitEvent* init_ev;

  // Note: could rewrite this function to use sendUntimedData instead of just putting messages
  // onto the link.

  switch ( phase ) {
  case 0:
    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::REPORT_ENDPOINT;
    init_ev->value = UINT32_MAX;
    link->sendUntimedData( init_ev );
    break;

  case 1:
    init_ev = getInitEvent( MordredInitEvent::Commands::REPORT_ROUTER );
    // Nothing of interest expected from the endpoint when receiving this packet;
    // routers are broadcasting this in phase 0
    // much like the endpoints are doing REPORT_ENDPOINT sends in phase 0.
    delete init_ev;

    init_ev = getInitEvent( MordredInitEvent::Commands::ROUTER_ID );
    rtrId = init_ev->value;
    delete init_ev;

    init_ev = getInitEvent( MordredInitEvent::Commands::PORT_NUM );
    rtrPort = init_ev->value;
    delete init_ev;
    output->verbose( CALL_INFO, 5, 0, "Received init phase=%" PRIu32 " packets from [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "]\n",
      phase, rtrId, rtrPort );
    break;

  case 2: {
    init_ev = getInitEvent( MordredInitEvent::Commands::NUM_VNS );
    if ( numVns != init_ev->value )
      output->fatal( CALL_INFO, -1, "Number of VNs in init packet (%" PRIu32 ") != number of VNs in config (%" PRIu32 ")\n",
        init_ev->value, numVns );

    init_ev = getInitEvent( MordredInitEvent::Commands::NUM_VCS );
    numVcs = init_ev->value;
    delete init_ev;

    init_ev = getInitEvent( MordredInitEvent::Commands::FLIT_WIDTH );
    flitSize = init_ev->value;
    delete init_ev;

    init_ev = getInitEvent( MordredInitEvent::Commands::BUS_WIDTH );
    channelBusWidth = init_ev->value;
    delete init_ev;
    output->verbose( CALL_INFO, 5, 0, "Received init phase=%" PRIu32 " packets with numVCs=%" PRIu32 ", flit_width=%" PRIu32 ", channel_bus_width=%" PRIu32 "\n",
      phase, numVcs, flitSize, channelBusWidth );
    break;

#if 0
    // from Kingsley
    init_ev = static_cast<MordredInitEvent*>(ev);
    UnitAlgebra flit_size_ua = init_ev->ua_value;
    flit_size = flit_size_ua.getRoundedValue();

    UnitAlgebra link_clock = link_bw / flit_size_ua;

    TimeConverter* tc = getTimeConverter(link_clock);
    output->timing->setDefaultTimeBase(tc);

    for ( int i = 0; i < req_vns; ++i ) {
      outbuf_credits[i] = outbuf_size.getRoundedValue() / flit_size;
      in_ret_credits[i] = inbuf_size.getRoundedValue() /flit_size;
    }

#endif
  }

  case 3: {
    init_ev = getInitEvent( MordredInitEvent::Commands::ENDPOINT_ID );
    netID = init_ev->value;
    initialized = true;
    output->verbose( CALL_INFO, 5, 0, "Received endpoint id = %" PRId64 "\n", netID );
    delete init_ev;

    // Setup/send credit info
    resizeVectors();
    // Send router credits equal to num_flits in_buf can hold
    auto credits = static_cast<int32_t>( inbufSize.getRoundedValue() / flitSize );
    for ( uint32_t i = 0; i < numVns; i++ ) {
      auto* credit_ev = new MordredCreditEvent( i, 0, credits );
      link->sendUntimedData( credit_ev );
    }
    break;
  }

  default:
    output->verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, "Init phase = %" PRIu32 "\n", phase );
    while ( ( ev = link->recvUntimedData() ) != nullptr ) {
      auto base_ev = static_cast<baseMordredEvent*>( ev );
      if( base_ev->getType() == baseMordredEvent::CREDIT ) {
        auto credit_ev = static_cast<MordredCreditEvent*>( ev );
        rtrCredits.at( credit_ev->vn ) += credit_ev->credits;
        output->verbose( CALL_INFO, 5, 0, "Received credit event vn=%" PRIu32 ", credits=%" PRId32 "; cur_credits=%" PRId32 "\n",
          credit_ev->vn, credit_ev->credits, rtrCredits.at( credit_ev->vn ) );
      } else {
        output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
      }
      delete ev;
    }
    break;
  }
}

void MordredNIC::setup() {
#if 0
  output->verbose(CALL_INFO, 5, 0, "MordredNIC SETUP nid=%" PRId64 ", rtrId=%" PRIu32 ", rtrPort=%" PRIu32 "\n", netID, rtrId, rtrPort);
  output->verbose( CALL_INFO, 5, 0, "MordredNIC SETUP numVCs=%" PRIu32 ", flitWidth=%" PRIu32 ", channelBusWidth=%" PRIu32 "\n", numVcs, flitSize, channelBusWidth );
  output->flush();
#endif
}

void MordredNIC::complete( uint32_t phase ) {
  output->verbose(CALL_INFO, 7, 0, "MordredNIC complete; phase=%" PRIu32 "\n", phase);
  output->flush();
}

void MordredNIC::finish() {
  output->verbose(CALL_INFO, 7, 0, "MordredNIC finish\n");
  output->flush();
}

void MordredNIC::sendUntimedData( Request* req ) {
  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
}

SST::Interfaces::SimpleNetwork::Request* MordredNIC::recvUntimedData() {
  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return nullptr;
}

bool MordredNIC::send( Request* req, int32_t vn ) {
  if ( vn != 0 )
    output->fatal( CALL_INFO, -1, "MordredNIC only supports vn=0\n" );
  auto u_vn = static_cast<uint32_t>( vn );
  if ( outbufCredits.at(u_vn) <= 0 )
    return false;

  outBuf.at(u_vn).push( req );
  outbufCredits.at(u_vn)--;
  return true;
}

// Have to keep the vn argument to match SimpleNetwork interface
SST::Interfaces::SimpleNetwork::Request* MordredNIC::recv( int32_t vn ) {
  if ( vn != 0 )
    output->fatal( CALL_INFO, -1, "MordredNIC only supports vn=0\n" );
  auto u_vn = static_cast<uint32_t>( vn );
  if ( inBuf.at(u_vn).empty() )
    return nullptr;

  Request* req = inBuf.at(u_vn).front();
  inBuf.at(u_vn).pop();

  // Return credit to router
  inReturnCredits.at( u_vn )++;
  return req;
}

bool MordredNIC::spaceToSend( int vn, int num_bits ) {
  if ( vn != 0 )
    output->fatal( CALL_INFO, -1, "MordredNIC only supports vn=0\n" );
  auto u_vn = static_cast<uint32_t>( vn );
  //TODO: This is assuming one flit per inquiry into this function
  if ( outbufCredits.at(u_vn) > 0 )
    return true;
  return false;
}

bool MordredNIC::requestToReceive( int vn ) {
  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return false;
}

bool MordredNIC::clockTick( Cycle_t cycle ) {

  bool sent = false;
  uint32_t vn = 0; // TODO: Fix if multiple VNs

  if ( !outBuf.at(vn).empty() ) {
    if ( rtrCredits.at(vn) > 0 ) {
      auto req = outBuf.at(vn).front();
      auto flit = new MordredFlit( req );
      link->send( flit );
      sent = true;
      outBuf.at(vn).pop();
      rtrCredits.at(vn)--;
      outbufCredits.at(vn)++;
      output->verbose( CALL_INFO, 5, 0, "Sent flit to link; credits_left=%" PRId32 "\n", rtrCredits.at(0) );
    }
  }

  // This is doing all VNs
  if ( !sent ) {
    for ( uint32_t i = 0; i < numVns; i++ ) {
      if ( inReturnCredits.at(i) > 0 ) {
        auto credit_ev = new MordredCreditEvent( i, 0, inReturnCredits.at(i) );
        link->send( credit_ev );
        output->verbose( CALL_INFO, 5, 0, "Returning %" PRId32 " credits to router vn=%" PRIu32 "\n",
          inReturnCredits.at(i), i );
        inReturnCredits.at(i) = 0;
        break;
      }
    }
  }

  return false;
}

void MordredNIC::resizeVectors() {
  inBuf.resize( numVns );
  outBuf.resize( numVns );

  auto credits = outbufSize.getRoundedValue() / flitSize;
  rtrCredits.resize( numVns, 0 );
  outbufCredits.resize( numVns, static_cast<int32_t>( credits ) );
  inReturnCredits.resize( numVns, 0 );
}

MordredInitEvent* MordredNIC::getInitEvent( MordredInitEvent::Commands cmd ) {
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

void MordredNIC::handleIncomingPacket( SST::Event* ev ) {
  // if it's a flit, add it to a buffer for the surrounding unit to reassemble, etc
  auto bev = static_cast<baseMordredEvent*>( ev );
  switch( bev->getType() ) {
  case baseMordredEvent::CREDIT: {
    auto credit = static_cast<MordredCreditEvent*>( bev );
    rtrCredits.at(credit->vn) += credit->credits;
    output->verbose( CALL_INFO, 5, 0, "Received %" PRId32 " credits to vn=%" PRIu32 ", cur_credits=%" PRIu32 "\n",
      credit->credits, credit->vn, rtrCredits.at( credit->vn ) );
    delete bev;
    break;
  } // end CREDIT
  case baseMordredEvent::FLIT: {
    auto flit = static_cast<MordredFlit*>( ev );
    Request* req = flit->getRequest();
    if ( req == nullptr ) {
      output->fatal( CALL_INFO, -1, "Request was nullptr!\n" );
    }
    inBuf.at(flit->vn).push( req );
    delete flit;
    break;
  } // end FLIT
  default:
    output->fatal( CALL_INFO, -1, "Unknown/unimplemented event type=%d\n", (int) bev->getType() );
  }  // end switch

}





