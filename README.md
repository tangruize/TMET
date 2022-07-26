# Transparent Model-based Explorative Testing Framework

TMET is a framework for transparently controlling distributed systems and running test-cases generated by model checking.

There are mainly three steps to test a distributed system using TMET framework:

1. Abstract core protocols from code and formalize them into TLA+ specifications.
2. Run model checking to generate traces, replay traces in the real distributed systems using TMET, and compare states for amending the formal specification to improve model/code conformance.
3. Run model checking, try fixing bugs in model, validate the fixes and fix bugs in code.

## Modelling and Checking

We modelled two raft libraries: [WRaft](./wraft/) and [PySyncObj](./PySyncObjTLA/). Current modelling progress:

- WRaft: leader election, log replication, log compaction, safety properties, fault injection and heuristic state pruning.
- PySyncObj: leader election, fault injection and safety properties.

We find several bugs, some are confirmed, some are not:

- [WRaft#118](https://github.com/willemt/raft/pull/118): 9 issues, 4 of which are protocol bugs.
- [PySyncObj#161](https://github.com/bakwc/PySyncObj/pull/161): 1 network partition bug, has been merged.

## Controlling Distributed Systems

The biggest challenge in controlling distributed systems is the control of the environment, mainly including global network delivery, and local program non-determinism.

### Network Delivery

For global network delivery, we develop a [tproxy](./tproxy/) module that runs in the router node, intercepts the network traffic and masquerades as peers. Currently, `tproxy` supports TCP network sending, receiving, and partition failure.

The underlying technique of `tproxy` is the Linux Kernel's [transparent proxy support](https://www.kernel.org/doc/Documentation/networking/tproxy.txt).

### Hacking Program Non-determinism

The main non-determinism comes from system calls. We develop a [mysyscall](./mysyscall/) module to intercept Linux syscalls. The task of intercepting syscalls is to control of networking, timing and randomization.

The underlying technique of `mysyscall` is [LD_PRELOAD](https://man7.org/linux/man-pages/man8/ld.so.8.html).