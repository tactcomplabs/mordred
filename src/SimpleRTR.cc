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
#include <cstdint>

#include "MordredEvents.h"
#include "RtrPortControlAPI.h"
#include "SimpleRTR.h"

using namespace SST;
using namespace SST::Mordred;

SimpleRTR::SimpleRTR( ComponentId_t cid, Params& params ) : Component( cid ) {

  auto Verbosity = params.find<uint32_t>( "verbose", 5 );
  // Initialize the output handler
  output.init( "SimpleRTR[" + getName() + ":@p:@t]: ", Verbosity, 0, SST::Output::STDOUT );

  id = params.find<uint32_t>("id",UINT32_MAX);
  if ( id == UINT32_MAX ) {
    output.fatal(CALL_INFO, -1, "SimpleRTR requires id to be specified\n");
  }

  auto clockFreq = params.find<std::string>("clock", "1GHz");
  timeConverter = registerClock( clockFreq, new Clock::Handler2<SimpleRTR, &SimpleRTR::clockTick>(this) );

  numPorts = params.find<uint32_t>("num_ports", 3);
  numLocalPorts = params.find<uint32_t>( "num_local_ports", 1 );
  numVns = params.find<uint32_t>( "num_vns", 1 ); // commented out as an ELI param for now
  numVcs = params.find<uint32_t>( "num_vcs", 1 );

  // TODO: Required for all topologies?  Probably not....
  if ( numPorts <= numLocalPorts )
    output.fatal( CALL_INFO, -1, "num_ports must be greater than num_local_ports\n" );

  // Load subcomponents
  topology = loadUserSubComponent<TopologyAPI>( "topology", ComponentInfo::SHARE_NONE, id, numPorts, numLocalPorts );
  if ( !topology )
    output.fatal( CALL_INFO, -1, "Couldn't load topology\n" );

  perPortSharedObjs.resize( numPorts );
  // Configure local/endpt ports -- borrowed this approach from
  // sst-elements/src/sst/elements/simpleElementExample/basicLinks.cc
  for ( uint32_t i = 0; i < numPorts; i++ ) {
    std::string linkname = "port" + std::to_string(i);
    if ( isPortConnected( linkname ) ) {
      perPortSharedObjs.at(i).allocateVecs( numVns, numVcs );
      portsVec.push_back( loadAnonymousSubComponent<RtrPortControlAPI>("mordred.rtrPortControl", "portcontrol", (int)i,
        ComponentInfo::SHARE_PORTS, params, topology, &perPortSharedObjs[i], id, i) );
    } else {
      output.verbose( CALL_INFO, 5, 0, "Port %u with name=%s unconnected\n", i, linkname.c_str() );
      portsVec.push_back( nullptr );
    }
  }

  arbiter = loadAnonymousSubComponent<XbarArbAPI>( "mordred.xbarArbRR", "arbiter", 0,
    ComponentInfo::SHARE_NONE, params, id, numPorts, numVns, numVcs );
  if (arbiter == nullptr) {
    output.fatal( CALL_INFO, -1, "arbiter is a nullptr\n" );
  }

  vcAlloc = loadAnonymousSubComponent<VcAllocAPI>( "mordred.VcAllocRR", "vcAlloc", 0,
    ComponentInfo::SHARE_NONE, params, id, numPorts, numVns, numVcs );
  if ( vcAlloc == nullptr ) {
    output.fatal( CALL_INFO, -1, "vcAlloc is a nullptr\n" );
  }

  // Try registering a stat
  std::string rtrstr = "rtr[" + std::to_string( id ) + "]";
  tickCounter = registerStatistic<uint64_t>( "tick10_cnt", rtrstr.c_str() );
  if ( tickCounter->isNullStatistic() )
    output.verbose( CALL_INFO, 5, 0, "tickCounter STAT is NULL\n" );

  output.verbose(
    CALL_INFO, 5, 0, "Constructor complete for %s. local_ports=%" PRIu32 "; rtr_ports=%" PRIu32 "\n",
    getName().c_str(), numLocalPorts, numPorts-numLocalPorts
  );
  output.flush();
}

SimpleRTR::~SimpleRTR() {
  for ( auto &port : portsVec )
      delete port;
  delete vcAlloc;
  delete arbiter;
  delete topology;
}

void SimpleRTR::init( uint32_t phase ) {
  output.verbose(CALL_INFO, 5, 0, "SimpleRTR::init(%" PRIu32 ")\n", phase);
  output.flush();

  topology->init( phase );
  vcAlloc->init( phase );
  for ( auto &port : portsVec )
    if ( port != nullptr )
      port->init( phase );
}

void SimpleRTR::setup() {
  output.verbose(CALL_INFO, 5, 0, "SimpleRTR::setup\n");
  output.flush();
  topology->setup();
  vcAlloc->setup();
  for ( auto &port : portsVec )
    if ( port != nullptr )
      port->setup();
}

bool SimpleRTR::clockTick( Cycle_t cycle ) {
  // May want/need to look at how we want to time/order ticking the ports and running the crossbar/arbitration here

  if ( cycle % 10 == 0 )
    tickCounter->addData( 1 );

  // For all router ports, see if we can receive a flit through the crossbar
  for ( uint32_t i = 0; i < numPorts; i++ ) {
    if ( portsVec.at( i ) == nullptr )
      continue;

    auto sending_port = portsVec.at( i )->getSendingPort();
    if ( sending_port == UINT32_MAX ) // can't receive a flit, move on
      continue;

    // Get the flit from the sender -- multiple checks there for invalid/null concerns
    auto flit = portsVec.at( sending_port )->getInBufFlit();

    // Give the flit to the receiver
    portsVec.at( i )->recvOutBufFlit( flit );

    if ( flit->ftype == MordredFlit::TAIL ) {
      // These MUST be reset prior to calling the resetSwitch{Send,Recv}Allocation functions in a clockTick
      // as the rely on the values that are reset when calling them
      portsVec.at(sending_port)->resetPerVcDest();
      portsVec.at(i)->resetPerVcSrc();
      // The switch allocation could be done more frequently than on a packet basis
      portsVec.at(sending_port)->resetSwitchSendAllocation();
      portsVec.at(i)->resetSwitchRecvAllocation();
      output.verbose( CALL_INFO, 3, 0, "Tail flit observed\n");
    }
  }

  //output.verbose( CALL_INFO, 3, 0, "Cycle=%" PRIu64 "\n", cycle );
  //output.flush();
  arbiter->arbitrate( portsVec, perPortSharedObjs );

  vcAlloc->arbitrate( portsVec, perPortSharedObjs );


  // Let the port do its work
  for ( auto &port : portsVec ) {
    if (port == nullptr) continue;
    port->ClockTick( cycle );
  }

  return false;
}
