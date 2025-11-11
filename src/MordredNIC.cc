//
// MordredNIC.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include <cmath>

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
  //output->setVerboseMask( DEBUG_INIT_PHASE );

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

  // Register stats
  statPacketsRecv = registerStatistic<uint64_t>( "packets_recv" );
  statAvgNocLatency = registerStatistic<double>( "average_noc_latency" );
  statAvgFlitsPerPacket = registerStatistic<double>( "average_packet_size" );

  output->verbose(CALL_INFO, 5, 0, "MordredNIC constructed\n");
  output->flush();
}

void MordredNIC::init( uint32_t phase ) {
  Event *ev;
  MordredInitEvent* init_ev;

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
    //output->verbose( CALL_INFO, 5, 0, "Received init phase=%" PRIu32 " packets from [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "]\n",
    //  phase, rtrId, rtrPort );
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

    //output->verbose( CALL_INFO, 5, 0, "Received init_phase=%" PRIu32 " packets with numVCs=%" PRIu32 ", flit_width=%" PRIu32 ", channel_bus_width=%" PRIu32 "\n",
    //  phase, numVcs, flitSize, channelBusWidth );
    //output->flush();
    resizeVectors();

    break;
  }

  case 3: {
    init_ev = getInitEvent( MordredInitEvent::Commands::ENDPOINT_ID );
    netID = init_ev->value;
    initialized = true;
    //output->verbose( CALL_INFO, 5, 0, "Received endpoint id = %" PRId64 "\n", netID );
    delete init_ev;
  } break;

  case 4: {
    // Send router credits equal to num_flits inBuf can hold
    auto credits = static_cast<int32_t>( inbufSize.getRoundedValue() / flitSize );
    if ( credits == 0 )
      output->fatal( CALL_INFO, -1, "Invalid configuration; flit_size=%" PRIu32 "b > input_buf_size=%" PRId64 "b (buf cannot hold a flit)\n",
        flitSize, inbufSize.getRoundedValue() );
    for ( uint32_t i = 0; i < numVns; i++ ) {
      auto* credit_ev = new MordredCreditEvent( i, 0, credits );
      link->sendUntimedData( credit_ev );
    }
    break;
  }

  default:
    //output->verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, "Init phase = %" PRIu32 "\n", phase );
    while ( ( ev = link->recvUntimedData() ) != nullptr ) {
      auto base_ev = static_cast<baseMordredEvent*>( ev );
      if( base_ev->getType() == baseMordredEvent::CREDIT ) {
        auto credit_ev = static_cast<MordredCreditEvent*>( ev );
        rtrCredits.at( credit_ev->vn ) += credit_ev->credits;
        //output->verbose( CALL_INFO, 5, 0, "Received credit event vn=%" PRIu32 ", credits=%" PRId32 "; cur_credits=%" PRId32 "\n",
        //  credit_ev->vn, credit_ev->credits, rtrCredits.at( credit_ev->vn ) );
        delete ev;
      } else if ( base_ev->getType() == baseMordredEvent::PACKET ) {
        //output->verbose( CALL_INFO, 5, 0, "Received untimed packet\n" );
        //output->flush();
        initEvents.push( static_cast<MordredInitEvent*>(ev) );
      } else {
        output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
        delete ev;
      }
    }
    break;
  }
  //output->verbose( CALL_INFO, 5, DEBUG_INIT_PHASE, " END init phase=%" PRIu32 "\n", phase );
  //output->flush();
}

void MordredNIC::setup() {
#if 0
  output->verbose(CALL_INFO, 5, 0, "MordredNIC SETUP nid=%" PRId64 ", rtrId=%" PRIu32 ", rtrPort=%" PRIu32 "\n", netID, rtrId, rtrPort);
  output->verbose( CALL_INFO, 5, 0, "MordredNIC SETUP numVCs=%" PRIu32 ", flitWidth=%" PRIu32 "\n", numVcs, flitSize );
  output->flush();
#endif
}

