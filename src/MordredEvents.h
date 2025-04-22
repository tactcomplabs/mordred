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

namespace SST {
namespace Mordred {

// currently just using sst-elements/src/sst/elements/simpleElementExample/basicEvent.h
class basicMordredEvent final : public SST::Event {
public:
  basicMordredEvent() : SST::Event() { /* empty */ }

  // Example data members
  std::vector<uint64_t> payload;
  std::string           src_name;
  bool                  last{};

  // will need things like destination, class/priority, etc

  // Events must provide a serialization function that serializes
  // all data members of the event
  void serialize_order( SST::Core::Serialization::serializer& ser ) override {
    Event::serialize_order( ser );
    ser & payload;
    ser & src_name;
    ser & last;
  }

  // Register this event as serializable
  ImplementSerializable( SST::Mordred::basicMordredEvent );
};

}  // namespace Mordred
}  // namespace SST