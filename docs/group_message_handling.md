Group Message Handling
=====================

## Introduction

Group messages are a very important issue for MaidSafe routing to provide guarantees about node placement and therefore authority. For nodes to join a close group (as they must to join the network) they have their ID confirmed via a series of cryptographic checks on their ID. This is a critical component of the network and ensures that the network is secured at join. 

Sending messages from one group to another requires the source groups identity is certain and not forged. There are many mechanisms to achieve this, but many require several new messages to be sent to confirm signatures. This is not only complex but slow and wasteful of network traffic.

## Motivation

This document outlines a method of guaranteeing group identification and does so in a simple, but very efficient method that requires no messages to confirm identities. This prevents man in the middle forgeries of any group messages. Even though these are a difficult attack, they are very dangerous and should be made impossible. The motivation here is to make such attacks impossible. 

At the same time this document addresses this situation using a method that requires very little code change and will improve network efficiency by a factor of circa 30 times. 

## Overview

This proposal will make use of N+P sharing and requires routing to make use of [Information Dispersal](https://github.com/maidsafe/MaidSafe-Common/blob/next/include/maidsafe/common/crypto.h#L231) where P (threshold) and N (number of shares) relate to majority and group size respectively. Nfs was consider the location for including this mechanism, but it appears routing is the logical place. When data is sent to the network is passes through ```IDA``` to create N parts. Each part is sent to a different close node. The close nodes then syncronise the parts until P are recieved. When P are recieved then this is considered syncronised and each node sends the part they were sent so the next group. The message_id identifies the message and each part is added until there are P parts delivered.

However, an important part is required, either this group has to send their ID packet with the nessage (instead of source address) or they send a notification of delivery to be picked up by the remote group. Calculating the distance a group should be in, is relatively simple (with a decent error level) and therefor the former option seems to be the most efficient, as long as the message (including message_id) is signed by the ID of each node in the group.  

## Implementation