void MordredNIC::complete( uint32_t phase ) {
  //output->verbose(CALL_INFO, 7, 0, "MordredNIC complete; phase=%" PRIu32 "\n", phase);
  //output->flush();
  Event *ev;

  while ( ( ev = link->recvUntimedData() ) != nullptr ) {
    auto base_ev = static_cast<baseMordredEvent*>( ev );
    if( base_ev->getType() == baseMordredEvent::CREDIT ) {
      auto credit_ev = static_cast<MordredCreditEvent*>( ev );
      rtrCredits.at( credit_ev->vn ) += credit_ev->credits;
      //output->verbose( CALL_INFO, 5, 0, "Received credit event vn=%" PRIu32 ", credits=%" PRId32 "; cur_credits=%" PRId32 "\n",
      //  credit_ev->vn, credit_ev->credits, rtrCredits.at( credit_ev->vn ) );
      delete ev;
    } else if ( base_ev->getType() == baseMordredEvent::PACKET ) {
      //output->verbose( CALL_INFO, 5, 0, "Received untimed packet\n" );
      //output->flush();
      initEvents.push( static_cast<MordredInitEvent*>(ev) );
    } else {
      output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d\n", (int) base_ev->getType() );
      delete ev;
    }
  }
}

void MordredNIC::finish() {
  double avg_ticks = (double)totalNocLatency / totalPackets;
  if ( totalPackets == 0 ) // need this to avoid some nasty output in sst 14.0.0
    avg_ticks = -1.0;
  statPacketsRecv->addData( totalPackets );
  statAvgNocLatency->addData( avg_ticks );
  double avg_flits = (double)totalNumFlits / totalPackets;
  statAvgFlitsPerPacket->addData( avg_flits );
  //output->verbose(CALL_INFO, 7, 0, "MordredNIC finish\n");
  //output->flush();
}

void MordredNIC::sendUntimedData( Request* req ) {
  auto ev = new MordredInitEvent(req);
  //output->verbose( CALL_INFO, 5, 0, "MordredNIC sendUntimedData; src=%" PRIu64 ", dest=%" PRIu64 "\n",
  //  req->src, req->dest);
  //output->flush();
  link->sendUntimedData( ev );
}

SST::Interfaces::SimpleNetwork::Request* MordredNIC::recvUntimedData() {
  if ( initEvents.empty() )
    return nullptr;

  auto ev = initEvents.front();
  initEvents.pop();
  auto req = ev->req;
  delete ev;
  return req;
}

int32_t MordredNIC::calcNumFlits( uint32_t num_bits ) {
  // Need to see if we have enough credits to send this
  auto num_flits = static_cast<int32_t>(ceil( num_bits / flitSize ));
  //output->verbose( CALL_INFO, 5, 0, "Sending request of size=%" PRIu32 " bits; num_flits=%" PRId32 "\n", num_bits, num_flits );
  if ( num_flits < 2 ) // per current docs, at least 2 flits per packet
    num_flits = 2;
  return num_flits;
}

