Group Message Handling
=====================

## Introduction

Group messages are a very important issue for MaidSafe routing to provide gurantees about node placement and therefor authority. For nodes to join a close group (as they must to join the network) they have their ID confirmed via a series of cryptographic checks on their ID. This is a critical component of the network and ensures that the network is secured at join. 

Sending messages from one group to another requires the source groups identity is certain and not forged. There are many mechanisms to achieve this, but many require several new messages to be sent to confirm signatures. This is not only complex but slow and wasteful of network traffic.

## Motivation

This document outlines a method of guranteeing group identification and does so in a simple, but very efficient method that requires no messages to confirm identities. This prevents man in the middle forgeries of any group messages. Even though these are a difficult attack, they are very dangerous and should be made impossible. The motivation here is to make such attacks impossible. 

At the same time this document addresses this situation using a method that requires very little code change and will improve netork efficiency by a factor of circa 30 times. 

## Overview

This proposal will make use of N+P sharing and requires routing to make use of [secret shared data](https://github.com/maidsafe/MaidSafe-Common/blob/next/include/maidsafe/common/crypto.h#L231) 

## Implementation
