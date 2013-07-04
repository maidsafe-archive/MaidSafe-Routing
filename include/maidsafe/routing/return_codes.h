/* Copyright 2012 MaidSafe.net limited

This MaidSafe Software is licensed under the MaidSafe.net Commercial License, version 1.0 or later,
and The General Public License (GPL), version 3. By contributing code to this project You agree to
the terms laid out in the MaidSafe Contributor Agreement, version 1.0, found in the root directory
of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also available at:

http://www.novinet.com/license

Unless required by applicable law or agreed to in writing, software distributed under the License is
distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
implied. See the License for the specific language governing permissions and limitations under the
License.
*/

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
