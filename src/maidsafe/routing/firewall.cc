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

#include "maidsafe/routing/firewall.h"

#include "maidsafe/routing/parameters.h"

namespace maidsafe {

namespace routing {

const int Firewall::kQueueSize_ = 1000;

Firewall::Firewall()
    : mutex_(), history_() {}

bool Firewall::Add(const NodeId& source_id, const uint32_t& message_id) {
  if (source_id.IsZero())
    return false;
  std::unique_lock<std::mutex> lock(mutex_);
  auto found(std::find_if(history_.begin(),
                          history_.end(),
                          [&](const ProcessedMessage& processed_message)->bool {
                            return ((std::get<0>(processed_message) == source_id) &&
                                    (std::get<1>(processed_message) == message_id));
                          }));

  if (found != std::end(history_))
    return false;

  history_.push_back(std::make_tuple(source_id, message_id, std::time(NULL)));
  if (history_.size() > kQueueSize_)
    history_.pop_front();
  return true;
}

void Firewall::Remove(std::unique_lock<std::mutex>& lock) {
  assert(lock.owns_lock());
//  static_cast<void>(lock);
//  std::time_t now(std::time(NULL));

//  auto old_entry(std::find_if(history_.begin(), history_.end(),
//                              [&](const ProcessedMessage& processed_message) {
//                                return ((now - std::get<2>(processed_message)) <
//                                        Parameters::message_age_to_drop);
//                              }));

//  if (old_entry != history_.begin())
//    history_.erase(history_.begin(), old_entry);
}

}  // namespace routing

}  // namespace maidsafe

