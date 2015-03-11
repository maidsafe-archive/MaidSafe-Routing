/*  Copyright 2015 MaidSafe.net limited

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

#include "cereal/types/utility.hpp"

#include "maidsafe/common/rsa.h"
#include "maidsafe/common/make_unique.h"

#include "maidsafe/routing/sentinel.h"
#include "maidsafe/routing/messages/get_client_key_response.h"
#include "maidsafe/routing/messages/get_group_key_response.h"
#include "maidsafe/routing/account_transfer_info.h"

namespace maidsafe {

namespace routing {

boost::optional<Sentinel::ResultType> Sentinel::Add(MessageHeader header,
                                                    MessageTypeTag tag,
                                                    SerialisedMessage message) {
  if (tag == MessageTypeTag::GetClientKeyResponse) {
    if (!header.FromGroup()) // "keys should always come from a group") One reponse should be enough
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    std::cout << *header.FromGroup() << ", " << header.MessageId() << " key \n";
    auto keys(node_key_accumulator_.Add(*header.FromGroup(),
                                        std::make_tuple(header, tag, std::move(message)),
                                        header.FromNode()));
    if (keys) {
      auto key(std::make_pair(NodeAddress(header.FromGroup()->data), header.MessageId()));
      auto messages(node_accumulator_.GetAll(key));
      if (messages) {
        auto resolved(Resolve(Validate<NodeAccumulatorType, KeyAccumulatorType>(
                                  messages->second, keys->second), SingleMessage()));
        if (resolved) {
          node_accumulator_.Delete(key);
          return resolved;
        }
      }
    }
  } else if (tag == MessageTypeTag::GetGroupKeyResponse) {
    if (!header.FromGroup()) // "keys should always come from a group") One reponse should be enough
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    auto keys(group_key_accumulator_.Add(*header.FromGroup(),
                                         std::make_tuple(header, tag, std::move(message)),
                                         header.FromNode()));
    if (keys) {
      auto key(std::make_pair(*header.FromGroup(), header.MessageId()));
      if (group_accumulator_.HaveName(key)) {
        auto messages(group_accumulator_.Add(std::make_pair(*header.FromGroup(),
                                                            header.MessageId()),
                                             std::make_tuple(header, tag, std::move(message)),
                                             header.FromNode()));
        if (messages) {
          auto resolved(Resolve(Validate<GroupAccumulatorType, KeyAccumulatorType>(
                                    messages->second, keys->second), GroupMessage()));
          if (resolved) {
            group_accumulator_.Delete(key);
            return resolved;
          }
        }
      }
    }
  } else {
    if (header.FromGroup()) {
      auto key(std::make_pair(*header.FromGroup(), header.MessageId()));
      if (!group_accumulator_.HaveName(key))
        send_get_group_key_(*header.FromGroup());
      auto messages(group_accumulator_.Add(key, std::make_tuple(header, tag, std::move(message)),
                                           header.FromNode()));
      if (messages) {
        auto keys(group_accumulator_.GetAll(messages->first));
        auto resolved(Resolve(Validate<GroupAccumulatorType, KeyAccumulatorType>(
                                  messages->second, keys->second),
                              GroupMessage()));
        if (resolved) {
          group_accumulator_.Delete(key);
          return resolved;
        }
      }
    } else {
      auto key(std::make_pair(header.FromNode(), header.MessageId()));
      std::cout << header.FromNode() << ", " << header.MessageId() << " message \n";
      if (!node_accumulator_.HaveName(key))
        send_get_client_key_(header.FromNode());
      auto messages(node_accumulator_.Add(key, std::make_tuple(header, tag, std::move(message)),
                                          header.FromNode()));
      if (messages) {
        auto keys(node_accumulator_.GetAll(messages->first));
        if (keys) {
          auto resolved(Resolve(Validate<NodeAccumulatorType, KeyAccumulatorType>(
                                    messages->second, keys->second),
                                SingleMessage()));
          if (resolved) {
            node_accumulator_.Delete(key);
            return resolved;
          }
        }
      }
    }
  }
  return boost::none;
}

template <>
std::vector<Sentinel::ResultType>
Sentinel::Validate<Sentinel::NodeAccumulatorType, Sentinel::KeyAccumulatorType>(
    const typename NodeAccumulatorType::Map& messages,
    const typename KeyAccumulatorType::Map& keys) {
  std::cout << messages.empty() << ", " << keys.size() << "\n";
  if (messages.empty() || keys.size() < QuorumSize)
    return std::vector<ResultType>();

  std::vector<ResultType>  verified_messages;
  std::map<Address, std::vector<asymm::PublicKey>> keys_map;

  for (const auto& node_key : keys) {
    auto key(Parse<GetClientKeyResponse>(std::get<2>(node_key.second)));
    if (keys_map.find(key.address()) == keys_map.end()) {
      keys_map.insert(std::make_pair(key.address(),
                                     std::vector<asymm::PublicKey> {key.public_key()}));
    } else {
      auto& public_keys(keys_map[key.address()]);
      if (std::none_of(public_keys.begin(), public_keys.end(),
                       [&](const asymm::PublicKey& public_key) {
                         return Serialise(key.public_key()) == Serialise(public_key);
                       }))
        keys_map[key.address()].push_back(key.public_key());
    }
  }

  // TODO(mmoadeli): Following checks that returned public keys from all nodes are identical.
  //  This could be changed in futute. And lying node to be reported.
  assert(keys_map.size() == 1);
  assert(keys_map.begin()->second.size() == 1);

  auto& public_key(*keys_map.begin()->second.begin());
  if (!asymm::ValidateKey(public_key))
    return std::vector<ResultType>();

  for (const auto& message : messages) {
    auto signature(std::get<0>(message.second).Signature());
    if (signature && asymm::CheckSignature(std::get<2>(message.second), *signature, public_key))
      verified_messages.emplace_back(message.second);
  }

  if (verified_messages.size() >= 1)
    return verified_messages;

  return std::vector<ResultType>();
}

template <>
std::vector<Sentinel::ResultType>
Sentinel::Validate<Sentinel::GroupAccumulatorType, Sentinel::KeyAccumulatorType>(
    const typename GroupAccumulatorType::Map& messages,
    const typename KeyAccumulatorType::Map& keys) {
  assert(messages.size() >= QuorumSize);
  assert(keys.size() >= QuorumSize);

  std::vector<ResultType>  verified_messages;
  std::map<Address, std::vector<asymm::PublicKey>> keys_map;

  for (const auto& group_keys : keys) {
    auto public_keys(Parse<GetGroupKeyResponse>(std::get<2>(group_keys.second)).public_keys());
    for (const auto& public_key : public_keys) {
      if (keys_map.find(public_key.first) == keys_map.end()) {
        keys_map.insert(std::make_pair(public_key.first,
                                       std::vector<asymm::PublicKey> {public_key.second}));
      } else {
        auto& existing_public_keys(keys_map[public_key.first]);
        if (std::none_of(existing_public_keys.begin(), existing_public_keys.end(),
                         [&](const asymm::PublicKey& entry) {
                           return Serialise(public_key.second) == Serialise(entry);
                         }))
          keys_map[public_key.first].push_back(public_key.second);
      }
    }
  }

  // TODO(mmoadeli): For the time being, we assume that no invalid public is received
  for (const auto& key_map : keys_map) {
    assert(key_map.second.size() == 1);
    static_cast<void>(key_map);
  }

  for (const auto& message : messages) {
    auto keys_map_iter = keys_map.find(std::get<0>(message.second).FromNode());
    if (keys_map_iter == keys_map.end())
      continue;

    auto public_key(*keys_map_iter->second.begin());
    if (!asymm::ValidateKey(public_key))
      continue;

    auto signature(std::get<0>(message.second).Signature());
    if (signature && asymm::CheckSignature(std::get<2>(message.second), *signature, public_key))
      verified_messages.emplace_back(message.second);
  }

  if (verified_messages.size() >= QuorumSize)
    return verified_messages;

  return std::vector<ResultType>();
}

boost::optional<Sentinel::ResultType>
Sentinel::Resolve(const std::vector<ResultType>& verified_messages, GroupMessage) {
  if (verified_messages.size() < QuorumSize)
    return boost::none;

  // if part addresses non-account transfer message types, where an exact match is required
  if (std::get<1>(*verified_messages.begin()) != MessageTypeTag::AccountTransfer) {
    for (size_t index(0); index < verified_messages.size(); ++index) {
      auto& serialised_message(std::get<2>(verified_messages.at(index)));
      if (static_cast<typename std::vector<ResultType>::size_type>(
              std::count_if(verified_messages.begin(), verified_messages.end(),
                            [&](const ResultType& result) {
                              return std::get<2>(result) == serialised_message;
                            })) >= QuorumSize)
        return verified_messages.at(index);
    }
  } else {  // account transfer
    std::vector<std::unique_ptr<AccountTransferInfo>> accounts;
    for (const auto& message : verified_messages)
       accounts.emplace_back(Parse<std::unique_ptr<AccountTransferInfo>>(std::get<2>(message)));
    auto merged_value_ptr((*accounts.begin())->Merge(accounts));
    if (merged_value_ptr) {
      auto result(*verified_messages.begin());
      std::get<2>(result) = Serialise(*merged_value_ptr);
      return result;
    }
  }

  return boost::none;
}

boost::optional<Sentinel::ResultType>
Sentinel::Resolve(const std::vector<ResultType>& verified_messages, SingleMessage) {
  if (verified_messages.empty())
    return boost::none;

  return verified_messages.at(0);
}

}  // namespace routing

}  // namespace maidsafe