bool MordredNIC::send( Request* req, int32_t vn ) {
  if ( vn != 0 )
    output->fatal( CALL_INFO, -1, "MordredNIC only supports vn=0\n" );
  auto u_vn = static_cast<uint32_t>( vn );

  auto num_flits = calcNumFlits( req->size_in_bits );
  if ( outbufCredits.at(u_vn) < num_flits ) {
    // The comparison here needs to stay in sync with the comparison done in spaceToSend()
    return false;
  }
  // Update credits
  outbufCredits.at(u_vn) -= num_flits;

  /* One thing to note here, we send the SimpleNetwork Request with every flit (useful for debugging
   * purposes).  When the dest NIC receives a TAIL flit, then we pull the Request out */

  // Create flits
  // Consider making this a separate function if we find a need for it elsewhere
  auto u_num_flits = static_cast<uint32_t>( num_flits );
  // Head flit
  auto flit = new MordredFlit( req, MordredFlit::HEAD, packetId, 0 );
  // auto head_flit = flit;
  outBuf.at(u_vn).push( flit );

  // Body flits
  for ( uint32_t i = 1; i < u_num_flits-1; i++ ) {
    flit = new MordredFlit( req, MordredFlit::BODY, packetId, i );
    outBuf.at(u_vn).push( flit );
    flit = nullptr;
  }

  // Tail flit
  flit = new MordredFlit( req, MordredFlit::TAIL, packetId++, u_num_flits-1 );
  flit->pkt_created_cycle = getCurrentSimCycle();
  outBuf.at(u_vn).push( flit );

  //output->verbose( CALL_INFO, 7, 0, "EPNIC Send to [RTR.Port]=[%u,%u] with dest=%" PRIu64 "; head_flit=%s, num_flits=%u\n",
  //  rtrId, rtrPort, req->dest, head_flit->pktIdStr().c_str(), u_num_flits );
  //output->flush();

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

  // Move to handleIncomingPacket()?
  if ( req->dest != netID ) {
    output->flush();
    output->fatal( CALL_INFO, -1, "Packet with dest=%" PRId64 " received by netID=%" PRId64 ". Enough endpoints?\n",
      req->dest, netID );
  }
  return req;
}

bool MordredNIC::spaceToSend( int vn, int num_bits ) {
  if ( vn != 0 )
    output->fatal( CALL_INFO, -1, "MordredNIC only supports vn=0\n" );
  int32_t num_flits = calcNumFlits( static_cast<uint32_t>(num_bits) );
  auto u_vn = static_cast<uint32_t>( vn );
  if ( outbufCredits.at(u_vn) >= num_flits )
    return true;
  return false;
}

bool MordredNIC::requestToReceive( int vn ) {
  if ( vn != 0 )
    output->fatal( CALL_INFO, -1, "MordredNIC only supports vn=0\n" );
  auto u_vn = static_cast<uint32_t>( vn );
  if ( inBuf.at(u_vn).empty() )
    return false;
  return true;
}

bool MordredNIC::clockTick( Cycle_t cycle ) {

  bool sent = false;
  uint32_t vn;

  // Since we're only doing 1 VN for now, we could remove the for vn loops
  // No use of VCs here

  // Send a flit to the router (if credit available)
  for ( vn = 0; vn < numVns; vn++ ) {
    if ( !outBuf.at(vn).empty() ) {
      if ( rtrCredits.at(vn) > 0 ) {
        auto flit = outBuf.at(vn).front();
        outBuf.at(vn).pop();
        if ( flit->ftype == MordredFlit::HEAD ) {
          headInjectCycle = getCurrentSimCycle();
          //output->verbose( CALL_INFO, 7, 0, "Sent head flit %s to link at cycle=%" PRIu64 "; rtrCredits=%" PRId32 "\n",
          //  flit->pktIdStr().c_str(), cycle, rtrCredits.at(vn) );
        } else if ( flit->ftype == MordredFlit::TAIL ) {
          flit->head_inject_cycle = headInjectCycle;
          headInjectCycle = UINT64_MAX;
          //output->verbose( CALL_INFO, 5, 0, "Sent tail flit %s to link at cycle=%" PRIu64 "; rtrCredits=%" PRId32 "\n",
          //  flit->pktIdStr().c_str(), cycle, rtrCredits.at(vn) );
          if (sendFunctor != nullptr) {
            bool keep = (*sendFunctor)((int)vn);
            if ( !keep ) sendFunctor = nullptr;
          }
        }
        //output->flush();
        link->send( flit );
        sent = true;
        rtrCredits.at(vn)--;
        outbufCredits.at(vn)++;
        output->verbose( CALL_INFO, 7, 0, "Sent flit %s to link at cycle=%" PRIu64 "; rtrCredits=%" PRId32 "\n",
          flit->pktIdStr().c_str(), cycle, rtrCredits.at(vn) );
        output->flush();
      }
    }
  }

  if (sent)
    return false;

  // Didn't send a flit, try returning credits
  // Once we send a credit packet out, we're done for this cycle
  for ( vn = 0; vn < numVns; vn++ ) {
    if ( inReturnCredits.at(vn) > 0 ) {
      auto credit_ev = new MordredCreditEvent( vn, 0, inReturnCredits.at(vn) );
      link->send( credit_ev );
      //output->verbose( CALL_INFO, 5, 0, "Returning %" PRId32 " credits to router vn=%" PRIu32 "\n",
      //  inReturnCredits.at(vn), vn );
      inReturnCredits.at(vn) = 0;
      break;
    }
  }
  return false;
}

