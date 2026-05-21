//
// MordredNicPC.cc
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include <cmath>

#include "sst_config.h"

#include "MordredEvents.h"
#include "MordredNicPC.h"

using namespace SST;
using namespace SST::Mordred;
using namespace SST::Interfaces;

// ---- Constructor ----

MordredNicPC::MordredNicPC( ComponentId_t cid, Params& params, int vns ) : SimpleNetwork( cid ), netID( -1 ) {
  const auto verbosity = params.find<uint32_t>( "verbose", 5 );
  output               = new Output( "MordredNicPC[" + getName() + ":@p:@t]: ", verbosity, 0, Output::STDOUT );

  // Buffer sizes
  inbufSize            = params.find<UnitAlgebra>( "input_buf_size", "1kiB" );
  if( !inbufSize.hasUnits( "b" ) && !inbufSize.hasUnits( "B" ) )
    output->fatal( CALL_INFO, -1, "input_buf_size must be specified in bits or bytes: %s\n", inbufSize.toStringBestSI().c_str() );
  if( inbufSize.hasUnits( "B" ) )
    inbufSize *= UnitAlgebra( "8b/B" );

  outbufSize = params.find<UnitAlgebra>( "output_buf_size", "1kiB" );
  if( !outbufSize.hasUnits( "b" ) && !outbufSize.hasUnits( "B" ) )
    output->fatal( CALL_INFO, -1, "output_buf_size must be specified in bits or bytes: %s\n", outbufSize.toStringBestSI().c_str() );
  if( outbufSize.hasUnits( "B" ) )
    outbufSize *= UnitAlgebra( "8b/B" );

  // Load the inner PhysChannelAPI subcomponent.
  physChannel = loadUserSubComponent<Prydwen::PhysChannelAPI>( "port_iface", ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, vns );
  if( !physChannel )
    output->fatal( CALL_INFO, -1, "MordredNicPC: no PhysChannelAPI subcomponent found in slot 'port_iface'\n" );

  physChannel->setNotifyOnReceive( new Prydwen::PhysChannelAPI::Handler2<MordredNicPC, &MordredNicPC::onReceive>( this ) );

  // Clock
  const auto clock_freq = params.find<std::string>( "clock", "1GHz" );
  registerClock( clock_freq, new Clock::Handler2<MordredNicPC, &MordredNicPC::clockTick>( this ) );

  // Initial bandwidth estimate (updated in init phase 2 when flitSize is known)
  UnitAlgebra ua_cf( clock_freq );
  bw                    = ua_cf * UnitAlgebra( "1b" );

  // Statistics
  statPacketsRecv       = registerStatistic<uint64_t>( "packets_recv" );
  statAvgNocLatency     = registerStatistic<double>( "average_noc_latency" );
  statAvgFlitsPerPacket = registerStatistic<double>( "average_packet_size" );

  output->verbose( CALL_INFO, 5, 0, "MordredNicPC constructed\n" );
}

// ---- Lifecycle ----

