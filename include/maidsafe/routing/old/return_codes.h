/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#ifndef MAIDSAFE_ROUTING_RETURN_CODES_H_
#define MAIDSAFE_ROUTING_RETURN_CODES_H_

namespace maidsafe {

namespace routing {

enum ReturnCode {
  // General
  kSuccess = 0,
  kGeneralError = -300001,
  kUndefined = -300002,
  kPendingResult = -300003,
  kInvalidPointer = -300004,
  kTimedOut = -300005,

  // DataStore
  kEmptyKey = -301001,
  kZeroTTL = -301002,
  kFailedToModifyKeyValue = -301003,

  // RoutingTable
  kOwnIdNotIncludable = -302001,
  kFailedToInsertNewContact = -302002,
  kFailedToFindContact = -302003,
  kFailedToSetPublicKey = -302004,
  kFailedToUpdateRankInfo = -302005,
  kFailedToSetPreferredEndpoint = -302006,
  kFailedToIncrementFailedRpcCount = -302007,
  kInvalidNodeId = -302008,

  // Node
  kNoOnlineBootstrapContacts = -303001,
  kInvalidBootstrapContacts = -303002,
  kNotListening = -303003,
  kNotJoined = -303004,
  kResponseTimeout = -303005,
  kResponseCancelled = -303006,
  kInvalidDestinationId = -303007,
  kEmptyData = -303008,
  kTypeNotAllowed = -303009,
  kFailedtoSendFindNode = -303010,
  kDataSizeNotAllowed = -303011,
  kFailedtoGetEndpoint = -303012,
  kPartialJoinSessionEnded = -303013,
  kNetworkShuttingDown = -303014
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RETURN_CODES_H_
