/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_
#define MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_

#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 3107
# error This API is not compatible with the installed library.\
  Please update the maidsafe_routing library.
#endif


/// Versioning information

#define MAIDSAFE_ROUTING_VERSION 3107

#if defined CMAKE_MAIDSAFE_ROUTING_VERSION &&\
            MAIDSAFE_ROUTING_VERSION != CMAKE_MAIDSAFE_ROUTING_VERSION
#  error The project version has changed.  Re-run CMake.
#endif

#include "maidsafe/common/version.h"
#include "maidsafe/transport/version.h"

#define THIS_NEEDS_MAIDSAFE_COMMON_VERSION 1100
#if MAIDSAFE_COMMON_VERSION < THIS_NEEDS_MAIDSAFE_COMMON_VERSION
#  error This API is not compatible with the installed library.\
    Please update the maidsafe-common library.
#elif MAIDSAFE_COMMON_VERSION > THIS_NEEDS_MAIDSAFE_COMMON_VERSION
#  error This API uses a newer version of the maidsafe-common library.\
    Please update this project.
#endif

#define THIS_NEEDS_MAIDSAFE_TRANSPORT_VERSION 104
#if MAIDSAFE_TRANSPORT_VERSION < THIS_NEEDS_MAIDSAFE_TRANSPORT_VERSION
#  error This API is not compatible with the installed library.\
    Please update the maidsafe-transport library.
#elif MAIDSAFE_TRANSPORT_VERSION > THIS_NEEDS_MAIDSAFE_TRANSPORT_VERSION
#  error This API uses a newer version of the maidsafe-transport library.\
    Please update this project.
#endif
/// end of versioning

#include "maidsafe/transport/transport.h"
#include "maidsafe/transport/message_handler.h"
#include "maidsafe/transport/tcp_transport.h"
#include "maidsafe/transport/udp_transport.h"


/// The size of ROUTING keys and node IDs in bytes.
const int16_t kKeySizeBytes(64);

/// Size of closest nodes group
const int16_t kClosestNodes(8);

/// total nodes in routing table
const int16_t kRoutingTableSize(64);

static_assert(kClosestNodes >= kRoutingTableSize); 

typedef std::function<void()> FindValueFunctor;

void Start();
void Stop();

/// must be set before node can start  This allows node to 
/// check locally for data that's marked cacheable. 
void SetGetDataFunctor(FindValueFunctor &find_value);

/// To create and send messages through network - may be best as object but less 
/// portable (to c and c++ I think)
/// Setters
void CreateMessage(const char &message, int64_t size, int64_t &message_id);
void setMessageDestination(const char &id, int32_t size, int64_t &message_id);
void setMessageCacheable(bool cache, int64_t message_id);
void setMessageLock(bool lock, int64_t &message_id);
void setMessageSignature(const char &signature, int16_t size, int64_t &message_id);
void setMessageSignatureId(const char &signature_id, int16_t size, int64_t &message_id);
void setMessageDirect(bool direct);

/// Getters
char* MessageDestination(int64_t &message_id);
bool MessageCacheable(int64_t &message_id);
bool MessageLock(int64_t &message_id);
char* MessageSignature(int64_t &message_id);
char* MessageSignatureId(int64_t &message_id);
bool MessageDirect(int64_t &message_id);
bool SendMessage(int64_t &message_id);
/// to recieve messages
bool RegisterMessageHandler(); ///needs some thought on how best to do this cleanly

bool Running();

#endif  // MAIDSAFE_ROUTING_MAIDSAFE_ROUTING_H_
