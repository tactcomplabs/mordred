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
using namespace SST::Interfaces;

// ---- Constructor ----

GenericPhysChannel::GenericPhysChannel( ComponentId_t id, Params& params, int num_vns )
  : Interfaces::SimpleNetwork( id ),
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

  // One receive queue per VN
  recvQueues.resize( static_cast<size_t>( num_vns ) );

  // Placeholder BW — actual wire speed is determined by the SST::Link timing
  linkBW = UnitAlgebra( "1b" );

  output->verbose( CALL_INFO, 1, 0,
    "Constructed; port='%s', num_vns=%d\n", portName.c_str(), numVns );
}

// ---- Lifecycle ----

void GenericPhysChannel::init( unsigned int /*phase*/ ) {
  // Thin adapter: the Mordred init protocol is driven by the parent via
  // sendUntimedData / recvUntimedData.  Nothing else to do here.
  initialized = true;
}

void GenericPhysChannel::setup() {
  // Nothing needed
}

void GenericPhysChannel::complete( unsigned int /*phase*/ ) {
  // Nothing needed
}

// ---- Untimed data (init phase) ----

void GenericPhysChannel::sendUntimedData( Request* req ) {
  link->sendUntimedData( new RequestWrapperEvent( req ) );
}

SimpleNetwork::Request* GenericPhysChannel::recvUntimedData() {
  auto* ev = dynamic_cast<RequestWrapperEvent*>( link->recvUntimedData() );
  if ( !ev ) return nullptr;
  Request* req = ev->req;
  ev->req = nullptr; // prevent double-free; caller owns req now
  delete ev;
  return req;
}

// ---- Timed send / receive ----

bool GenericPhysChannel::send( Request* req, int /*vn*/ ) {
  link->send( new RequestWrapperEvent( req ) );
  return true;
}

SimpleNetwork::Request* GenericPhysChannel::recv( int vn ) {
  if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) ) return nullptr;
  auto& q = recvQueues.at( static_cast<size_t>( vn ) );
  if ( q.empty() ) return nullptr;
  Request* req = q.front();
  q.pop();
  return req;
}

// ---- Incoming link handler ----

void GenericPhysChannel::handleIncoming( SST::Event* ev ) {
  auto* wev = dynamic_cast<RequestWrapperEvent*>( ev );
  if ( !wev )
    output->fatal( CALL_INFO, -1,
      "GenericPhysChannel: received unexpected event type on link '%s'\n",
      portName.c_str() );

  Request* req = wev->req;
  wev->req = nullptr;
  delete wev;

  const int vn = req->vn;
  if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) )
    output->fatal( CALL_INFO, -1,
      "GenericPhysChannel: incoming VN %d out of range [0,%d)\n",
      vn, static_cast<int>( recvQueues.size() ) );

  recvQueues.at( static_cast<size_t>( vn ) ).push( req );

  if ( recvNotifyHandler )
    (*recvNotifyHandler)( vn );
}
