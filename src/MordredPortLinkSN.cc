//
// MordredPortLinkSN.cc
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "sst_config.h"

#include "MordredPortLinkSN.h"
#include "MordredEvents.h"

using namespace SST;
using namespace SST::Mordred;
using namespace SST::Interfaces;

// ---- Constructor ----

MordredPortLinkSN::MordredPortLinkSN( ComponentId_t id, Params& params, int num_vns )
  : Interfaces::SimpleNetwork( id ),
    numVns( num_vns )
{
  const auto verbosity = params.find<uint32_t>( "verbose", 0 );
  output = new Output(
    "MordredPortLinkSN[" + getName() + ":@p:@t]: ",
    verbosity, 0, Output::STDOUT );

  portName = params.find<std::string>( "port_name", "port" );

  link = configureLink(
    portName,
    new Event::Handler2<MordredPortLinkSN,
                        &MordredPortLinkSN::handleIncoming>( this ) );
  if ( !link )
    output->fatal( CALL_INFO, -1,
      "MordredPortLinkSN: unable to configure link '%s'\n", portName.c_str() );

  // One receive queue per VN
  recvQueues.resize( static_cast<size_t>( num_vns ) );

  // linkBW is used only for getLinkBW() diagnostics; set a placeholder.
  // The actual wire bandwidth is determined by the link's latency/timing.
  linkBW = UnitAlgebra( "1b" );

  output->verbose( CALL_INFO, 1, 0,
    "Constructed; port='%s', num_vns=%d\n", portName.c_str(), numVns );
}

// ---- Lifecycle (thin adapter — no internal protocol) ----

void MordredPortLinkSN::init( unsigned int /*phase*/ ) {
  // The Mordred init protocol is driven entirely by the parent (RtrPortControlSN)
  // via sendUntimedData / recvUntimedData.  Nothing to do here.
  initialized = true;
}

void MordredPortLinkSN::setup() {
  // Nothing needed
}

void MordredPortLinkSN::complete( unsigned int /*phase*/ ) {
  // Nothing needed
}

// ---- Untimed data (init phase) ----

void MordredPortLinkSN::sendUntimedData( Request* req ) {
  SST::Event* ev = req->takePayload();
  delete req;
  link->sendUntimedData( ev );
}

SimpleNetwork::Request* MordredPortLinkSN::recvUntimedData() {
  SST::Event* ev = link->recvUntimedData();
  if ( !ev ) return nullptr;
  auto* req = new Request();
  req->givePayload( ev );
  return req;
}

// ---- Timed send / receive ----

bool MordredPortLinkSN::send( Request* req, int /*vn*/ ) {
  SST::Event* ev = req->takePayload();
  delete req;
  link->send( ev );
  return true;
}

SimpleNetwork::Request* MordredPortLinkSN::recv( int vn ) {
  if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) ) return nullptr;
  auto& q = recvQueues.at( static_cast<size_t>( vn ) );
  if ( q.empty() ) return nullptr;
  Request* req = q.front();
  q.pop();
  return req;
}

// ---- Incoming link handler ----

void MordredPortLinkSN::handleIncoming( SST::Event* ev ) {
  auto* bev = static_cast<baseMordredEvent*>( ev );
  if ( !bev )
    output->fatal( CALL_INFO, -1,
      "MordredPortLinkSN: null or non-Mordred event on link '%s'\n", portName.c_str() );

  // Determine VN from event payload
  int vn = 0;
  switch ( bev->getType() ) {
  case baseMordredEvent::FLIT: {
    auto* flit = static_cast<MordredFlit*>( bev );
    vn = static_cast<int>( flit->vn );
    break;
  }
  case baseMordredEvent::CREDIT: {
    auto* credit = static_cast<MordredCreditEvent*>( bev );
    vn = static_cast<int>( credit->vn );
    break;
  }
  default:
    // INITIALIZATION events should not arrive during timed simulation;
    // fall back to VN 0 to avoid dropping the event.
    vn = 0;
    break;
  }

  if ( vn < 0 || vn >= static_cast<int>( recvQueues.size() ) )
    output->fatal( CALL_INFO, -1,
      "MordredPortLinkSN: incoming VN %d out of range [0,%d)\n",
      vn, static_cast<int>( recvQueues.size() ) );

  auto* req = new Request();
  req->givePayload( ev );
  req->vn = vn;
  recvQueues.at( static_cast<size_t>( vn ) ).push( req );

  if ( recvNotifyHandler )
    (*recvNotifyHandler)( vn );
}
