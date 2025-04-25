//
// MordredEvents.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

// NOTE: Borrowing heavily from the Kingsley sst-element library and, to a lesser extent, the
// shogun sst-element library.


#ifndef MORDREDEVENTS_H
#define MORDREDEVENTS_H

// Standard headers
#include <cinttypes>
#include <vector>

// Local SST header
#include "sst_config.h"

namespace SST {
namespace Mordred {

class baseMordredEvent : public Event {
public:
  enum MordredEventType { HEAD_FLIT, BODY_FLIT, TAIL_FLIT, CREDIT, INITIALIZATION };

  baseMordredEvent( MordredEventType type_ ) : SST::Event(), type( type_ ) { /* empty */ }

  void serialize_order(Core::Serialization::serializer& ser) override {
    Event::serialize_order(ser);
    ser & type;
  }

private:
  baseMordredEvent() {} // for serialization
  MordredEventType type;

  ImplementSerializable( SST::Mordred::baseMordredEvent );
};

// Used to initialize the network
class MordredInitEvent : public baseMordredEvent {
public:
  enum Commands { REPORT_ENDPOINT };
  MordredInitEvent() : baseMordredEvent( INITIALIZATION ) {}

  Commands command;
  int32_t value;
  UnitAlgebra ua_value;

private:
  ImplementSerializable( SST::Mordred::MordredInitEvent )
};

// currently just using sst-elements/src/sst/elements/simpleElementExample/basicEvent.h
// TODO: Inherit from baseMordredEvent
class MordredFlit final : public SST::Event {
public:
  MordredFlit() : SST::Event() { /* empty */ }

  enum FlitTypeE { HEAD, BODY, TAIL, CREDIT, EMPTY, NUM_TYPES };
  FlitTypeE ftype{NUM_TYPES};

  // Using this as a placeholder
  std::string src_name;
  uint64_t src;
  uint64_t dest;

  uint64_t datum;

  // Events must provide a serialization function that serializes
  // all data members of the event
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    ser & ftype;
    ser & src_name;
    ser & src;
    ser & dest;
    ser & datum;
  }

  // Register this event as serializable
  ImplementSerializable( SST::Mordred::MordredFlit );
};

}  // namespace Mordred
}  // namespace SST

#endif