void MordredNIC::resizeVectors() {
  if ( numVns == 0 ) {
    output->flush();
    output->fatal( CALL_INFO, -1, "MordredNIC resizing vectors failure\n" );
  }

  inBuf.resize( numVns );
  outBuf.resize( numVns );

  auto credits = outbufSize.getRoundedValue() / flitSize;
  rtrCredits.resize( numVns, 0 );
  outbufCredits.resize( numVns, static_cast<int32_t>( credits ) );
  inReturnCredits.resize( numVns, 0 );
}

MordredInitEvent* MordredNIC::getInitEvent( MordredInitEvent::Commands cmd ) {
  Event *ev = link->recvUntimedData();
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
    if ( credit->vn != 0 )
      output->fatal( CALL_INFO, -1, "Unsupported vn=%u\n", credit->vn );
    rtrCredits.at(credit->vn) += credit->credits;
    output->verbose( CALL_INFO, 7, 0, "Received %" PRId32 " credits to vn=%" PRIu32 ", cur_credits=%" PRIu32 "\n",
      credit->credits, credit->vn, rtrCredits.at( credit->vn ) );
    delete bev;
    break;
  } // end CREDIT
  case baseMordredEvent::FLIT: {
    auto flit = static_cast<MordredFlit*>( ev );
    output->verbose( CALL_INFO, 7, 0, "Received flit vn,vc=%" PRIu32 ", %" PRIu32 ", type=%s\n",
      flit->vn, flit->cur_vc, flit->getFtypeStr().c_str() );
    if ( flit->ftype == MordredFlit::TAIL) {
      Request* req = flit->getRequest();
      if ( req == nullptr ) {
        output->fatal( CALL_INFO, -1, "Request was nullptr!\n" );
      }
      if ( flit->vn != 0 )
        output->fatal( CALL_INFO, -1, "Unsupported vn=%u\n", flit->vn );
      inBuf.at(flit->vn).push( req );
      // Compute elapsed latency of the packet
      // TODO: Do this with the actual clock rate, etc...seems like some things may change in sst 16, so I'm not in a rush
      // to deal with it today
      uint64_t noc_latency = getCurrentSimCycle() - flit->head_inject_cycle;
      //uint64_t total_latency = getCurrentSimCycle() - flit->pkt_created_cycle;
      double noc_latency_ns = ceil( noc_latency / 1000 );
      // Time in ns == clock ticks with 1 GHz clock.
      //output->verbose( CALL_INFO, 7, 0, "Finished receiving %s; total latency=%" PRIu64 "; NoC latency=%" PRIu64 "= %f ns\n",
      //  flit->pktIdStr().c_str(), total_latency, noc_latency, noc_latency_ns );
      totalNocLatency += (uint64_t)noc_latency_ns;
      totalPackets++;
      totalNumFlits += (flit->flit_id+1);
      if ( receiveFunctor != NULL ) {
        bool keep = (*receiveFunctor)((int)flit->vn);
        if ( !keep) receiveFunctor = NULL;
      }
    }
    //output->flush();
    // Update num of credits to return to router
    inReturnCredits.at( flit->vn )++;
    delete flit;
    break;
  } // end FLIT
  default:
    output->fatal( CALL_INFO, -1, "Unknown/unimplemented event type=%d\n", (int) bev->getType() );
  }  // end switch

}





