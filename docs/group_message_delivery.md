# Group Message Delivery

### Introduction

The Routing layer is responsible for reliable message delivery.  This is achieved through a recursive mechanism where the message is passed to ever-closer nodes until the destination has been reached.  This document only covers the final delivery of a message which is destined for a close group of nodes rather than a single one.


### Motivation

At the time of writing, the Routing layer handles final delivery through a "group leader" mechanism.  The notion of a group leader is that the closest node (in XOR terms) of a group to a given address is the leader.  All messages destined for that address are deliberately channelled through this node.  This was done to try and ensure that all messages reached the correct group of nodes; the rationale being that the group leader should have the best knowledge of that part of the address space and there would be little or no scope for error.

However, this presents a weakness in that there is a single point of failure, which could be exploited by a malicious user, and is also creating a deliberate bottleneck in the message delivery path.


### Overview

The new approach will avoid the notion of a group leader.  In essence, all nodes which are close to the target will circulate the message amongst themselves.

### Implementation

Every node will follow the below procedure for all group messages.  It should usually result in a group message being sent via single nodes for the first few hops, but as it gets close to the destination it will spread out in parallel to nodes close to the target.

For a node with ID `X` which has received a group message destined for address `A`, the protocol is:

- [`nth_element`](http://en.cppreference.com/w/cpp/algorithm/nth_element) sort the routing table by closesness to `X` where n<sup>th</sup> element index is 15
- if `A` is closer to `X` than the 16<sup>th</sup> element (index 15 from above), then
  - [`partial_sort`](http://en.cppreference.com/w/cpp/algorithm/partial_sort) the routing table by closeness to `A` for the four closest
  - send the message to these four closest to `A`
- else
  - `nth_element` sort the routing table by closesness to `A` where n<sup>th</sup> element index is 0
  - send the message to this single node closest to `A`

Nodes will need to keep a record of received and handled messages for a short duration so that messages aren't re-broadcast amongst the close group endlessly.
