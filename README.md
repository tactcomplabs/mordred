# mordred - SST NoC Library

## Building
### Requirements
CMake version >= 3.19<br>
SST and SST-Elements (especially merlin) version >= 15.0 installed and in the current `PATH`

### Build steps
```
mkdir build && cd build
cmake ..
make -j
make install
```

## Installing/Running tests
The basic testsuite, found in the `tests/` folder, has been implemented using the standard SST testsuite approach documented [here](https://sst-simulator.org/sst-docs/docs/guides/dev/testframework).

The testsuite is not registered with SST by default when building the library. Add the following flag to your cmake command to do so: `-DINSTALL_TESTS=ON`.

To run the testsuite after it has been registered with SST, run: `sst-test-elements -w "*mordred*"`

Alternatively, you can use the test suite locally using `ctest` as follows:
```
make test
```

## Usage/Assumptions/Etc
Endpoints are expected to be connected to the local ports of the router; do not connect endpoints to the normal "routing" ports (for example, if doing a mesh, endpoints should be connected to port 4 or higher).

All routers should have the same number of local ports to ensure proper endpoint numbering.  Unconnected local ports are allowed.

### Mesh/2D Torus Topology
For these topologies, $x = 0, y = 0$ is the bottom left corner of the network.  The router ID is calculated as $(y * xDim) + x$.  Router IDs are expected to increase linearly following this equation.

### 3D Torus Topology
This topology is an extension of the mesh/2D torus topology. The router ID is calculated as $(z * (xDim * yDim)) + (y * xDim) + x$. Router IDs are expected to increase linearly following this equation.

### Flattened Butterfly Topology
The FlattenedButterfly class in tests/flatbutterfly_k2n4_testnic.py will handle the naming and numbering of routers and endpoints.

## Tested Compatibility
To replace a merlin.hr_router using a singlerouter topology, a single mordred.simple_rtr can be used with a 1x1 mesh topology.  The links between the endpoints and the merlin.hr_router then become links between the endpoints and the local ports of mordred.simple_rtr

The mordredNIC subcomponent has been tested as the subcomponent(s) in both memHierarchy.MemNIC and memHierarchy.MemNICFour and found to operate correctly.  An example of the former is repotest/sst_ipdps2025tutorial_demo7.py and for the latter in repotest/memH/testKingsley_Mordred.py - this test was originally in sst-elements/memHierarchy/)

The mordredNIC subcomponent has also been tested as a subcomponent "networkIF" in the merlin.Bridge component; see repotest/memH/testBridge_Mordred.py - original copied from sst-elements/memHierarchy/testBridge.py which is alongside testBridge_Mordred.py)

## Random thoughts/questions/discussion
- See comments towards top of MordredEvents.h for a description of the event types
- The current design maintains a buffer on the output of router ports (currently have a small one per VN and VC)
  - Do we want to have a configurable arbitration for which VN,VC gets access? Currently designed as round-robin
  - In merlin, there is an OutputArbitration API class that is a member of the PortInterface (see comments in RtrPortControlAPI.h) 
- Buffers are all individualized per VN,VC - no sharing of buffer space
- NetworkInspectors are not yet supported.

## Notes on VN,VC
In Merlin, the topology is what defines the number of VCs per VN - so this is a factor of the topology, not the router. Within the router, they sum the number of VCs across the VNs and use this value (num_vcs) when allocating data structures, etc.

Here, most data structures are multi-dimensional arrays contained within a port (or within a per-port object) where one dimension is the number of VNs and another dimension is the number of VCs.

## Random open items/TODOs
- Assuming 1 flit traverses the link at a time; see the channel_width branch for some initial support that modifies this (this branch is likely out of date)
- Priority is completely unimplemented (may need to use VNs since SST::SimpleNetwork::Request does not have a priority field)
- Additional topologies and arbitration methods can be added
- Router latency is fixed
- No maximum packet length (number of flits) set; packet to flit translation is happening only in MordredNIC and there is a minimum of 2 flits per packet
- Continue to review timing of the router and its subcomponents

## Basic Software Architecture/Router Behavior
The router owns a vector called perPortSharedObjs (one element per port) where each element is a RtrOwnedSharedObjs (in MordredEvents).
The RtrOwnedSharedObjs contains a pair of 2D vectors: needVcAlloc and needSwitchAlloc.

On a clock tick, the RtrPortControl will inspect the state of each VN, VC pair it owns.  If the flit in that pair needs an output VC
the needVcAlloc for that VN, VC pair is marked.  The VC allocator can then identify and operate (however it would like) on any/all
of the VN, VC pairs that need a VC allocation.  When the VC allocator has given an output VC for a given VN,VC pair, that entry in 
needVcAlloc is cleared.  This allows for persistent requests across clock cycles.  Additionally, since the VC allocator will know
all the packets that are ready for an allocation, it can operate at whatever level it desires (across ports, vns, vcs, etc).

We do a similar thing for the flits that are in need of switch allocation.

Currently, the SimpleRtr performs a switch allocation on a per packet basis however, it should be able to handle doing allocations
on a per clock tick basis (this should be tested).

## Notes on the initialization process

Currently, the initialization procedure does not send any information "globally" to all routers/endpoints; the
initialization is strictly done between the endpoint NIC (MordredNIC) and the port control of the router (RtrPortControl
is the only one implemented).

Note to self: If there are no messages during a phase of init(), the init() process ends.

The table below outlines the current initialization process. The (s) notes a send, (r) notes a receive.

| Phase | MordredNIC                                        | RtrPortControl                                                                                                                                                          |
|-------|---------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0     | (s) Report Endpoint                               | - (s) Report Router <br> - (s) Router ID <br> - (s) Port Number                                                                                                         |
| 1     | - (r) Router report <br> - (r) Router ID <br> - (r) Port Number | (r) Connection type <br> - If Router <br>   - (r) Router ID <br>   - (r) Port Number <br> - Else (endpoint) <br> - (s) Num VNs <br> - (s) Num VCs <br> - (s) Flit Width |
| 2     | - (r) Num VNs <br> - (r) Num VCs <br> - (r) Flit Width <br> | If connection_type = Endpoint <br> - (s) Endpoint ID                                                                                                                    |
| 3     | (r) Endpoint ID                                   | IDLE (held for channel width setup if needed in the future)                                                                                                             |
| 4     | Send credits                                      | Send credits                                                                                                                                                            |
| 5+    | Receive Credits; enqueue anything else            | Receive Credits; enqueue anything else                                                                                                                                  |

## Acknowledgments

This work was supported by the U.S. Department of Energy, Office of Science, Advanced Scientific Computing Research program under project 84245—Democratization of Co-design for Energy-Efficient Heterogeneous Computing (DeCoDe) at Pacific Northwest National Laboratory (PNNL).  PNNL is a multi-program national laboratory operated for the U.S. Department of Energy (DOE) by Battelle Memorial Institute under Contract No. DE-AC05-76RL01830.
