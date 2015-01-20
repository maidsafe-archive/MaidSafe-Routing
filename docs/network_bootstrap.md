# Network Bootstrap design document

Currently (jan 2015) the network is bootstrapped in a constructive manner,
i.e. first zero-state nodes connect and from this initial nugget new nodes
are instructed to bootstrap off.  With the local network controller (LNC) these initial nodes are all processes running on the same physical machine.

The proposal in this document consists of two parts.  First a natural
re-orderering of the sequence of events is proposed.  Secondly a security
improvement on the bootstrap procedure is discussed that additionally
protects concurrent versions of the network to run.

## Brief outline

The new bootstrapping design aims to make setting up a new network an effortless, natural flow of events.  To achieve this we change the order of events: first set up a large set of nodes ready to bootstrap off of the not-yet existing network.  These nodes will be given a bootstrap file, more details of which will be given later.  Only when all nodes are ready, will two nodes be instructed to initiate a zero-state bootstrap procedure.  From this point on the network can unfold itself. An analogy to keep in mind is of a wildfire spreading in a dry forest: there first needs to be a collection of ignitable trees in close contact to each other.  With that condition satisfied, all you need is a spark.

There are two important aspects to ensure that the network can grow without defects.  In metallurgy the process of annealing heats up a metal and lets it cool down slowly to increase the softness and make it less brittle.  The analogy to the SAFE network would be for a routing node to respond to a ConnectRequest with an 'I-am-not-ready-for-connecting' message (or simply not respond) until it has fully connected itself.
