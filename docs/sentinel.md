#Sentinel overview

## Quick intro to network consensus, authority and crypto usage.

In a decentralised autonomous network there are many challenges to face. One such challenge is the range of attacks that consist of sybil / sparticus and forgery attacks (did the message really come from who you think). One of the simplest attack to foil is the forgery attack, thanks to asymmetric cryptography. This allows for a public key to be known by everyone and when anythign is encrypted with this key it can only (supposedly) be decrypted by the private key of that keypair. Assuming a reasonable algorithm, keysize and implementtion this holds true. 

This also removes the Sparticus type attacks (claim to be another identity)., but not necessarily sybil attacks, where an attack on a bit of a network is ehough to persuade the rest of the network that any request is valid or indeed anything asked of that part of th enetwork is done as expected. To overcome this MAidSafe have several techniques used in parallel. These boil down to 

1. Have nodes create key chains (a chain of keys each signing the next intil one is selected). We call these Fobs. A Publicfob consists of a public_key & signature as well as a name field. The name is the SHA512HASH(public_key+signature) making copying a key crypto hard (we can confirm the signature is also signed by a valid key pair by checking the signature there, where this 'pure key' is self signed). The Fob type is this PublicFob + the provate key.
2: Ask network to store the PublicFob for a node. The network will accept this if the node has certain characteristics (based on rank - later discussion) and the key is less than 3 leading bits diffferent from the current group of nodes. This makes key placement distribute equally across the address range (as for rank consider only a single non ranked node allowed per group, and failure to increase rank means the key is deleted form the network adn has to be re-stored if possible). 

3. This now resembles a PKI network where to speak to node ABC you go get the PublicFob at ABC and either encrypt a message to the node or check a message from the node is signed by using that PublicFob.public_key. The difference being no central authority exists and the netwoerk distributes and collects keys as any DHT would (except in this case the DHT is secured by the very PKI it manages). So this is very secure and does nto require any hunman intervnetion (unlike a certificate authority). 

4. Assemble nodes into groups that will act in unison on any request/response. So these groups are selected to be large enough to ensure a sybil attack would require at least 3X network size of attackers to be able to join (a signle attacker with no other node types joining). The magic number here is 28, realistically this number is closer to 17. 

5. Allow a failure rate as failures will defnitely happen. This is done by having a GroupSize of say 32 and set the QuorumSize to 28. Thie means for any action we require 28 nodes close to a target address to agreee and carry out an action. 

This Quorum creates a mechnism where another group or node can belive the action is correct and valid. This is called group consensus. 

The group consesnus provides the network a way to request or carry out actions adn ensure such requests are valid and actions actually done. This is required as the network looks after itself (autonomous). 

A client has a close group and requires to persuade this group to request teh network take an action which Puts something on the network (a data elemenet/message etc.) Clients create data and messages, the network handles these. As the client cannot just connect to an arbitary group and demand soemthing be done, they connect to their close group and register themselves (with their Fob) an account. The close group can then be persuaded by the client to request another part of the network create soemthing (a Put). In the case of Maidsafe the close group request the client pay via safecoin (it used to be they paid with storage that another gorup managed an dagred). So the client persuades the close group to put data. (ignore how payment is made, it actually requires client send safecoin to a provable recycle part of the network (another group confirms this)).

So a client can sign request to the group (crypto secure) and the group can check validity of the request and then ask the appropriate group close to the address of the data or message to accept this request. 

After anything is put the client can mutate this thing (if they have signed it). This is the case for directory entries, wehre a client can add versions to a list (StructuredDataVersion) as it was Put signed by the client. So the owner of the signature can sign a request to alter this. This is crypto secured authority. 

In the case of groups requestying actions then we have group based consensus and the network grants authority based on a group that can be measured as a valid close group, wehre each memeber has signed a request as being from that close group. This authority is what the sentinel confirms prior to the routing object processing an action request. 
Almost all messages are Sentinel checked, with teh exception of get_group as this fetches Fob's which are self validating and it fetches a copy of all Fobs from all group members and confirms they agree and validate. 

##Sentinel components


