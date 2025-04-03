//
// SimpleRTR.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include <string>

#include "TestEP.h"
#include "MordredEvents.h"

using namespace SST;
using namespace SST::Mordred;

TestEP::TestEP( ComponentId_t cid, Params& params ) : Component( cid ) {

  auto Verbosity = params.find<uint32_t>( "verbose", 5 );
  // Initialize the output handler
  output.init( "TestEP[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  localPort = configureLink( "port", new Event::Handler2<TestEP, &TestEP::handleIncomingPacket>( this ) );
  if( !localPort )
    output.fatal( CALL_INFO, -1, "Error in %s: unable to configure link\n", getName().c_str() );

  output.verbose( CALL_INFO, 5, 0, "Constructor complete for %s.\n", getName().c_str() );
  output.flush();
}

void TestEP::init( uint32_t phase ) {
  output.verbose( CALL_INFO, 5, 0, "TestEP::init(%" PRIu32 ")\n", phase );
  output.flush();
}

void TestEP::setup() {

}

void TestEP::complete( uint32_t phase ) {

}

void TestEP::finish() {

}

void TestEP::handleIncomingPacket( SST::Event* ev ) {
  basicMordredEvent* mev = static_cast<basicMordredEvent*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "TestEP::handle in packet\n" );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s\n", getName().c_str() );
  }
}