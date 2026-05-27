//
// RtrPortControlPC.cc
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "RtrPortControlPC.h"

using namespace SST;
using namespace SST::Mordred;

RtrPortControlPC::RtrPortControlPC(
  ComponentId_t id, Params& params, TopologyAPI* topology,
  RtrOwnedSharedObjs* rtr_shared_objs, uint32_t rtr_num, uint32_t port_num
) : RtrPortControlBase( id, params, topology, rtr_shared_objs, rtr_num, port_num, "RtrPortControlPC" ) {
  physChannel = loadUserSubComponent<Prydwen::PhysChannelAPI>(
    "port_iface", ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, static_cast<int>( numVns )
  );
  if( !physChannel )
    output->fatal( CALL_INFO, -1, "RtrPortControlPC: no PhysChannelAPI subcomponent in slot 'port_iface'\n" );

  physChannel->setNotifyOnReceive(
    new Prydwen::PhysChannelAPI::Handler2<RtrPortControlPC, &RtrPortControlPC::onReceive>( this )
  );

  output->verbose(
    CALL_INFO, 1, 0,
    "Constructor complete; [Rtr.Port]=[%" PRIu32 ".%" PRIu32 "], inbuf=%" PRIu32 "b, outbuf=%" PRIu32 "b\n",
    rtrId, portId, inBufSize, outBufSize
  );
}

bool RtrPortControlPC::onReceive( int sn_vn ) {
  SST::Event* ev = nullptr;
  while( ( ev = physChannel->recv( sn_vn ) ) != nullptr )
    processIncomingEvent( ev );
  return true;
}
