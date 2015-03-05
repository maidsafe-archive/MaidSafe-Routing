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
          return Validate<NodeAccumulatorType, KeyAccumulatorType>(messages->second, keys->second);
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
          return Validate<GroupAccumulatorType, KeyAccumulatorType>(messages->second, keys->second);
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
        return Validate<GroupAccumulatorType, KeyAccumulatorType>(messages->second, keys->second);
      }
    } else {
      if (!node_accumulator_.HaveName(std::make_pair(header.FromNode(), header.MessageId())))
        get_key_(header.FromNode());
      auto messages(node_accumulator_.Add(std::make_pair(header.FromNode(), header.MessageId()),
                                          std::make_tuple(header, tag, std::move(message)),
                                          header.FromNode()));
      if (messages) {
        auto keys(node_accumulator_.GetAll(messages->first));
        return Validate<NodeAccumulatorType, KeyAccumulatorType>(messages->second, keys->second);
      }
    }
  }
  return boost::none;
}

template <>
boost::optional<Sentinel::ResultType>
Sentinel::Validate<Sentinel::NodeAccumulatorType, Sentinel::KeyAccumulatorType>(
    const typename NodeAccumulatorType::Map& messages,
    const typename KeyAccumulatorType::Map& keys) {
  assert(messages.size() >= 1);
  assert(keys.size() >= QuorumSize);
  // Following checks that returned public keys from all nodes are identical. This could be
  // changed in futute. And lying node to be reported.
  auto& raw_public_key(std::get<2>(keys.begin()->second));
  assert(keys.size() == static_cast<typename KeyAccumulatorType::Map::size_type>(
                            std::count_if(keys.begin(), keys.end(),
                            [&](const std::pair<NodeId, ResultType>& entry) {
                              return raw_public_key == std::get<2>(entry.second);
                            })));
  auto signature(std::get<0>(messages.begin()->second).Signature());
  if (!signature)
    return boost::none;

  std::string public_key_string;
  std::copy(raw_public_key.begin(), raw_public_key.end(), std::back_inserter(public_key_string));
  asymm::PublicKey public_key;
  try {
    public_key = asymm::DecodeKey(asymm::EncodedPublicKey(public_key_string));
  }
  catch (const std::exception& /*error*/) {
    return boost::none;
  }

  if (asymm::CheckSignature(std::get<2>(messages.begin()->second), *signature, public_key))
    return messages.begin()->second;

  return boost::none;
}

template <>
boost::optional<Sentinel::ResultType>
Sentinel::Validate<Sentinel::GroupAccumulatorType, Sentinel::KeyAccumulatorType>(
    const typename GroupAccumulatorType::Map& messages,
    const typename KeyAccumulatorType::Map& keys) {
  assert(messages.size() >= QuorumSize);
  assert(keys.size() >= QuorumSize);
  return boost::none;
}

}  // namespace routing

}  // namespace maidsafe
