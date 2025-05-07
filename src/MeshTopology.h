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
    "2D Mesh Topology for NoC Router",
    SST::Mordred::TopologyAPI
  )

  // register the parameters
  SST_ELI_DOCUMENT_PARAMS(
    { "verbose",       "Sets the output verbsoity",                    "5" },
    { "xDim", "Number of points in the X dimension", "1"},
     {"yDim", "Number of points in the Y dimension", "1"}
  )

  // register the ports
  SST_ELI_DOCUMENT_PORTS()

  // register the subcomponent slots
  SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS()

  /// MeshTopology: constructor
  MeshTopology( ComponentId_t id, Params& params, uint32_t rtr_id, uint32_t num_ports, uint32_t num_local_ports );

  /// MeshTopology: destructor
  ~MeshTopology() override = default;

  PortConnectionE getPortConnection( uint32_t portnum ) final;

  int32_t getEndpointId( uint32_t portnum ) override;


  // TODO: Probably don't need these next two functions
  /// Send init messages (e.g, network discovery, etc)
  //MordredFlit* sendInitMessage() final;

  /// Handle init messages (e.g, network discovery, etc)
  //void processInitMessage( Event* ev, size_t topo_port_num, uint32_t vn ) final;

private:
  SST::Output* output;

  uint32_t rtrId;
  uint32_t numPorts;
  uint32_t numLocalPorts;

  //uint32_t init_state{0};

  // Mesh parameters
  uint32_t xId{UINT32_MAX};
  uint32_t yId{UINT32_MAX};
  uint32_t xDim{UINT32_MAX};
  uint32_t yDim{UINT32_MAX};

  std::vector<int32_t> dir_topo_port_vec; // n,e,s,w order; content of -1 is unused, otherwise it's SimpleRtr.topo_port[]

  // TODO: Probably don't need the below data members
  //std::queue<MordredFlit*> init_out_queue;
  //std::queue<std::tuple<Event*,size_t,uint32_t> > init_in_queue; // Event, port_num, vn

};

} // namespace SST::Mordred


#endif //MESHTOPOLOGY_H
