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
        if (!messages)
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
        if (!messages)
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
      if (!messages) {
        auto keys(group_accumulator_.GetAll(messages->first));
        return Validate<GroupAccumulatorType, KeyAccumulatorType>(messages->second, keys->second);
      }
    } else {
      if (!node_accumulator_.HaveName(std::make_pair(header.FromNode(), header.MessageId())))
        get_key_(header.FromNode());
      auto messages(node_accumulator_.Add(std::make_pair(header.FromNode(), header.MessageId()),
                                          std::make_tuple(header, tag, std::move(message)),
                                          header.FromNode()));
      if (!messages) {
        auto keys(node_accumulator_.GetAll(messages->first));
        return Validate<NodeAccumulatorType, KeyAccumulatorType>(messages->second, keys->second);
      }
    }
  }
  return boost::none;
}

}  // namespace routing

}  // namespace maidsafe
