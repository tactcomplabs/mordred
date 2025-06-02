//
// MordredEvents.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MORDREDEVENTS_H
#define MORDREDEVENTS_H

// Standard headers
#include <cinttypes>
#include <utility>

// Local SST header
#include "sst_config.h"

/**
 * For merlin, here is the "standard" approach for packet types from endpoint to endpoint:
 * - start with an endpoint event that creates a SimpleNetwork::Request; the endpoint event
 *    is stored into the SimpleNetwork::Request as the payload
 * - SimpleNetwork::Request is passed into the NIC (linkControl) and the NIC creates a wrapping event,
 *    RtrEvent.  The RtrEvent is transmitted on the SST::Link from the endpt NIC to the router
 * - The router then creates yet another wrapping event (internal_router_event) to use while the
 *    packet traverses the network (this transition happens in topology->proecess_input() ).
 * - The router removes the outer shell (internal_router_event) when sending the packet to the endpt
 *    NIC (thus the endpt NIC gets a RtrEvent)
 * - Endpt NIC removes the RtrEvent shell and returns a SimpleNetwork::Request to the endpt
 * - Endpt will remove the SimpleNetwork::Request shell and finally, we have the original endpoint event
 */

/**
 * In comparison, what I've done here (for now anyway) is the following:
 * - The simpleTestEvent is the endpoint event; the TestEP is the endpoint and it creates the
 *    SimpleNetwork::Request wrapper for the simpleTestEvent. The request destination is the router
 *    number/ID of the desired endpoint
 * - The MordredNIC receives the SimpleNetwork::Request and creates a single MordredFlit that
 *    wraps the SimpleNetwork::Request.  The MordredFlit is transmitted on the SST::Link to the router
 * - The network passes around the MordredFlit until we hit the desired endpoint router; the MordredFlit
 *    is sent to the endpt NIC.
 * - The MordredNIC removes the MordredFlit wrapper and returns the SimpleNetwork::Request to the endpt
 * - Endpt code (TestEP) removes the SimpleNetwork::Request shell and prints out the message stored in
 *    the simpleTestEvent.
 */

namespace SST::Mordred {

// Use for setting up output masks - not really using at the moment, but may
// be desirable in the long run
constexpr uint32_t DEBUG_CONSTRUCTORS = (1UL << 0);
constexpr uint32_t DEBUG_INIT_PHASE   = (1UL << 1);

// TODO: Add a mask for debugging credits

// This is a very simple event being sent by the TestEP.
class simpleTestEvent : public Event {
public:
  simpleTestEvent() { /* empty */ }
  simpleTestEvent( std::string str_ ) : str( std::move(str_) ) { /* empty */ }

  void serialize_order(Core::Serialization::serializer& ser) override {
    Event::serialize_order(ser);
    ser & str;
  }

  ImplementSerializable( SST::Mordred::simpleTestEvent );
public:
  std::string str;
};

// Base event sent around the NoC.
class baseMordredEvent : public Event {
public:
  enum MordredEventType { FLIT, CREDIT, INITIALIZATION };

  baseMordredEvent( MordredEventType type_ ) : type( type_ ) { /* empty */ }

  void serialize_order(Core::Serialization::serializer& ser) override {
    Event::serialize_order(ser);
    ser & type;
  }

  MordredEventType getType() { return type; }

private:
  baseMordredEvent() {} // for serialization
  MordredEventType type;

  ImplementSerializable( SST::Mordred::baseMordredEvent );
};

// Used to initialize the network
class MordredInitEvent : public baseMordredEvent {
public:
  enum Commands { REPORT_ENDPOINT, REPORT_ROUTER, ROUTER_ID, PORT_NUM, ENDPOINT_ID, NUM_VNS,
                  NUM_VCS, FLIT_WIDTH, BUS_WIDTH, NUM_COMMANDS };
  MordredInitEvent() : baseMordredEvent( INITIALIZATION ) {}

  Commands command;
  uint32_t value;
  UnitAlgebra ua_value;

private:
  ImplementSerializable( SST::Mordred::MordredInitEvent )
};

// This is intended to be the basic Flit running around the NoC.
class MordredFlit final : public baseMordredEvent {
public:
  enum FlitTypeE { HEAD, BODY, TAIL, EMPTY, NUM_TYPES };

