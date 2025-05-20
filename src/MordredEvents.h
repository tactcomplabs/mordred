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

// Local SST header
#include "sst_config.h"

namespace SST::Mordred {

// Use for setting up output masks - not really using at the moment, but may
// be desirable in the long run
constexpr uint32_t DEBUG_CONSTRUCTORS = (1UL << 0);
constexpr uint32_t DEBUG_INIT_PHASE   = (1UL << 1);

// This is a very simple event being sent by the TestEP.
class simpleTestEvent : public Event {
public:
  simpleTestEvent() { /* empty */ }
  simpleTestEvent( std::string str_ ) : str( str_ ) { /* empty */ }

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
  enum Commands { REPORT_ENDPOINT, REPORT_ROUTER, ROUTER_ID, PORT_NUM, ENDPOINT_ID, NUM_VCS, FLIT_WIDTH, BUS_WIDTH, NUM_COMMANDS };
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
  MordredFlit() : baseMordredEvent( FLIT ){ /* empty */ }
  MordredFlit( Interfaces::SimpleNetwork::Request *req_ ) : baseMordredEvent( FLIT ), req( req_ ) { /* empty */ }

  Interfaces::SimpleNetwork::Request *getRequest() { return req; }

  enum FlitTypeE { HEAD, BODY, TAIL, EMPTY, NUM_TYPES };
  FlitTypeE ftype{NUM_TYPES};

  // This is what the endpoint passes into the MordredNIC; within
  // this request is the data packet (sst Event) that one endpoint
  // wants to pass to another endpoint
  Interfaces::SimpleNetwork::Request  *req;

  uint32_t next_port{UINT32_MAX};
  //uint32_t next_vc{}; Need? Most likely
  // uint32_t cur_vc{UINT32_MAX}; // TODO: Start using me rather than assuming 0 everywhere

  // Events must provide a serialization function that serializes
  // all data members of the event
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    ser & ftype;
    ser & req;
  }

  // Register this event as serializable
  ImplementSerializable( SST::Mordred::MordredFlit );
};

// Unused at the moment, but more-or-less borrowed from merlin's
// credit_event in router.h
class MordredCreditEvent : public baseMordredEvent {
public:
  uint32_t vc;
  int32_t credits;

  MordredCreditEvent() : baseMordredEvent( CREDIT ) {}

  MordredCreditEvent( uint32_t vc_, int32_t credits_ ) :
  baseMordredEvent( CREDIT ), vc( vc_ ), credits( credits_ ) {}

  void serialize_order(Core::Serialization::serializer& ser) override {
    baseMordredEvent::serialize_order(ser);
    ser & vc;
    ser & credits;
  }

private:
  ImplementSerializable( SST::Mordred::MordredCreditEvent );
};

} // namespace SST::Mordred


#endif
