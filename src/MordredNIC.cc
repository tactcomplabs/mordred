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

MordredNIC::MordredNIC( ComponentId_t cid, Params& params, int32_t num_vns=1 ) :
   SimpleNetwork(cid),
   netID(-1),
   bw("1GB/s")
{
  const auto verbosity = params.find<uint32_t>("verbose", 0);
  output = new SST::Output("MordredNIC-Startup ", verbosity, 0, Output::STDOUT);

  // Validate vns
  if ( num_vns <= 0 ) {
    output->fatal( CALL_INFO, -1, "Invalid number of vns=%" PRId32 "; must be >= 1\n", num_vns );
  }
  size_t vns = static_cast<size_t>(num_vns);

  // Set up buffers (paritally borrowed from Kingsley)
  inbuf_size = params.find<UnitAlgebra>("in_buf_size", "1kB");
  if ( !inbuf_size.hasUnits("b") && !inbuf_size.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"in_buf_size must be specified in either "
                       "bits or bytes: %s\n",inbuf_size.toStringBestSI().c_str());
  }
  if ( inbuf_size.hasUnits("B") ) inbuf_size *= UnitAlgebra("8b/B");

  outbuf_size = params.find<UnitAlgebra>("out_buf_size", "1kB");
  if ( !outbuf_size.hasUnits("b") && !outbuf_size.hasUnits("B") ) {
    output->fatal(CALL_INFO,-1,"out_buf_size must be specified in either "
                       "bits or bytes: %s\n",outbuf_size.toStringBestSI().c_str());
  }
  if ( outbuf_size.hasUnits("B") ) outbuf_size *= UnitAlgebra("8b/B");

  in_buf.resize( vns );
  out_buf.resize( vns );

  rtr_credits.resize( vns, 0 );
  outbuf_credits.resize( vns );
  in_ret_credits.resize( vns );

  // Configure the links
  // For now give it a fake timebase.  Will give it the real timebase during init
  std::string port_name("port");
  //if ( isAnonymous())
  //  port_name = params.find<std::string>("port_name");
  rtr_link = configureLink(port_name, std::string("1GHz"),
      new Event::Handler<MordredNIC>(this,&MordredNIC::handleIncomingPacket));

  output->verbose(CALL_INFO, 5, 0, "MordredNIC constructed\n");
  output->flush();
}

// TODO: This is incomplete
void MordredNIC::init( uint32_t phase ) {
  Event *ev;
  MordredInitEvent* init_ev;

  switch ( init_state ) {
  case NOTIFY_RTR:
    init_ev = new MordredInitEvent();
    init_ev->command = MordredInitEvent::REPORT_ENDPOINT;
    rtr_link->sendUntimedData( init_ev );
    init_state = RCV_FLIT_SIZE;
    break;

  case RCV_FLIT_SIZE: {
    ev = rtr_link->recvUntimedData();
    if (ev == nullptr) break;
#if 0 // from Kingsley
    init_ev = static_cast<MordredInitEvent*>(ev);
    UnitAlgebra flit_size_ua = init_ev->ua_value;
    flit_size = flit_size_ua.getRoundedValue();

    UnitAlgebra link_clock = link_bw / flit_size_ua;

    TimeConverter* tc = getTimeConverter(link_clock);
    output->timing->setDefaultTimeBase(tc);

    for ( int i = 0; i < req_vns; ++i ) {
      outbuf_credits[i] = outbuf_size.getRoundedValue() / flit_size;
      in_ret_credits[i] = inbuf_size.getRoundedValue() /flit_size;
    }

    delete ev;
#endif
    init_state = WAIT_FOR_ID;
    break;
  }

  case WAIT_FOR_ID: {
    ev = rtr_link->recvUntimedData();
    if ( NULL == ev ) break;
#if 0 // from Kingsley
    init_ev = static_cast<MordredInitEvent*>(ev);
    id = init_ev->int_value;
    delete ev;

    // Send credit event to router
    credit_event* cr_ev = new credit_event(0,inbuf_size.getRoundedValue() / flit_size);
    rtr_link->sendUntimedData(cr_ev);
#endif
    // initialized = true;
    init_state = INIT_COMPLETE;
    break;
  }

  case INIT_COMPLETE:
    initialized = true;
    init_state = NUM_STATES;
    break;

  case NUM_STATES: [[fallthrough]];
  default:
    output->fatal( CALL_INFO, -1, "Invalid state\n" );
#if 0 // from Kingsley
    // For all other phases, look for credit events, any other
    // events get passed up to containing component by adding them
    // to init_events queue
    while ( ( ev = rtr_link->recvUntimedData() ) != NULL ) {
      BaseNocEvent* bev = static_cast<BaseNocEvent*>(ev);
      switch (bev->getType()) {
      case BaseNocEvent::CREDIT:
      {
        credit_event* ce = static_cast<credit_event*>(bev);
        // output->output->"%d: Got a credit event for VN %d with %d credits\n",id,ce->vn,ce->credits);
        if ( ce->vn < req_vns ) {  // Ignore credit events for VNs I don't have
          rtr_credits[ce->vn] += ce->credits;
        }
        delete ev;
        // if ( waiting && have_packets ) {
        //     output->timing->send(1,NULL);
        //     waiting = false;
        // }
      }
        break;
      case BaseNocEvent::PACKET:
        init_events.push_back(static_cast<NocPacket*>(ev));
        break;
      default:
        // This shouldn't happen.  Only NocPackets (PACKET
        // types) should not be handled in the LinkControl
        // object.
        // output->fatal(CALL_INFO, 1, "Reached state where a non-NocPacket was not handled.");
        break;
      }
    }
    break;

#endif

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





