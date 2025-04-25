//
// MeshTopology.h
//
// Copyright (C) 2025-2025 Tactical Computing Laboratories, LLC
// All Rights Reserved
// contact@tactcomplabs.com
//
// See LICENSE in the top level directory for licensing details
//

#ifndef MESHTOPOLOGY_H
#define MESHTOPOLOGY_H

// Standard headers
#include <cstdint>
#include <queue>

// Local SST header
#include "sst_config.h"

// Local headers
#include "TopologyAPI.h"
#include "MordredEvents.h"

namespace SST::Mordred {

class MeshTopology : public TopologyAPI {

public:
  // register with the SST Core
  SST_ELI_REGISTER_SUBCOMPONENT(
    MeshTopology,
    "mordred",       // component library
    "MeshTopology",  // component name
    SST_ELI_ELEMENT_VERSION( 0, 0, 1 ),
    "Mesh Topology for NoC Router",
    SST::Mordred::TopologyAPI
  )

  // register the parameters
  SST_ELI_DOCUMENT_PARAMS(
    { "verbose",       "Sets the output verbsoity",                    "5" },
    { "xId", "ID in the X-dimension (expect 0 <= xId < xSize)", "-1"},
    { "yId", "ID in the Y dimension (expect 0 <= yId < ySize)", "-1"},
    { "xSize", "Number of points in the X dimension", "1"},
     {"ySize", "Number of points in the Y dimension", "1"}
  )

  // register the ports
  SST_ELI_DOCUMENT_PORTS()

  // register the subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  /// MeshTopology: constructor
  MeshTopology( ComponentId_t id, Params& params );

  /// MeshTopology: destructor
  ~MeshTopology() override = default;

  /// Send init messages (e.g, network discovery, etc)
  MordredFlit* sendInitMessage() final;

  /// Handle init messages (e.g, network discovery, etc)
  void processInitMessage( size_t topo_port_num, Event *ev ) final;

private:
  SST::Output* output;

  // Mesh parameters
  int32_t xId;
  int32_t yId;
  int32_t xSize;
  int32_t ySize;
  uint32_t num_links;

  std::queue<MordredFlit*>init_flit_vec;
  std::vector<int32_t> dir_topo_port_vec; // n,e,s,w order; content of -1 is unused, otherwise it's SimpleRtr.topo_port[]

};

} // namespace SST::Mordred


#endif //MESHTOPOLOGY_H
