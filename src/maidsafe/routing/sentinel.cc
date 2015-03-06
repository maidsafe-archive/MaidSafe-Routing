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

#include "maidsafe/routing/sentinel.h"

namespace maidsafe {

namespace routing {

boost::optional<Sentinel::ResultType> Sentinel::Add(MessageHeader header,
                                                    MessageTypeTag tag,
                                                    SerialisedMessage message) {
  if (tag == MessageTypeTag::GetKeyResponse) {
    if (!header.FromGroup()) // "keys should always come from a group") One reponse should be enough
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    auto keys(node_key_accumulator_.Add(*header.FromGroup(),
                                        std::make_tuple(header, tag, std::move(message)),
                                        header.FromNode()));
    if (keys) {
      if (node_accumulator_.HaveName(std::make_pair(header.FromNode(), header.MessageId()))) {
        auto messages(node_accumulator_.Add(std::make_pair(header.FromNode(), header.MessageId()),
                                            std::make_tuple(header, tag, std::move(message)),
                                            header.FromNode()));
        if (messages)
          return Resolve(Validate<NodeAccumulatorType, KeyAccumulatorType>(
                             messages->second, keys->second), SingleMessage());
      }
    }
  } else if (tag == MessageTypeTag::GetGroupKeyResponse) {
    if (!header.FromGroup()) // "keys should always come from a group") One reponse should be enough
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    auto keys(group_key_accumulator_.Add(*header.FromGroup(),
                                   std::make_tuple(header, tag, std::move(message)),
                                   header.FromNode()));
    if (keys) {
      if (group_accumulator_.HaveName(std::make_pair(*header.FromGroup(), header.MessageId()))) {
        auto messages(group_accumulator_.Add(std::make_pair(*header.FromGroup(),
                                                            header.MessageId()),
                                           std::make_tuple(header, tag, std::move(message)),
                                           header.FromNode()));
        if (messages)
          return Resolve(Validate<GroupAccumulatorType, KeyAccumulatorType>(
                             messages->second, keys->second), GroupMessage());
      }
    }
  } else {
    if (header.FromGroup()) {
      if (!group_accumulator_.HaveName(std::make_pair(*header.FromGroup(), header.MessageId())))
        get_group_key_(*header.FromGroup());
      auto messages(group_accumulator_.Add(std::make_pair(*header.FromGroup(), header.MessageId()),
                                           std::make_tuple(header, tag, std::move(message)),
                                           header.FromNode()));
      if (messages) {
        auto keys(group_accumulator_.GetAll(messages->first));
        return Resolve(Validate<GroupAccumulatorType, KeyAccumulatorType>(
                           messages->second, keys->second),
                       GroupMessage());
      }
    } else {
      if (!node_accumulator_.HaveName(std::make_pair(header.FromNode(), header.MessageId())))
        get_key_(header.FromNode());
      auto messages(node_accumulator_.Add(std::make_pair(header.FromNode(), header.MessageId()),
                                          std::make_tuple(header, tag, std::move(message)),
                                          header.FromNode()));
      if (messages) {
        auto keys(node_accumulator_.GetAll(messages->first));
        return Resolve(Validate<NodeAccumulatorType, KeyAccumulatorType>(
                           messages->second, keys->second),
                       SingleMessage());
      }
    }
  }
  return boost::none;
}

template <>
boost::optional<std::vector<Sentinel::ResultType>>
Sentinel::Validate<Sentinel::NodeAccumulatorType, Sentinel::KeyAccumulatorType>(
    const typename NodeAccumulatorType::Map& messages,
    const typename KeyAccumulatorType::Map& keys) {
  assert(messages.size() >= 1);
  assert(keys.size() >= QuorumSize);

  std::vector<ResultType>  verified_messages;
  std::map<Address, std::set<SerialisedMessage>> keys_map;

  for (const auto& node_key : keys) {
    auto key(Parse<PublicKeyId>(std::get<2>(node_key.second)));
    if (keys_map.find(key.first) == keys_map.end())
      keys_map.insert(std::make_pair(key.first, std::set<SerialisedMessage> {key.second}));
    else
      keys_map[key.first].insert(key.second);
  }

  // TODO(mmoadeli): Following checks that returned public keys from all nodes are identical.
  //  This could be changed in futute. And lying node to be reported.
  assert(keys_map.size() == 1);
  assert(keys_map.begin()->second.size() == 1);

  auto public_key(Parse<asymm::PublicKey>(*keys_map.begin()->second.begin()));
  if (!asymm::ValidateKey(public_key))
    return boost::none;

  for (const auto& message : messages) {
    auto signature(std::get<0>(message.second).Signature());
    if (signature && asymm::CheckSignature(std::get<2>(message.second), *signature, public_key))
      verified_messages.emplace_back(message.second);
  }

  if (verified_messages.size() >= 1)
    return verified_messages;

  return boost::none;
}

template <>
boost::optional<std::vector<Sentinel::ResultType>>
Sentinel::Validate<Sentinel::GroupAccumulatorType, Sentinel::KeyAccumulatorType>(
    const typename GroupAccumulatorType::Map& messages,
    const typename KeyAccumulatorType::Map& keys) {
  assert(messages.size() >= QuorumSize);
  assert(keys.size() >= QuorumSize);

  std::vector<ResultType>  verified_messages;
  std::map<Address, std::set<SerialisedMessage>> keys_map;

  for (const auto& group_keys : keys) {
    auto group_public_key_ids(Parse<std::vector<PublicKeyId>>(std::get<2>(group_keys.second)));
    for (const auto& public_key_id : group_public_key_ids) {
      if (keys_map.find(public_key_id.first) == keys_map.end())
        keys_map.insert(std::make_pair(public_key_id.first,
                                       std::set<SerialisedMessage> {public_key_id.second}));
      else
        keys_map[public_key_id.first].insert(public_key_id.second);
    }
  }

  // TODO(mmoadeli): For the time being, we assume that no invalid public is received
  for (const auto& key_map : keys_map)
    assert(key_map.second.size() == 1);

  for (const auto& message : messages) {
    auto keys_map_iter = keys_map.find(std::get<0>(message.second).FromNode());
    if (keys_map_iter == keys_map.end())
      continue;

    auto public_key(Parse<asymm::PublicKey>(*keys_map_iter->second.begin()));
    if (!asymm::ValidateKey(public_key))
      continue;

    auto signature(std::get<0>(message.second).Signature());
    if (signature && asymm::CheckSignature(std::get<2>(message.second), *signature, public_key))
      verified_messages.emplace_back(message.second);
  }

  if (verified_messages.size() >= QuorumSize)
    return verified_messages;

  return boost::none;
}

boost::optional<Sentinel::ResultType>
Sentinel::Resolve(const boost::optional<std::vector<ResultType>>& verified_messages, GroupMessage) {
  if (verified_messages)
    return boost::none;

  // TODO(mmoadeli): below is only for non-account transfer message types, where exact match is
  // taken into account

  for (size_t index(0); index < verified_messages->size(); ++index) {
    auto& serialised_message(std::get<2>(verified_messages->at(index)));
    if (static_cast<typename std::vector<ResultType>::size_type>(
            std::count_if(verified_messages->begin(), verified_messages->end(),
                          [&](const ResultType& result) {
                            return std::get<2>(result) == serialised_message;
                          })) >= QuorumSize)
      return verified_messages->at(index);
  }

  return boost::none;
}

boost::optional<Sentinel::ResultType>
Sentinel::Resolve(const boost::optional<std::vector<ResultType>>& verified_messages,
                  SingleMessage) {
  if (verified_messages)
    return boost::none;

  return verified_messages->at(0);
}

}  // namespace routing

}  // namespace maidsafe
