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

## Open Questions
- Do we pass in a packet to the MordredNIC and let it divide it into flits? Yes
- Do we want to add another SST::Event wrapper similar to what Merlin does? Hopefully, no
- Maintain a buffer on the output of router ports (currently have a small one per VC)
- VNs? Allow for the possibility - easy enough in the MordredNIC - need to consider in the router and its underlying components
- 