void MordredNicPC::init( uint32_t phase ) {
  physChannel->init( phase );

  output->verbose( CALL_INFO, 5, 0, "START init phase=%" PRIu32 "\n", phase );

  switch( phase ) {
  case 0:
    physChannel->sendUntimedData( [&]() -> MordredInitEvent* {
      auto* ev    = new MordredInitEvent();
      ev->command = MordredInitEvent::REPORT_ENDPOINT;
      ev->value   = UINT32_MAX;
      return ev;
    }() );
    break;

  case 1: {
    auto* init_ev = static_cast<MordredInitEvent*>( physChannel->recvUntimedData() );
    if( !init_ev )
      output->fatal( CALL_INFO, -1, "MordredNicPC: unable to recv REPORT_ROUTER in phase 1\n" );
    if( init_ev->command != MordredInitEvent::REPORT_ROUTER )
      output->fatal( CALL_INFO, -1, "MordredNicPC: expected REPORT_ROUTER, got command=%d\n", (int) init_ev->command );
    delete init_ev;

    init_ev = getInitEvent( MordredInitEvent::ROUTER_ID );
    rtrId   = init_ev->value;
    delete init_ev;

    init_ev = getInitEvent( MordredInitEvent::PORT_NUM );
    rtrPort = init_ev->value;
    output->verbose( CALL_INFO, 5, 0, "Phase 1: connected to [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "]\n", rtrId, rtrPort );
    delete init_ev;
    break;
  }

  case 2: {
    auto* init_ev = getInitEvent( MordredInitEvent::NUM_VNS );
    numVns        = init_ev->value;
    delete init_ev;

    init_ev = getInitEvent( MordredInitEvent::NUM_VCS );
    numVcs  = init_ev->value;
    delete init_ev;

    init_ev  = getInitEvent( MordredInitEvent::FLIT_WIDTH );
    flitSize = init_ev->value;  // bits
    bw *= UnitAlgebra( std::to_string( flitSize ) );
    delete init_ev;

    output->verbose(
      CALL_INFO, 5, 0, "Phase 2: numVNs=%" PRIu32 ", numVCs=%" PRIu32 ", flit_width=%" PRIu32 "\n", numVns, numVcs, flitSize
    );
    resizeVectors();
    break;
  }

  case 3: {
    auto* init_ev = getInitEvent( MordredInitEvent::ENDPOINT_ID );
    netID         = static_cast<nid_t>( init_ev->value );
    initialized   = true;
    output->verbose( CALL_INFO, 5, 0, "Phase 3: endpoint id = %" PRId64 "\n", netID );
    delete init_ev;

    // Send input-buffer credits to router
    const auto credits = static_cast<int32_t>( inbufSize.getRoundedValue() / flitSize );
    if( credits == 0 )
      output->fatal(
        CALL_INFO,
        -1,
        "Invalid configuration; flit_size=%" PRIu32 "b > input_buf_size=%" PRId64 "b\n",
        flitSize,
        inbufSize.getRoundedValue()
      );
    for( uint32_t i = 0; i < numVns; i++ ) {
      physChannel->sendUntimedData( new MordredCreditEvent( i, 0, credits ) );
    }
    break;
  }

  default: {
    // Drain any remaining untimed events (credits + routed packets)
    SST::Event* ev = nullptr;
    while( ( ev = physChannel->recvUntimedData() ) != nullptr ) {
      auto* base_ev = static_cast<baseMordredEvent*>( ev );
      if( base_ev->getType() == baseMordredEvent::CREDIT ) {
        auto* credit_ev = static_cast<MordredCreditEvent*>( ev );
        rtrCredits.at( credit_ev->vn ) += credit_ev->credits;
        output->verbose(
          CALL_INFO, 5, 0, "Received credit vn=%" PRIu32 ", credits=%" PRId32 "\n", credit_ev->vn, credit_ev->credits
        );
        delete ev;
      } else if( base_ev->getType() == baseMordredEvent::PACKET ) {
        initEvents.push( static_cast<MordredInitEvent*>( ev ) );
      } else {
        output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d in init default\n", (int) base_ev->getType() );
        delete ev;
      }
    }
    break;
  }
  }
  output->verbose( CALL_INFO, 5, 0, "END init phase=%" PRIu32 "\n", phase );
}

void MordredNicPC::setup() {
  physChannel->setup();
}

void MordredNicPC::complete( uint32_t phase ) {
  physChannel->complete( phase );

  SST::Event* ev = nullptr;
  while( ( ev = physChannel->recvUntimedData() ) != nullptr ) {
    auto* base_ev = static_cast<baseMordredEvent*>( ev );
    if( base_ev->getType() == baseMordredEvent::CREDIT ) {
      auto* credit_ev = static_cast<MordredCreditEvent*>( ev );
      rtrCredits.at( credit_ev->vn ) += credit_ev->credits;
      delete ev;
    } else if( base_ev->getType() == baseMordredEvent::PACKET ) {
      initEvents.push( static_cast<MordredInitEvent*>( ev ) );
    } else {
      output->verbose( CALL_INFO, 5, 0, "Received unexpected event type=%d in complete\n", (int) base_ev->getType() );
      delete ev;
    }
  }
}

void MordredNicPC::finish() {
  const double avg_ticks = totalPackets > 0 ? (double) totalNocLatency / (double) totalPackets : -1.0;
  statPacketsRecv->addData( totalPackets );
  statAvgNocLatency->addData( avg_ticks );
  const double avg_flits = totalPackets > 0 ? (double) totalNumFlits / (double) totalPackets : 0.0;
  statAvgFlitsPerPacket->addData( avg_flits );
}

// ---- SimpleNetwork outer interface ----

void MordredNicPC::sendUntimedData( Request* req ) {
  physChannel->sendUntimedData( new MordredInitEvent( req ) );
}

SimpleNetwork::Request* MordredNicPC::recvUntimedData() {
  if( initEvents.empty() )
    return nullptr;
  auto* ev = initEvents.front();
  initEvents.pop();
  auto* req = ev->req;
  delete ev;
  return req;
}

