# mordred

SST NoC Component

See the mordred.info.txt file at the top level to see the current components and subcomponents.

### Notes on the initialization process

Currently, the initialization procedure does not send any information "globally" to all routers/endpoints; the
initialization is strictly done between the endpoint NIC (MordredNIC) and the port control of the router (RtrPortControl
is the only one implemented).

Note to self: If there are no messages during a phase of init(), the init() process ends.

The table below outlines the current initialization process. The (s) notes a send, (r) notes a receive.

| Phase | MordredNIC                                                                  | RtrPortControl                                                                                                                                                                               |
|-------|-----------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 0     | (s) Report Endpoint                                                         | - (s) Report Router <br> - (s) Router ID <br> - (s) Port Number                                                                                                                              |
| 1     | - (r) Router report <br> - (r) Router ID <br> - (r) Port Number             | (r) Connection type <br> - If Router <br>   - (r) Router ID <br>   - (r) Port Number <br> - Else (endpoint) <br> - (s) Num VNs <br> - (s) Num VCs <br> - (s) Flit Width <br> - (s) Bus Width |
| 2 | - (r) Num VNs <br> - (r) Num VCs <br> - (r) Flit Width <br> - (r) Bus Width | If connection_type = Endpoint <br> - (s) Endpoint ID                                                                                                                                         |
| 3 | (r) Endpoint ID <br> Send credits                                           | Send credits                                                                                                                                                                                 |
| 4+ | Receive Credits; discard anything else                                      | Receive Credits; discard anything else                                                                                                                                                       |

## Random thoughts/questions/discussion
- Do we want to add another SST::Event wrapper similar to what Merlin does? As of now, no.
  - See comments towards top of MordredEvents.h
- The current design maintains a buffer on the output of router ports (currently have a small one per VN and VC)
  - Do we want to have a configurable arbitration for which VN,VC gets access? Currently designed as round-robin
  - In merlin, there is an OutputArbitration API class that is a member of the PortInterface (see comments in RtrPortControlAPI.h) 
- Buffers are all individualized per VN,VC - no sharing of buffer space

## Notes on VN,VC
The topology is what defines the number of VCs per VN - so this is a factor of the topology, not of the router. Within the router, the sum the number of VCs across the VNs and use this value (num_vcs) when allocating data structures, etc.

Here, I've taken a different approach and created most data structures as being multi-dimensional arrays where one dimension is the number of VNs and another dimension is the number of VCs. Unfortunately then, there are some data structs that end up being three dimensions ([port][vn][vc])

## TODOs
- The channelBusWidth is unused at present. Assuming 1 flit traverses the link at a time
- Priority is completely unimplemented
- Additional topologies and arbitration methods can be added
- Router latency is fixed
- Arbitration isn't changing VN,VC (so there is no VC allocation/arbitration)
- No maximum packet length (number of flits) set; packet to flit translation is happening only in MordredNIC and there is a minimum of 2 flits per packet
- Need to review timing of the router and its subcomponents
- 