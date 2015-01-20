#Bootstrap overview

##Zero state 

An rudp node will start by default and listen on the 'live' port (5483) for incomming connections. From routing perspective this node requires to store his own PublicPmid (which is a key fob containing public_key, signature (called verification block/token) and name. The SHA512HASH(public_key + signature) == name and this name is a network address (Network Addressable Entity NAE).

When node 2 starts it shoudl be provided with the zero state nodes endpoint (see boostrap method in routing_node) to connect to.  

This joining node will by default 

1. Do a PutData to store his PublicPmid on the closest nodes (this will be the only node on the network). 
2. Send a find_group message (with the relay field in the header set for the boostrap node who will return the message to the joining node) to get the PublicPmid of all nodes close to it. 
3. On reciept of a find_group message this node will then send connect requests to each node identified in the find_group.
4. On reciept of a connect_reesponse this node will attempt and connection_manager_SuggestNodetoAdd(Address) then rudp connection (rudp_.Add) and if successfull call connection_manager_.Add(Address, Public Pmid (NodeInofo struct)).

The boostrap nodes follows these rules

1: On reciept of a find_group - call connectin_manager_OurGroup() and send a find_group_reponse back to the joining node. 
2: On reciept of a connect_request this node checks with connection_manager_.SuggestNode(Address) and if true, returns a connect_response and starts attempting the connection via rudp (whic needs to keep trying for a period which is long enough for the response to be delivered and the joining node to also attempt to connect). 
3: On rudp_Add success this node adds the joining node to its routing table.

This allows two nodes to connect. 

Further nodes will follow the same pattern.

###Differences in zero state nodes

1: They cannot wait for QuorumSize messages as the network has not enough nodes to satisfy this request. So these nodes have a quorumSize set to 1 (see types.h) to allows them to act on a single reply (insecure but we are starting this seed network). 

###Items to consider

1. As the nodes are storing keys into cache as opposed to long term or perminent storage then each node needs to republish thier keys to this network with a frequency less than 10 minutes (default cache period). 
2. As the network grows beyong Quorumsize the keys stored in cache may not be cloe to the address they are meant to be. If the republich is frequent enough and in line wiht network startup speed then this balances out as each new republish will put the keys in the correct location. 
3. With a QuorumSize of 1 these nodes will likely see the same traffic as the seed network gets up to size (GroupSize). This is handled by the filter_ but can look inneficient, there may be ways to adjust quorumsize up as the entwork grows (your close group is an indication of how healthy or populated the network is).

##Existing network

As zero state ndoes have differences (limited to quorumsize for now) we want to add more nodes and kill off the seed nodes (they are producing too much traffic).  We add non zero state nodes to the network now using the same process, but without any changes to Qorumsize etc.). As we add these nodes we need to kill off the seed nodes, For testing the seed nodes will produce traffic, so are of limited (but not useless) value.
