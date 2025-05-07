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
#include <cinttypes>

#include "TestEP.h"
#include "MordredEvents.h"

using namespace SST;
using namespace SST::Mordred;

TestEP::TestEP( ComponentId_t cid, Params& params ) : Component( cid ) {

  auto Verbosity = params.find<uint32_t>( "verbose", 5 );
  // Initialize the output handler
  output.init( "TestEP[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  auto clockFreq = params.find<std::string>("clock", "1GHz");
  timeConverter = registerClock( clockFreq, new Clock::Handler2<TestEP, &TestEP::clockTick>(this) );
  registerAsPrimaryComponent();
  primaryComponentDoNotEndSim();

  nocIface = loadUserSubComponent<Interfaces::SimpleNetwork>( "noc_iface", ComponentInfo::SHARE_NONE, 1 );
  if ( !nocIface )
    output.fatal( CALL_INFO, -1, "Failed to load nocIface\n" );

  output.verbose( CALL_INFO, 5, 0, "Constructor complete for %s with cid=%" PRIu64 ".\n", getName().c_str(), cid );
  output.flush();
}

void TestEP::init( uint32_t phase ) {
  output.verbose( CALL_INFO, 5, 0, "TestEP::init(%" PRIu32 ")\n", phase );
  output.flush();

  nocIface->init( phase );
}

void TestEP::setup() {

}

void TestEP::complete( uint32_t phase ) {

}

void TestEP::finish() {

}

bool TestEP::clockTick( Cycle_t cycle ) {
  output.verbose( CALL_INFO, 3, 0, "Cycle=%" PRIu64 "\n", cycle );
  if ( cycle == 10 ) {
    primaryComponentOKToEndSim();
  }
  return false;
}


void TestEP::handleIncomingPacket( SST::Event* ev ) {
  MordredFlit* mev = static_cast<MordredFlit*>( ev );
  if( mev ) {
    output.verbose( CALL_INFO, 5, 0, "TestEP::handle in packet\n" );
    delete mev;
  } else {
    output.fatal( CALL_INFO, -1, "Error! Bad mev type received by %s\n", getName().c_str() );
  }
}