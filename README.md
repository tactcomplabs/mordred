# mordred - SST NoC Library

[Here](SST-UG2025-Mordred.pdf) is an introductory presentation on this library from the 2025 SST User's Group meeting.

## Building
### Requirements
CMake version >= 3.19<br>
SST and SST-Elements (especially merlin) version >= 15.0 installed and in the current `PATH`

### Build/install/test steps
```
mkdir build && cd build
cmake ..
make -j
make install
make test
```

## Tests/Tested Compatibility
The tests run by `make test` are in the `tests/` folder.

### Standard tests

| Test script | Topology | Notes |
|---|---|---|
| `mesh3x3_testnic.py` | 3×3 mesh | Baseline mesh test |
| `mesh3x3_untimed_broadcast.py` | 3×3 mesh | `send_untimed_broadcast=true`; exercises `MeshTopology::routeUntimedBroadcastPacket` |
| `mesh3x3_2local_testnic.py` | 3×3 mesh | 2 local ports per router (concentration=2) |
| `mesh3x3_single_flit.py` | 3×3 mesh | 1-flit request → 2-flit minimum packet (HEAD+TAIL, no body flits) |
| `mesh3x3_large_message.py` | 3×3 mesh | 16-flit packets; exercises body-flit loop and output-buffer backpressure |
| `torus5x5_2vc_testnic.py` | 5×5 2D torus | 2 VCs for deadlock avoidance |
| `torus5x5_2vc_untimed_broadcast.py` | 5×5 2D torus | `send_untimed_broadcast=true`; exercises `TorusTopo::sendBroadcast` wrap logic |
| `torus5x5_3vc_testnic.py` | 5×5 2D torus | 3 VCs; exercises `VcAllocRR` beyond the minimum deadlock-free configuration |
| `torus3D_3x3x3_2vc_testnic.py` | 3×3×3 3D torus | 2 VCs |
| `torus3D_3x3x3_2vc_untimed_broadcast.py` | 3×3×3 3D torus | `send_untimed_broadcast=true`; exercises `Torus3DTopo::sendBroadcast` |
| `flatbutterfly_k2n4_testnic.py` | k=2, n=4 butterfly | — |
| `ipdps25tutorial_demo7.py` | Mesh | `memHierarchy.MemNIC` integration |
| `mordred_memNICFour.py` | Mesh | `memHierarchy.MemNICFour` integration |
| `mordred_testBridge.py` | Mesh | `merlin.Bridge` integration |

### Tested compatibility
To replace a `merlin.hr_router` using a single-router topology, a single `mordred.mordred_router` can be used with a 1x1 mesh topology.  The links between the endpoints and the `merlin.hr_router` then become links between the endpoints and the local ports of `mordred.mordred_router`.

The `mordred.mordredNIC` subcomponent works in the subcomponent slots of `memHierarchy.MemNIC` and `memHierarchy.MemNICFour`.  See `tests/ipdps25tutorial_demo7.py` and `tests/mordred_memNICFour.py` respectively.

The `mordred.mordredNIC` subcomponent also works in the `networkIF` subcomponent slot of the `merlin.Bridge` component; see `tests/mordred_testBridge.py`.

### PhysChannel tests (require prydwen)
When `MORDRED_ENABLE_PHYS_CHANNEL=ON` (the default), `make test` also registers a set of tests that exercise the `prydwen.uciePhysChannel` transport on every link:

| Test script | Topology | Transport |
|---|---|---|
| `mesh2x1_rtrportcontrolpc.py` | 2×1 mesh | `prydwen.genericPhysChannel` |
| `mesh2x1_uciePhysChannel.py` | 2×1 mesh | `prydwen.uciePhysChannel` |
| `mesh3x3_uciePhysChannel.py` | 3×3 mesh | `prydwen.uciePhysChannel` |
| `flatbutterfly_k2n4_uciePhysChannel.py` | k=2, n=4 butterfly | `prydwen.uciePhysChannel` |
| `torus5x5_2vc_uciePhysChannel.py` | 5×5 2D torus | `prydwen.uciePhysChannel`, 2 VCs |
| `torus5x5_3vc_uciePhysChannel.py` | 5×5 2D torus | `prydwen.uciePhysChannel`, 3 VCs |
| `torus5x5_4vc_uciePhysChannel.py` | 5×5 2D torus | `prydwen.uciePhysChannel`, 4 VCs |
| `torus3D_3x3x3_2vc_uciePhysChannel.py` | 3×3×3 3D torus | `prydwen.uciePhysChannel`, 2 VCs |
| `mesh2x1_uciePhysChannel_flit_format2.py` | 2×1 mesh | `prydwen.uciePhysChannel`, FLIT format 2 (68B wire, 64B payload) |
| `mesh3x3_uciePhysChannel_2module.py` | 3×3 mesh | `prydwen.uciePhysChannel`, 2 bonded modules |
| `mesh3x3_uciePhysChannel_2stack.py` | 3×3 mesh | `prydwen.uciePhysChannel`, 2 UCIe stacks; uses `mordred.mordredTestEP` |

