# Mordred - SST Network-on-Chip Library

Mordred is a modular, packet-switched Network-on-Chip (NoC) simulation library for the
[Structural Simulation Toolkit (SST)](http://sst-simulator.org/). It provides a router,
endpoint NIC, and topology implementations as drop-in alternatives to Merlin, with a
pluggable physical-channel layer that allows the transport between routers and endpoints
to be swapped independently of the routing and flow-control machinery.

## Requirements

- CMake >= 3.19
- SST and SST-Elements (including Merlin) >= 15.0
- C++17 compiler
- `sst` and `sst-config` must be on `PATH`
- **Prydwen** must be built and registered before building Mordred (Mordred's build
  system references `prydwen/src/` for the `PhysChannelAPI` header)

## Building

Build prydwen first, then mordred:

```bash
# 1. Build prydwen (physical channel library)
cd ../prydwen
mkdir build && cd build
cmake ..
make -j
make install

# 2. Build mordred
cd ../../mordred
mkdir build && cd build
cmake ..
make -j
make install
make test
```

## Repository Layout

```
src/             Main library source
tests/           SST Python test scripts
repotest/        Sandbox scripts for experiments and performance comparisons
merlin_testing/  Scripts comparing Mordred and Merlin behavior
sst_test_framework/  Files for running tests via the standard SST test framework
```

---

## Components

### Router: `mordred.mordred_router`

The top-level SST component representing one router in the network. It owns the topology,
per-port control subcomponents, a VC allocator, and a crossbar arbiter.

**Parameters**

| Parameter        | Description                                        | Default |
|------------------|----------------------------------------------------|---------|
| `id`             | Router ID (must follow topology numbering)         | —       |
| `clock`          | Clock frequency (e.g. `"1GHz"`)                   | —       |
| `num_ports`      | Total number of ports (routing + local combined)   | —       |
| `num_local_ports`| Number of ports connected to endpoints             | —       |
| `num_vns`        | Number of virtual networks                         | —       |
| `num_vcs`        | Virtual channels per VN                            | —       |
| `flit_size`      | Flit width (e.g. `"16b"`, `"64b"`)                | —       |
| `input_buf_size` | Per-VC input buffer size (e.g. `"8"` flits)        | —       |
| `output_buf_size`| Per-VC output buffer size                          | —       |
| `verbose`        | Verbosity level                                    | `0`     |

**Subcomponent slots**

| Slot           | API                       | Required |
|----------------|---------------------------|----------|
| `topology`     | `TopologyAPI`             | Yes      |
| `portcontrol*` | `RtrPortControlAPI`       | Yes      |
| `vcalloc`      | `VcAllocAPI`              | Yes      |
| `xbarArb`      | `XbarArbAPI`              | Yes      |

---

### Endpoint NIC: `mordred.mordredNIC`

An `SST::Interfaces::SimpleNetwork` subcomponent that connects an endpoint (e.g.
`merlin.test_nic`, `memHierarchy.MemNIC`) to a Mordred router over a raw SST link.
Handles packet-to-flit fragmentation, credit-based flow control, and reassembly.

Minimum packet size is 2 flits (one HEAD flit carrying the destination and a TAIL flit
carrying the payload).

**Parameters**

| Parameter        | Description                                              |
|------------------|----------------------------------------------------------|
| `clock`          | Interface clock frequency                                |
| `input_buf_size` | Input buffer size in bits/bytes (SI prefix supported)    |
| `output_buf_size`| Output buffer size                                       |
| `verbose`        | Verbosity level                                          |

**Statistics:** `packets_recv`, `average_noc_latency`, `average_packet_size`

---

### Endpoint NIC (PhysChannel-backed): `mordred.mordredNicPC`

Identical external interface to `mordredNIC` but uses an inner `PhysChannelAPI`
subcomponent (slot `port_iface`) instead of a raw SST link. This makes the physical
transport between the NIC and router pluggable — swap in `prydwen.genericPhysChannel`
for a simple raw-link transport, or `prydwen.uciePhysChannel` (from the prydwen repo)
for a full UCIe adapter-layer transport.

**Additional subcomponent slot**

| Slot         | API              | Description                                     |
|--------------|------------------|-------------------------------------------------|
| `port_iface` | `PhysChannelAPI` | Physical channel to the router (e.g. `prydwen.genericPhysChannel`) |

---

### Port Control: `mordred.rtrPortControl`

Per-port subcomponent of `mordred_router`. Manages the raw SST link to the attached
router or endpoint, input/output VC state machines, credit tracking, and flit
buffering. Coordinates with the VC allocator and crossbar arbiter through shared
per-router data structures.

---

### Port Control (PhysChannel-backed): `mordred.rtrPortControlPC`

Same as `rtrPortControl` but uses a `PhysChannelAPI` subcomponent (slot `port_iface`)
for the physical link, matching `mordredNicPC` on the endpoint side.

---

### Physical Channel API: `SST::Mordred::PhysChannelAPI`

> **Moved to the [prydwen](../prydwen) repo.** Concrete implementations and the
> abstract API now live in `prydwen/src/`. Mordred references `prydwen/src/` at
> build time via `PRYDWEN_SRC_DIR` in `src/CMakeLists.txt`.

Available implementations (registered in the prydwen element library):

| SST name                   | Library  | Description                                              |
|----------------------------|----------|----------------------------------------------------------|
| `prydwen.genericPhysChannel` | prydwen | Raw `SST::Link` transport via `PhysChannelLinkEvent`     |
| `prydwen.uciePhysChannel`   | prydwen | Full UCIe adapter-layer with FLIT serialization + credits |

---

## Topologies

All topologies are subcomponents registered under the `TopologyAPI`. Endpoints must be
connected to **local ports only** — do not connect endpoints to routing ports.

All routers in a simulation should have the same number of local ports. Unconnected
local ports are permitted.

### 2D Mesh — `mordred.meshTopology`

Rectangular grid without wrap-around.

Router ID = `(y × xDim) + x`, where `(x=0, y=0)` is the bottom-left corner.

**Parameters:** `xDim`, `yDim`, `num_local_ports`, `num_vns`, `num_vcs`

### 2D Torus — `mordred.torusTopo`

Mesh with wrap-around in both dimensions. Requires at least 2 VCs per VN to avoid
deadlock on larger networks.

Router ID = `(y × xDim) + x`

**Parameters:** `xDim`, `yDim`, `num_local_ports`, `num_vns`, `num_vcs`

### 3D Torus — `mordred.torus3dTopo`

Extension of the 2D torus with a Z dimension.

Router ID = `(z × (xDim × yDim)) + (y × xDim) + x`

**Parameters:** `xDim`, `yDim`, `zDim`, `num_local_ports`, `num_vns`, `num_vcs`

### Flattened Butterfly — `mordred.flatButterflyTopo`

K-ary N-fly topology. A helper class in `tests/flatbutterfly_k2n4_testnic.py` handles
router and endpoint naming/numbering.

**Parameters:** `k`, `n`, `num_local_ports`, `num_vns`, `num_vcs`

---

## Allocators and Arbiters

| Component              | SST name                  | Description                                   |
|------------------------|---------------------------|-----------------------------------------------|
| `VcAllocRR`            | `mordred.vcAllocRR`       | Round-robin virtual channel allocator         |
| `XbarArbRR`            | `mordred.xbarArbRR`       | Round-robin crossbar arbitration              |

---

## Flow Control and Initialization

Credits are exchanged during SST's `init()` phases before simulation begins. One credit
represents one flit of buffer space. The initialization handshake runs strictly between
each `MordredNIC` (or `MordredNicPC`) and the `RtrPortControl` (or `RtrPortControlPC`)
of the directly connected router port; no global broadcast occurs.

| Phase | MordredNIC                              | RtrPortControl                                                   |
|-------|-----------------------------------------|------------------------------------------------------------------|
| 0     | Send: report endpoint                   | Send: report router, router ID, port number                      |
| 1     | Recv: router ID and port number         | Recv: connection type; if endpoint, send num VNs, num VCs, flit width |
| 2     | Recv: num VNs, num VCs, flit width      | If endpoint: send endpoint ID                                    |
| 3     | Recv: endpoint ID                       | Idle                                                             |
| 4     | Exchange credits                        | Exchange credits                                                 |
| 5+    | Receive credits; enqueue other events   | Receive credits; enqueue other events                            |

---

## Compatibility with Merlin

`mordred.mordredNIC` fills the `SST::Interfaces::SimpleNetwork` subcomponent slot, so
it works with any component that accepts a `SimpleNetwork` interface:

- `merlin.test_nic` — general-purpose test endpoint
- `memHierarchy.MemNIC` — see `tests/ipdps25tutorial_demo7.py`
- `memHierarchy.MemNICFour` — see `tests/mordred_memNICFour.py`
- `merlin.Bridge` — see `tests/mordred_testBridge.py`

To replace a `merlin.hr_router` with a single-router deployment, use a 1×1 mesh
topology. Links that previously connected to `hr_router` become links to the local
ports of `mordred.mordred_router`.

---

## Tests

Tests live in `tests/` and are run via `make test` from the build directory.

**Standard tests** (always registered):

| Test script                                  | Topology           | Notes                                                                  |
|----------------------------------------------|--------------------|------------------------------------------------------------------------|
| `mesh3x3_testnic.py`                         | 3×3 mesh           | Baseline: 1 VC, 1 VN, 16-bit flits, 1 local port per router           |
| `mesh3x3_untimed_broadcast.py`               | 3×3 mesh           | `send_untimed_broadcast=true`; exercises `MeshTopology::routeUntimedBroadcastPacket` |
| `mesh3x3_2local_testnic.py`                  | 3×3 mesh           | Concentration=2 (2 local ports per router); exercises non-zero `dest - endptZeroId` final-hop offset |
| `mesh3x3_single_flit.py`                     | 3×3 mesh           | 1-flit request → 2-flit minimum packet; HEAD immediately followed by TAIL, empty body loop |
| `mesh3x3_large_message.py`                   | 3×3 mesh           | 16-flit packets; stresses body-flit send loop and output-buffer backpressure |
| `torus5x5_2vc_testnic.py`                    | 5×5 2D torus       | 2 VCs required for deadlock avoidance on 5×5                           |
| `torus5x5_2vc_untimed_broadcast.py`          | 5×5 2D torus       | `send_untimed_broadcast=true`; exercises `TorusTopo::sendBroadcast` wrap-aware flood logic |
| `torus5x5_3vc_testnic.py`                    | 5×5 2D torus       | 3 VCs; exercises `VcAllocRR` beyond the minimum deadlock-free VC count |
| `torus3D_3x3x3_2vc_testnic.py`              | 3×3×3 3D torus     | 2 VCs                                                                  |
| `torus3D_3x3x3_2vc_untimed_broadcast.py`    | 3×3×3 3D torus     | `send_untimed_broadcast=true`; exercises `Torus3DTopo::sendBroadcast` across 6 directions |
| `flatbutterfly_k2n4_testnic.py`             | k=2, n=4 butterfly | —                                                                      |
| `ipdps25tutorial_demo7.py`                  | Mesh               | `memHierarchy.MemNIC` integration                                      |
| `mordred_memNICFour.py`                     | Mesh               | `memHierarchy.MemNICFour` integration                                  |
| `mordred_testBridge.py`                     | Mesh               | `merlin.Bridge` integration                                            |

**PhysChannel tests** (registered only when `MORDRED_ENABLE_PHYS_CHANNEL=ON`; require prydwen):

| Test script                                  | Topology           | Transport                               |
|----------------------------------------------|--------------------|-----------------------------------------|
| `mesh2x1_rtrportcontrolpc.py`                | 2×1 mesh           | `prydwen.genericPhysChannel`            |
| `mesh2x1_uciePhysChannel.py`                 | 2×1 mesh           | `prydwen.uciePhysChannel`               |
| `mesh3x3_uciePhysChannel.py`                 | 3×3 mesh           | `prydwen.uciePhysChannel`               |
| `flatbutterfly_k2n4_uciePhysChannel.py`      | k=2, n=4 butterfly | `prydwen.uciePhysChannel`               |
| `torus5x5_2vc_uciePhysChannel.py`            | 5×5 2D torus       | `prydwen.uciePhysChannel`, 2 VCs        |
| `torus5x5_3vc_uciePhysChannel.py`            | 5×5 2D torus       | `prydwen.uciePhysChannel`, 3 VCs        |
| `torus5x5_4vc_uciePhysChannel.py`            | 5×5 2D torus       | `prydwen.uciePhysChannel`, 4 VCs        |
| `torus3D_3x3x3_2vc_uciePhysChannel.py`      | 3×3×3 3D torus     | `prydwen.uciePhysChannel`, 2 VCs        |

---

## Notes and Limitations

- **Priority:** Not implemented. Use VNs for traffic class separation (SST `SimpleNetwork::Request` has no priority field).
- **Router latency:** Fixed; no configurable pipeline depth.
- **Packet size:** No maximum enforced. Minimum is 2 flits (one HEAD + one TAIL), enforced at the NIC.
- **Channel width:** Only one flit traverses a link per clock cycle.
- **NetworkInspectors:** Not yet supported.
- **Buffer allocation:** Buffers are fully segregated per (VN, VC) pair; no shared pool.
- **Arbitration:** Round-robin only for both VC allocation and crossbar arbitration.
- **Routing:** Each topology implements its own routing; no general-purpose routing algorithm API.
- **Broadcast/Multicast:** Not supported at simulation time. Untimed broadcast (SST init/complete phases) is supported and tested via the `*_untimed_broadcast` test scripts — each topology implements `routeUntimedBroadcastPacket` with its own flood logic.

---

## Acknowledgments

This work was supported by the U.S. Department of Energy, Office of Science, Advanced
Scientific Computing Research program under project 84245 — Democratization of
Co-design for Energy-Efficient Heterogeneous Computing (DeCoDe) at Pacific Northwest
National Laboratory (PNNL). PNNL is a multi-program national laboratory operated for
the U.S. Department of Energy (DOE) by Battelle Memorial Institute under Contract No.
DE-AC05-76RL01830.