bool MordredNicPC::send( Request* req, int vn ) {
  const auto u_vn = static_cast<uint32_t>( vn );
  if( numVns <= u_vn )
    output->fatal( CALL_INFO, -1, "send: requested vn=%d is invalid (numVns=%" PRIu32 ")\n", vn, numVns );

  const int32_t num_flits = calcNumFlits( req->size_in_bits );
  if( outbufCredits.at( u_vn ) < num_flits )
    return false;
  outbufCredits.at( u_vn ) -= num_flits;

  const auto u_num_flits = static_cast<uint32_t>( num_flits );

  // Head flit
  outBuf.at( u_vn ).push( new MordredFlit( req, MordredFlit::HEAD, packetId, 0 ) );
  // Body flits
  for( uint32_t i = 1; i < u_num_flits - 1; i++ )
    outBuf.at( u_vn ).push( new MordredFlit( req, MordredFlit::BODY, packetId, i ) );
  // Tail flit
  auto* tail              = new MordredFlit( req, MordredFlit::TAIL, packetId++, u_num_flits - 1 );
  tail->pkt_created_cycle = getCurrentSimCycle();
  outBuf.at( u_vn ).push( tail );

  if( req->getTraceType() != Request::NONE ) {
    output->output(
      "TRACE(%d): %" PRIu64 " ns send called on %s\n", req->getTraceID(), getCurrentSimTimeNano(), getName().c_str()
    );
  }
  return true;
}

SimpleNetwork::Request* MordredNicPC::recv( int vn ) {
  const auto u_vn = static_cast<uint32_t>( vn );
  if( numVns <= u_vn )
    output->fatal( CALL_INFO, -1, "recv: requested vn=%d is invalid\n", vn );
  if( inBuf.at( u_vn ).empty() )
    return nullptr;
  auto* req = inBuf.at( u_vn ).front();
  inBuf.at( u_vn ).pop();
  if( req->getTraceType() != Request::NONE ) {
    output->output(
      "TRACE(%d): %" PRIu64 " ns recv called on %s\n", req->getTraceID(), getCurrentSimTimeNano(), getName().c_str()
    );
  }
  if( req->dest != netID )
    output->fatal( CALL_INFO, -1, "Packet with dest=%" PRId64 " received by netID=%" PRId64 "\n", req->dest, netID );
  return req;
}

bool MordredNicPC::spaceToSend( int vn, int num_bits ) {
  const auto u_vn = static_cast<uint32_t>( vn );
  if( numVns <= u_vn )
    output->fatal( CALL_INFO, -1, "spaceToSend: requested vn=%d is invalid\n", vn );
  return outbufCredits.at( u_vn ) >= calcNumFlits( static_cast<uint32_t>( num_bits ) );
}

bool MordredNicPC::requestToReceive( int vn ) {
  const auto u_vn = static_cast<uint32_t>( vn );
  if( numVns <= u_vn )
    output->fatal( CALL_INFO, -1, "requestToReceive: requested vn=%d is invalid\n", vn );
  return !inBuf.at( u_vn ).empty();
}

// ---- Clock ----

bool MordredNicPC::clockTick( Cycle_t cycle ) {
  bool sent = false;

  // Send one flit per tick (with router credits)
  for( uint32_t vn = 0; vn < numVns; vn++ ) {
    if( !outBuf.at( vn ).empty() && rtrCredits.at( vn ) > 0 ) {
      auto* flit = outBuf.at( vn ).front();
      outBuf.at( vn ).pop();

      if( flit->ftype == MordredFlit::HEAD ) {
        headInjectCycle = getCurrentSimCycle();
        if( flit->req->getTraceType() == Request::FULL ) {
          output->output(
            "TRACE(%d): %" PRIu64 " ns put head flit on link %s\n",
            flit->req->getTraceID(),
            getCurrentSimTimeNano(),
            getName().c_str()
          );
        }
      } else if( flit->ftype == MordredFlit::TAIL ) {
        flit->head_inject_cycle = headInjectCycle;
        headInjectCycle         = UINT64_MAX;
        if( flit->req->getTraceType() == Request::FULL ) {
          output->output(
            "TRACE(%d): %" PRIu64 " ns put tail flit on link %s\n",
            flit->req->getTraceID(),
            getCurrentSimTimeNano(),
            getName().c_str()
          );
        }
        if( sendFunctor ) {
          const bool keep = ( *sendFunctor )( static_cast<int>( vn ) );
          if( !keep )
            sendFunctor = nullptr;
        }
      }

      physChannel->send( flit, static_cast<int>( vn ) );
      sent = true;
      rtrCredits.at( vn )--;
      outbufCredits.at( vn )++;

      output->verbose(
        CALL_INFO,
        7,
        0,
        "Sent flit %s at cycle=%" PRIu64 "; rtrCredits=%" PRId32 "\n",
        flit->pktIdStr().c_str(),
        cycle,
        rtrCredits.at( vn )
      );
    }
  }

  if( sent )
    return false;

  // Return one credit batch per tick
  for( uint32_t vn = 0; vn < numVns; vn++ ) {
    if( inReturnCredits.at( vn ) > 0 ) {
      physChannel->send( new MordredCreditEvent( vn, 0, inReturnCredits.at( vn ) ), static_cast<int>( vn ) );
      inReturnCredits.at( vn ) = 0;
      break;
    }
  }
  return false;
}

