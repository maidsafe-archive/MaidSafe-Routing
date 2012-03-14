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

#include "maidsafe/routing/version.h"

#if MAIDSAFE_ROUTING_VERSION != 100
#  error This API is not compatible with the installed library.\
    Please update the maidsafe-routing library.
#endif


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
  kMarkedForDeletion = -301004,

  // RoutingTable
  kOwnIdNotIncludable = -302001,
  kFailedToUpdateLastSeenTime = -302002,
  kNotInBrotherBucket = -302003,
  kOutwithClosest = -302004,
  kFailedToInsertNewContact = -302005,
  kFailedToFindContact = -302006,
  kFailedToSetPublicKey = -302007,
  kFailedToUpdateRankInfo = -302008,
  kFailedToSetPreferredEndpoint = -302009,
  kFailedToIncrementFailedRpcCount = -302010,

  // Node
  kNoOnlineBootstrapContacts = -303001,
  kInvalidBootstrapContacts = -303002,
  kNotListening = -303003,
  kNotJoined = -303004,
  kFindNodesFailed = -303005,
  kFoundTooFewNodes = -303006,
  kStoreTooFewNodes = -303007,
  kDeleteTooFewNodes = -303008,
  kUpdateTooFewNodes = -303009,
  kFailedToGetContact = -303010,
  kFailedToFindValue = 303011,  // value intentionally positive
  kFoundAlternativeStoreHolder = 303012,  // value intentionally positive
  kIterativeLookupFailed = -303013,
  kContactFailedToRespond = -303014,
  kValueAlreadyExists = -303015,
  kFailedValidation = -303016
};

}  // namespace routing
}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_RETURN_CODES_H_