  MordredFlit() : baseMordredEvent( FLIT ){ /* empty */ }
  MordredFlit( Interfaces::SimpleNetwork::Request *req_ ) : baseMordredEvent( FLIT ), req( req_ ) { /* empty */ }
  MordredFlit( Interfaces::SimpleNetwork::Request *req_, FlitTypeE ftype_, uint64_t pkt_id, uint32_t flit_id_ ) :
    baseMordredEvent( FLIT ), ftype( ftype_ ), req( req_ ), packet_id( pkt_id ), flit_id( flit_id_ ) { /* empty */ }

  Interfaces::SimpleNetwork::Request *getRequest() { return req; }

  /** START: Data members of the flit **/
  FlitTypeE ftype{NUM_TYPES};

  // This is what the endpoint passes into the MordredNIC; within
  // this request is the data packet (SST::Event) that one endpoint
  // wants to pass to another endpoint
  Interfaces::SimpleNetwork::Request  *req;

  uint32_t vn{0};
  uint32_t next_port{UINT32_MAX};
  uint32_t next_vc{UINT32_MAX};
  uint32_t cur_vc{0}; // TODO: Start using this rather than assuming 0 everywhere
  uint64_t packet_id{};
  uint32_t flit_id{};
  /** END: Data members of the flit **/

  std::string getFtypeStr() {
    switch( ftype ) {
      case HEAD: return "HEAD";
      case BODY: return "BODY";
      case TAIL: return "TAIL";
      case EMPTY: return "EMPTY";
      default: return "UNKNOWN";
    }
  }

  std::string pktIdStr() {
    std::string ss = "[Src.PktId.FlitId]=[" + std::to_string( req->src ) + "." + std::to_string( packet_id ) + "." + std::to_string( flit_id ) + "]";
    return ss;
  }

  // Events must provide a serialization function that serializes
  // all data members of the event
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    ser & ftype;
    ser & req;
    ser & vn;
    ser & next_port;
    ser & next_vc;
    ser & cur_vc;
    ser & packet_id;
    ser & flit_id;
  }

  // Register this event as serializable
  ImplementSerializable( SST::Mordred::MordredFlit );
};

// Unused at the moment, but more-or-less borrowed from merlin's
// credit_event in router.h
class MordredCreditEvent : public baseMordredEvent {
public:
  uint32_t vn;
  uint32_t vc;
  int32_t credits;

  MordredCreditEvent() : baseMordredEvent( CREDIT ) {}

  // TODO: Delete this constructor
  //MordredCreditEvent( uint32_t vc_, int32_t credits_ ) :
  //baseMordredEvent( CREDIT ), vc( vc_ ), credits( credits_ ) {}

  MordredCreditEvent( uint32_t vn_, uint32_t vc_, int32_t credits_ ) :
  baseMordredEvent( CREDIT ), vn( vn_ ), vc( vc_ ), credits( credits_ ) { /* empty */ }

  void serialize_order(Core::Serialization::serializer& ser) override {
    baseMordredEvent::serialize_order(ser);
    ser & vc;
    ser & credits;
  }

private:
  ImplementSerializable( SST::Mordred::MordredCreditEvent );
};


/**
 * So the naming here is probably horrible, but this is a collection of
 * per-VN data structures that the router owns.  Most everything in
 * this structure is going to be a vector because we many of the things
 * are needed for each VC as well.
 */
struct RtrOwnedVnObj {

  // For each VC, the HEAD flit of the input buffer; this flit is used for making routing/arbitration decisions
  std::vector<MordredFlit*> vcHeads;

  // Other fields to add?
  // vc state
  // next vn -- packets shouldn't really be changing vns, but maybe they could if bridging networks
  // next vc -- this gets maintained from HEAD to TAIL flit

  // TODO: May want to combine these functions, esp if we always use them together
  void allocateVecs( uint32_t num_vcs ) {
    vcHeads.resize( num_vcs );
  }

  void initVecs() {
    for( auto &vc_head : vcHeads ) {
      vc_head = nullptr;
    }
  }

};


} // namespace SST::Mordred


#endif
