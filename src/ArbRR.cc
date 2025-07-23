//
// ArbRR.cc
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//
//

#include <string>
#include <cinttypes>

// Local SST config
#include "sst_config.h"

#include "MordredEvents.h"
#include "ArbRR.h"

using namespace SST::Mordred;

ArbRR::ArbRR( ComponentId_t id, Params &params, uint32_t rtr_id, uint32_t num_ports, uint32_t num_vns, uint32_t num_vcs ) :
ArbAPI( id ),
rtrId( rtr_id ),
numPorts( num_ports ),
numVns( num_vns ),
numVcs( num_vcs )
{
  const auto verbosity = params.find<uint32_t>("verbose", 5);
  output = new Output("ArbRR[[" + std::to_string( rtrId ) + "]:@p:@t]: ", verbosity, 0, Output::STDOUT);

  resetSendingVnVc();
}

// This code is nearly vomit inducing; using continues to reduce the nesting a little bit
void ArbRR::arbitrate( std::vector<RtrPortControlAPI*> &ports, std::vector<RtrOwnedSharedObjs> &rtr_shared_objs ) {
  resetSendingVnVc();
  for( uint32_t i = 0, rcvportnum = recv_rr_port; i < numPorts; ++i, rcvportnum = ( rcvportnum + 1 ) % numPorts ) {
    if ( ports.at(rcvportnum) == nullptr )
      continue;
    if( ports.at( rcvportnum )->isRecvAllocatedFromSwitch() ) // already receiving from someone
      continue;
    // rcvportnum is not actively receiving from the switch, so RR through the ports and find the next sender
    for( uint32_t j = 0, sendportnum = send_rr_port; j < numVns; ++j, sendportnum = ( sendportnum + 1 ) % numPorts ) {
      if ( sendportnum == rcvportnum ) // disallow sending back to self
        continue;
      if ( ports.at( sendportnum ) == nullptr ) // skip invalid ports
        continue;
      if( ports.at( sendportnum )->isSendAllocatedToSwitch() ) // already sending to someone
        continue;
      if( findSendableFlit( rcvportnum, ports.at( sendportnum ), rtr_shared_objs.at( sendportnum ) ) ) {
        // We have a sendable flit, so notify/update send/recv ports; clear flit from shared struct
        ports.at(sendportnum)->sendAllocateToSwitch( rcvportnum, sending_vn, sending_vc );
        auto dest_vc = ports.at( sendportnum )->getDestVc( sending_vn, sending_vc );
        ports.at(rcvportnum)->recvAllocateFromSwitch( sendportnum, sending_vn, dest_vc );
        rtr_shared_objs.at( sendportnum ).needSwitchAlloc.at(sending_vn).at(sending_vc) = nullptr;
        resetSendingVnVc();
        break; // found a matching sender, no need to do another one
      }
    }
  }
  // Update start values
  recv_rr_port = ( recv_rr_port + 1 ) % numPorts;
  send_rr_port = ( send_rr_port + 1 ) % numPorts;
  send_rr_vn = ( send_rr_vn + 1 ) % numVns;
  send_rr_vc = ( send_rr_vn + 1 ) % numVcs;
}

bool ArbRR::findSendableFlit( uint32_t rcvportnum, RtrPortControlAPI* &sendport, RtrOwnedSharedObjs &shared_obj ) {
  for ( uint32_t i = 0, vn = send_rr_vn; i < numVns; ++i, vn = (vn + 1) % numVns ) {
    for ( uint32_t j = 0, vc = send_rr_vc; j < numVcs; ++j, vc = (vc + 1) % numVcs ) {
      if ( shared_obj.needSwitchAlloc.at(vn).at(vc) != nullptr ) {
        if ( rcvportnum == sendport->getDestPort( vn, vc ) ) {
          // We have a winner!
          sending_vn = vn;
          sending_vc = vc;
          return true;
        }
      }
    }
  }
  return false;
}


#if 0
  for ( uint32_t i = 0; i < numPorts; i++) {
    if ( vnObjs->at(i).empty() ) // unconnected ports
      continue;

    if ( arbWinners->at( i ).first != UINT32_MAX ) // no need to rearbitrate yet
      continue;

    // j = Per VN loop
    // Since we're only doing 1 VN for now, I'm not going to worry about any kind of arbitration for it
    for ( uint32_t j = 0; j < vnObjs->at(i).size(); j++ ) {
      // k = per VC loop
      auto &vcHeads = vnObjs->at(i).at(j).vcHeads;
      for ( uint32_t k = 0; k < vcHeads.size(); k++ ) {
        auto flit = vcHeads.at( k );
        if ( flit != nullptr ) {
          //output->verbose( CALL_INFO, 5, 0, "Port %" PRIu32 " has a flit of type=%s; outstate=%d\n",
          //    i, flit->getFtypeStr().c_str(), outVcStates->at( flit->next_port ) );
          if ( flit->next_port >= numPorts )
            output->fatal( CALL_INFO, -1, "Flit next_port == %" PRIu32 " is invalid\n", flit->next_port );
          if ( vnObjs->at( flit->next_port ).empty() )
            output->fatal( CALL_INFO, -1, "Flit next_port == %" PRIu32 " is unconnected\n", flit->next_port );
          if ( outVcStates->at( flit->next_port ) != OutVcStateE::OUT_IDLE)
            continue;

          output->verbose( CALL_INFO, 5, 0, "Port %" PRIu32 " has flit %s\n",
            i, flit->pktIdStr().c_str() );
          // only moving a single flit right now, so i can just assume it happens - clearly will need to check the output
          // side of the dest port to ensure it can move through
          // TODO: get VN,VC from packet
          arbWinners->at(i) = std::make_pair( 0, 0 ); // 0,0 is the VN,VC of the receiving port for the flit
          outVcStates->at( flit->next_port ) = OutVcStateE::OUT_BUSY;
        }
      } // end k loop
    } // end j loop
  } // end i loop

}
#endif
