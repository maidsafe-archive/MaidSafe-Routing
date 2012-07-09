/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

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

  // Node
  kNoOnlineBootstrapContacts = -303001,
  kInvalidBootstrapContacts = -303002,
  kNotListening = -303003,
  kNotJoined = -303004,
  kResponseTimeout = -303005,
  kAnonymousSessionEnded = -303006,
  kInvalidDestinationId = -303007,
  kEmptyData = -303008,
  kTypeNotAllowed = -303009
};

}  // namespace routing
}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RETURN_CODES_H_
