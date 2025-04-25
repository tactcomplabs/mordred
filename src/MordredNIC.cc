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

MordredNIC::MordredNIC( ComponentId_t cid, Params& params, int32_t vns=1 ) :
   SimpleNetwork(cid),
   netID(-1),
   bw("1GB/s")
{
  const auto verbosity = params.find<uint32_t>("verbose", 0);
  output = new Output("ShogunNIC-Startup ", verbosity, 0, Output::STDOUT);

  // Configure the links
  // For now give it a fake timebase.  Will give it the real timebase during init
  std::string port_name("port");
  //if ( isAnonymous())
  //  port_name = params.find<std::string>("port_name");
  rtr_link = configureLink(port_name, std::string("1GHz"),
      new Event::Handler<MordredNIC>(this,&MordredNIC::handleIncomingPacket));

  if (vns >= 1)
    out_credits.resize( static_cast<size_t>(vns) );
  else
    output->fatal( CALL_INFO, -1, "Invalid number of vns=%" PRId32 "\n", vns );

  output->verbose(CALL_INFO, 5, 0, "MordredNIC constructed\n");
  output->flush();

}

// TODO: This is probably way off base
void MordredNIC::init( uint32_t phase ) {
  Event *ev;
  MordredInitEvent* init_ev;

  switch ( init_state ) {
  case SEND_ENDPT_NOTIFY:
    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::REPORT_ENDPOINT;
    rtr_link->sendUntimedData( init_ev );
    init_state = INIT_NETWORK_CONFIG;
    break;

  case INIT_NETWORK_CONFIG:
    ev = rtr_link->recvUntimedData();
    if (ev == nullptr) break;
    break;

  case INIT_ENDPT_CONFIG:
    break;

  case INIT_COMPLETE:
    break;

  default:
    output->fatal( CALL_INFO, -1, "Invalid state\n" );
  }
}

void MordredNIC::setup() {
  output->verbose(CALL_INFO, 5, 0, "MordredNIC setup\n");
  output->flush();
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
  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return false;
}

SST::Interfaces::SimpleNetwork::Request* MordredNIC::recv( int32_t vn ) {
  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
  return nullptr;
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

void MordredNIC::handleIncomingPacket( SST::Event* ev ) {
  // TODO: If it's a credit, add to the credit
  // if it's a flit, add it to a buffer for the surrounding unit to reassemble, etc

  output->flush();
  output->fatal( CALL_INFO, -1, "Not yet implemented\n" );
}