These tests require prydwen to be built and registered first.  Build with `-DMORDRED_ENABLE_PHYS_CHANNEL=OFF` to skip them when prydwen is not available.

### Comments on the `repotest` folder
This folder is a sandbox for scipts/tests under development, performance comparisons, etc.  Feel free to use anything in here, but no promises are made as to the completeness and correctness of any script.  Only a couple of unique scripts exist - most are copy/edit from one of the original ones.

In an early development stage, Mordred had a component named `mordred.test_ep` as a standin for `merlin.test_nic`; while the `test_ep` component has been removed/replaced by `test_nic`, some scripts may fail as not every script has been retested.

Numerous scripts in this folder also use a component named `merlin.clocked_offered_load` - this was a local component (not upstreamed) based on `merlin.offered_load` to test using a clock rate to generate traffic (rather than a bandwidth parameter).  The behavior between the `clocked_offered_load` component and `merlin.offered_load` was equivalent when setting the `link_bw` parameter to match the link bandwidth of the Mordred network.

### Comments on the `sst_test_framework` folder
This folder contains a collection of files that would be useful for executing the tests via the standard SST elements test framework.

## Usage/Assumptions/Etc
Endpoints are expected to be connected to the local ports of the router; do not connect endpoints to the normal "routing" ports (for example, if doing a mesh, endpoints should be connected to port 4 or higher).

All routers should have the same number of local ports to ensure proper endpoint numbering.  Unconnected local ports are allowed.

### Mesh/2D Torus Topology
For these topologies, $x = 0, y = 0$ is the bottom left corner of the network.  The router ID is calculated as $(y * xDim) + x$.  Router IDs are expected to increase linearly following this equation.

### 3D Torus Topology
This topology is an extension of the mesh/2D torus topology. The router ID is calculated as $(z * (xDim * yDim)) + (y * xDim) + x$. Router IDs are expected to increase linearly following this equation.

### Flattened Butterfly Topology
The FlattenedButterfly class in tests/flatbutterfly_k2n4_testnic.py will handle the naming and numbering of routers and endpoints.

## Notes on VN,VC
In Merlin, the topology is what defines the number of VCs per VN - so this is a factor of the topology, not the router. Within the router, they sum the number of VCs across the VNs and use this value (num_vcs) when allocating data structures, etc.

Here, most data structures are multi-dimensional arrays contained within a port (or within a per-port object) where one dimension is the number of VNs and another dimension is the number of VCs.

## Random commentary
- Assuming 1 flit traverses the link at a time; see the channel_width branch for some initial support that modifies this (this branch is likely out of date)
- Priority is completely unimplemented (may need to use VNs since SST::SimpleNetwork::Request does not have a priority field)
- Additional topologies and arbitration methods can be added
- Router latency is fixed
- No maximum packet length (number of flits) set; packet to flit translation is happening only in MordredNIC and there is a minimum of 2 flits per packet
- When using `prydwen.uciePhysChannel` as the transport, `MordredNicPC::setup()` validates
  that the router's `flit_size` parameter matches the UCIe channel's flit payload size
  (via `PhysChannelAPI::getFlitPayloadBytes()`).  A mismatch aborts simulation at startup.
- Continue to review timing of the router and its subcomponents
- NetworkInspectors are not yet supported.
- See comments towards top of MordredEvents.h for a description of the event types
- The current design maintains a buffer on the output of router ports (currently have a small one per VN and VC)
  - Do we want to have a configurable arbitration for which VN,VC gets access? Currently designed as round-robin
  - In merlin, there is an OutputArbitration API class that is a member of the PortInterface (see comments in RtrPortControlAPI.h)
- Buffers are all individualized per VN,VC - no sharing of buffer space

## Basic Software Architecture/Router Behavior
The router owns a vector called perPortSharedObjs (one element per port) where each element is a RtrOwnedSharedObjs (in MordredEvents).
The RtrOwnedSharedObjs contains a pair of 2D vectors: needVcAlloc and needSwitchAlloc.

On a clock tick, the RtrPortControl will inspect the state of each VN, VC pair it owns.  If the flit in that pair needs an output VC
the needVcAlloc for that VN, VC pair is marked.  The VC allocator can then identify and operate (however it would like) on any/all
of the VN, VC pairs that need a VC allocation.  When the VC allocator has given an output VC for a given VN,VC pair, that entry in 
needVcAlloc is cleared.  This allows for persistent requests across clock cycles.  Additionally, since the VC allocator will know
all the packets that are ready for an allocation, it can operate at whatever level it desires (across ports, vns, vcs, etc).

We do a similar thing for the flits that are in need of switch allocation.

Currently, the MordredRouter performs a switch allocation on a per packet basis however, it should be able to handle doing allocations
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
