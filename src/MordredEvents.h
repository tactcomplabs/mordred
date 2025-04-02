//
// SimpleRTR.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

// Standard headers
#include <cinttypes>
#include <vector>

// Local SST header
#include "sst_config.h"

// TODO: Configure verbosity control

namespace SST {
namespace Mordred {

// currently just using sst-elements/src/sst/elements/simpleElementExample/basicEvent.h
class basicMordredEvent : public SST::Event {
public:
  basicMordredEvent() : SST::Event() { /* empty */ }

  // Example data members
  std::vector<char> payload;
  bool              last;

  // Events must provide a serialization function that serializes
  // all data members of the event
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    ser & payload;
    ser & last;
  }

  // Register this event as serializable
  ImplementSerializable( SST::Mordred::basicMordredEvent );
};

}  // namespace Mordred
}  // namespace SST