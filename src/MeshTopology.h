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
  /// TODO: This is where I can include additional parameters since the constructor is called
  /// when loading the subcomponent
  MeshTopology( ComponentId_t id, Params& params, ComponentId_t rtr_id_, uint32_t num_rtr_ports, uint32_t num_local_ports );

  /// MeshTopology: destructor
  ~MeshTopology() override = default;

  /// MeshTopology: lifecycle functions
  void init ( uint32_t phase ) final;

  /// Send init messages (e.g, network discovery, etc)
  MordredFlit* sendInitMessage() final;

  /// Handle init messages (e.g, network discovery, etc)
  void processInitMessage( Event* ev, size_t topo_port_num, uint32_t vn ) final;

private:
  SST::Output* output;

  ComponentId_t rtr_id;
  uint32_t rtr_port_count;
  uint32_t local_port_count;

  uint32_t init_state{0};

  // Mesh parameters
  int32_t xId;
  int32_t yId;
  int32_t xSize;
  int32_t ySize;
  uint32_t num_links;

  std::vector<int32_t> dir_topo_port_vec; // n,e,s,w order; content of -1 is unused, otherwise it's SimpleRtr.topo_port[]

  std::queue<MordredFlit*> init_out_queue;
  std::queue<std::tuple<Event*,size_t,uint32_t> > init_in_queue; // Event, port_num, vn

};

} // namespace SST::Mordred


#endif //MESHTOPOLOGY_H