// ---- Inner channel receive callback ----

bool MordredNicPC::onReceive( int sn_vn ) {
  SST::Event* ev = nullptr;
  while( ( ev = physChannel->recv( sn_vn ) ) != nullptr ) {
    processIncoming( ev );
  }
  return true;
}

void MordredNicPC::processIncoming( SST::Event* ev ) {
  auto* bev = static_cast<baseMordredEvent*>( ev );
  switch( bev->getType() ) {

  case baseMordredEvent::CREDIT: {
    auto* credit = static_cast<MordredCreditEvent*>( bev );
    if( credit->vn >= numVns )
      output->fatal( CALL_INFO, -1, "Unsupported vn=%u\n", credit->vn );
    rtrCredits.at( credit->vn ) += credit->credits;
    output->verbose(
      CALL_INFO,
      7,
      0,
      "Received %" PRId32 " credits for vn=%" PRIu32 ", cur=%" PRId32 "\n",
      credit->credits,
      credit->vn,
      rtrCredits.at( credit->vn )
    );
    delete bev;
    break;
  }

  case baseMordredEvent::FLIT: {
    auto* flit = static_cast<MordredFlit*>( bev );
    auto* req  = flit->getRequest();
    if( !flit || !req )
      output->fatal( CALL_INFO, -1, "Null flit or request\n" );

    if( flit->ftype == MordredFlit::HEAD && req->getTraceType() == Request::FULL ) {
      output->output(
        "TRACE(%d): %" PRIu64 " ns received head flit from link %s\n",
        flit->req->getTraceID(),
        getCurrentSimTimeNano(),
        getName().c_str()
      );
    }

    if( flit->ftype == MordredFlit::TAIL ) {
      if( flit->vn >= numVns )
        output->fatal( CALL_INFO, -1, "Unsupported vn=%u\n", flit->vn );
      if( req->getTraceType() == Request::FULL ) {
        output->output(
          "TRACE(%d): %" PRIu64 " ns received tail flit from link %s\n",
          flit->req->getTraceID(),
          getCurrentSimTimeNano(),
          getName().c_str()
        );
      }
      inBuf.at( flit->vn ).push( req );
      const uint64_t noc_latency = getCurrentSimCycle() - flit->head_inject_cycle;
      totalNocLatency += static_cast<uint64_t>( std::ceil( noc_latency / 1000.0 ) );
      totalPackets++;
      totalNumFlits += ( flit->flit_id + 1 );
      if( receiveFunctor ) {
        const bool keep = ( *receiveFunctor )( static_cast<int>( flit->vn ) );
        if( !keep )
          receiveFunctor = nullptr;
      }
    }

    inReturnCredits.at( flit->vn )++;
    delete flit;
    break;
  }

  default: output->fatal( CALL_INFO, -1, "processIncoming: unknown event type=%d\n", (int) bev->getType() );
  }
}

// ---- Private helpers ----

void MordredNicPC::resizeVectors() {
  if( numVns == 0 )
    output->fatal( CALL_INFO, -1, "MordredNicPC: numVns is 0 in resizeVectors\n" );

  inBuf.resize( numVns );
  outBuf.resize( numVns );

  const auto credits = static_cast<int32_t>( outbufSize.getRoundedValue() / flitSize );
  rtrCredits.resize( numVns, 0 );
  outbufCredits.resize( numVns, credits );
  inReturnCredits.resize( numVns, 0 );
}

MordredInitEvent* MordredNicPC::getInitEvent( MordredInitEvent::Commands cmd ) {
  auto* ev = static_cast<MordredInitEvent*>( physChannel->recvUntimedData() );
  if( !ev )
    output->fatal( CALL_INFO, -1, "MordredNicPC: unable to recv init event (cmd=%d)\n", (int) cmd );
  if( ev->getType() != baseMordredEvent::INITIALIZATION )
    output->fatal( CALL_INFO, -1, "getInitEvent: unexpected event type=%d\n", (int) ev->getType() );
  if( ev->command != cmd )
    output->fatal( CALL_INFO, -1, "getInitEvent: unexpected command %d, expected %d\n", (int) ev->command, (int) cmd );
  return ev;
}

int32_t MordredNicPC::calcNumFlits( uint32_t num_bits ) {
  auto num_flits = static_cast<int32_t>( std::ceil( (float) num_bits / (float) flitSize ) );
  if( num_flits < 2 )
    num_flits = 2;
  return num_flits;
}
