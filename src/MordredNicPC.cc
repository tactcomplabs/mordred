//
// MordredNicPC.cc
//
// Copyright (C) 2025-2026 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#include "MordredNicPC.h"

using namespace SST;
using namespace SST::Mordred;

MordredNicPC::MordredNicPC( ComponentId_t cid, Params& params, int vns )
    : MordredNicBase( cid, params, vns, "MordredNicPC" ) {
  physChannel = loadUserSubComponent<Prydwen::PhysChannelAPI>(
    "port_iface", ComponentInfo::SHARE_PORTS | ComponentInfo::INSERT_STATS, vns
  );
  if( !physChannel )
    output->fatal( CALL_INFO, -1, "MordredNicPC: no PhysChannelAPI subcomponent found in slot 'port_iface'\n" );

  physChannel->setNotifyOnReceive(
    new Prydwen::PhysChannelAPI::Handler2<MordredNicPC, &MordredNicPC::onReceive>( this )
  );

  output->verbose( CALL_INFO, 5, 0, "MordredNicPC constructed\n" );
}

bool MordredNicPC::onReceive( int sn_vn ) {
  SST::Event* ev = nullptr;
  while( ( ev = physChannel->recv( sn_vn ) ) != nullptr )
    processIncomingEvent( ev );
  return true;
}
