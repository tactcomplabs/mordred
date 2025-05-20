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

#if 0
  // Validate vns
  if ( num_vns <= 0 ) {
    output->fatal( CALL_INFO, -1, "Invalid number of vns=%" PRId32 "; must be >= 1\n", num_vns );
  }
  size_t vns = static_cast<size_t>(num_vns);
#endif
  //size_t num_vns = (size_t)vns;

  // Set up buffers (partially borrowed from Kingsley)
  inbuf_size = params.find<UnitAlgebra>("input_buf_size", "1kiB");
  if ( !inbuf_size.hasUnits("b") && !inbuf_size.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"input_buf_size must be specified in either "
                       "bits or bytes: %s\n",inbuf_size.toStringBestSI().c_str());
  }
  if ( inbuf_size.hasUnits("B") )
    inbuf_size *= UnitAlgebra("8b/B");

  outbuf_size = params.find<UnitAlgebra>("output_buf_size", "1kiB");
  if ( !outbuf_size.hasUnits("b") && !outbuf_size.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"output_buf_size must be specified in either "
                       "bits or bytes: %s\n",outbuf_size.toStringBestSI().c_str());
  }
  if ( outbuf_size.hasUnits("B") )
    outbuf_size *= UnitAlgebra("8b/B");

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
    auto credits = static_cast<int32_t>( inbuf_size.getRoundedValue() / flitSize );
    for( uint32_t i = 0; i < numVcs; i++ ) {
      auto* credit_ev = new MordredCreditEvent( i, credits );
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
        rtr_credits.at( credit_ev->vc ) += credit_ev->credits;
        output->verbose( CALL_INFO, 5, 0, "Received credit event vc=%d, credits=%d; cur_credits=%d\n",
          credit_ev->vc, credit_ev->credits, rtr_credits.at( credit_ev->vc ) );
      } else {
        output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
      }
      delete ev;
    }
    break;
  }
}

void MordredNIC::setup() {
  //output->verbose(CALL_INFO, 5, 0, "MordredNIC SETUP nid=%" PRId64 ", rtrId=%" PRIu32 ", rtrPort=%" PRIu32 "\n", netID, rtrId, rtrPort);
  //output->verbose( CALL_INFO, 5, 0, "MordredNIC SETUP numVCs=%" PRIu32 ", flitWidth=%" PRIu32 ", channelBusWidth=%" PRIu32 "\n", numVcs, flitSize, channelBusWidth );
  //output->flush();
}

void MordredNIC::complete( uint32_t phase ) {
  output->verbose(CALL_INFO, 5, 0, "MordredNIC complete; phase=%" PRIu32 "\n", phase);
  output->flush();
}

void MordredNIC::finish() {
  output->verbose(CALL_INFO, 5, 0, "MordredNIC finish\n");
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
  // This is a gross oversimplification since it's not checking credits available, etc.
  if ( vn != 0 )
    output->fatal( CALL_INFO, -1, "MordredNIC only supports vn=0\n" );
  out_buf.at(0).push( req );
  return true;
}

SST::Interfaces::SimpleNetwork::Request* MordredNIC::recv( int32_t vn ) {
  if ( in_buf.at(0).empty() )
    return nullptr;

  Request* req = in_buf.at(0).front();
  in_buf.at(0).pop();

  return req;
}

bool MordredNIC::spaceToSend( int vn, int num_bits ) {
  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return false;
}

bool MordredNIC::requestToReceive( int vn ) {
  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return false;
}

bool MordredNIC::clockTick( Cycle_t cycle ) {

  if ( !out_buf.at(0).empty() ) {
    auto req = out_buf.at(0).front();
    auto flit = new MordredFlit( req );
    link->send( flit );
    out_buf.at(0).pop();
    output->verbose( CALL_INFO, 5, 0, "Sent flit to link\n" );
  }
  return false;
}

void MordredNIC::resizeVectors() {
  in_buf.resize( numVcs );
  out_buf.resize( numVcs );

  auto credits = outbuf_size.getRoundedValue() / flitSize;
  rtr_credits.resize( numVcs, 0 );
  outbuf_credits.resize( numVcs, static_cast<int32_t>( credits ) );
  in_ret_credits.resize( numVcs, 0 );
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
  // TODO: If it's a credit, add to the credit
  // if it's a flit, add it to a buffer for the surrounding unit to reassemble, etc
  auto bev = static_cast<baseMordredEvent*>( ev );
  switch( bev->getType() ) {
  case baseMordredEvent::CREDIT:
    output->fatal( CALL_INFO, -1, "Credit handling not yet implemented\n" );
    break;
  case baseMordredEvent::FLIT: {
    auto flit = static_cast<MordredFlit*>( ev );
    Request* req = flit->getRequest();
    if ( req == nullptr ) {
      output->fatal( CALL_INFO, -1, "Request was nullptr!\n" );
    }
    in_buf.at(0).push( req );
    delete flit;
    break;
    }
  default:
    output->fatal( CALL_INFO, -1, "Unknown/unimplemented event type=%d\n", (int) bev->getType() );
  }  // end switch

}





