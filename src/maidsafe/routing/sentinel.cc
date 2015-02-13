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

boost::optional<std::future<Sentinel::ResultType>> Sentinel::Add(MessageHeader header,
                                                                 MessageTypeTag tag,
                                                                 SerialisedMessage message) {
  if (tag == MessageTypeTag::GetKeyResponse) {
    if (!header.FromGroup())  // "keys should always come from a group");
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    if (group_key_accumulator_.Add(*header.FromGroup(),
                                   std::make_tuple(header.Source(), tag, std::move(message)),
                                   header.FromNode())) {
      // get the other accumulator and go for it
    }
  } else {
    if (header.FromGroup() && !group_accumulator_.HaveName(*header.FromGroup())) {  // we need
      // to send a findkey for this GroupAddress

    } else if (!node_accumulator_.HaveName(header.FromNode())) {  // we need
      // to send a findkey for this SourceAddress
    }
  }

  if (node_key_accumulator_.HaveName(header.FromNode())) {  // ok direct
  } else if (header.FromGroup() && group_key_accumulator_.HaveName(*header.FromGroup())) {  // ok dht
  }
  return boost::none;
}

}  // namespace routing

}  // namespace maidsafe
