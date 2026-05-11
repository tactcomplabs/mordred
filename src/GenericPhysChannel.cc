//
// GenericPhysChannel.cc
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "sst_config.h"

#include "GenericPhysChannel.h"

using namespace SST;
using namespace SST::Mordred;

// ---- Constructor ----

GenericPhysChannel::GenericPhysChannel( ComponentId_t id, Params& params, int num_vns )
  : PhysChannelAPI( id, params, num_vns ),
    numVns( num_vns )
{
  const auto verbosity = params.find<uint32_t>( "verbose", 0 );
  output = new Output(
    "GenericPhysChannel[" + getName() + ":@p:@t]: ",
    verbosity, 0, Output::STDOUT );

  portName = params.find<std::string>( "port_name", "port" );

  link = configureLink(
    portName,
    new Event::Handler2<GenericPhysChannel,
                        &GenericPhysChannel::handleIncoming>( this ) );
  if ( !link )
    output->fatal( CALL_INFO, -1,
      "GenericPhysChannel: unable to configure link '%s'\n", portName.c_str() );

  recvQueues.resize( static_cast<size_t>( num_vns ) );

  // Placeholder BW — actual wire speed is determined by the SST::Link timing
  linkBW = UnitAlgebra( "1b" );

  output->verbose( CALL_INFO, 1, 0,
    "Constructed; port='%s', num_vns=%d\n", portName.c_str(), numVns );
}

// ---- Lifecycle ----

void GenericPhysChannel::init( unsigned int /*phase*/ ) {
  initialized = true;
}

void GenericPhysChannel::setup() {}

void GenericPhysChannel::complete( unsigned int /*phase*/ ) {}

// ---- Untimed data (init phase) ----

void GenericPhysChannel::sendUntimedData( SST::Event* payload ) {
  // Send directly — no wrapper needed; untimed path has no VN demux
  link->sendUntimedData( payload );
}

SST::Event* GenericPhysChannel::recvUntimedData() {
  return link->recvUntimedData();
}

// ---- Timed send / receive ----

bool GenericPhysChannel::send( SST::Event* payload, int vn ) {
  link->send( new PhysChannelLinkEvent( payload, vn ) );
  return true;
}

SST::Event* GenericPhysChannel::recv( int vn ) {
  if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) ) return nullptr;
  auto& q = recvQueues.at( static_cast<size_t>( vn ) );
  if ( q.empty() ) return nullptr;
  SST::Event* ev = q.front();
  q.pop();
  return ev;
}

// ---- Incoming link handler ----

void GenericPhysChannel::handleIncoming( SST::Event* ev ) {
  auto* lev = dynamic_cast<PhysChannelLinkEvent*>( ev );
  if ( !lev )
    output->fatal( CALL_INFO, -1,
      "GenericPhysChannel: received unexpected event type on link '%s'\n",
      portName.c_str() );

  SST::Event* payload = lev->payload;
  const int   vn      = lev->vn;
  lev->payload = nullptr; // caller owns payload now
  delete lev;

  if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) )
    output->fatal( CALL_INFO, -1,
      "GenericPhysChannel: incoming VN %d out of range [0,%d)\n",
      vn, static_cast<int>( recvQueues.size() ) );

  recvQueues.at( static_cast<size_t>( vn ) ).push( payload );

  if ( recvNotifyHandler )
    (*recvNotifyHandler)( vn );
}
