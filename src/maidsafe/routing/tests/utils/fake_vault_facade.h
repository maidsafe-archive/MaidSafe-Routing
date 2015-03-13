/*  Copyright 2014 MaidSafe.net limited

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

#ifndef MAIDSAFE_ROUTING_TESTS_UTILS_FAKE_VAULT_FACADE_H_
#define MAIDSAFE_ROUTING_TESTS_UTILS_FAKE_VAULT_FACADE_H_

#include <map>

#include "maidsafe/routing/routing_node.h"

namespace maidsafe {

namespace vault {

namespace test {

template <typename Facade>
class MaidManager {
 public:
  MaidManager() {}

  template <typename Data>
  routing::HandlePutPostReturn HandlePut(const routing::SourceAddress& from, const Data& data);
};

template <typename Facade> template <typename Data>
routing::HandlePutPostReturn MaidManager<Facade>::HandlePut(
    const routing::SourceAddress& /*source_address*/, const Data& data) {
  std::vector<routing::DestinationAddress> result;
  result.push_back(std::make_pair(routing::Destination(routing::Address(data.name())),
                                  boost::none));
  return routing::HandlePutPostReturn(result);
}


template <typename Facade>
class DataManager {
 public:
  DataManager() {}

  template <typename Data>
  routing::HandlePutPostReturn HandlePut(const routing::SourceAddress& from, const Data& data);
  template <typename Data>
  routing::HandleGetReturn HandleGet(const routing::SourceAddress& from, const Identity& name);

 private:
  std::map<std::string, std::string> data_;
  routing::CloseGroupDifference close_group_;
};

template <typename Facade> template <typename Data>
routing::HandlePutPostReturn DataManager<Facade>::HandlePut(
    const routing::SourceAddress& /*from*/, const Data& data) {
  if (data_.find(data.name().value.string()) == std::end(data_))
    data_.insert(std::make_pair(data.name().value.string(), data.data().string()));
  return boost::make_unexpected(MakeError(CommonErrors::success));
}

template <typename Facade> template <typename Data>
routing::HandleGetReturn DataManager<Facade>::HandleGet(
    const routing::SourceAddress& /*from*/, const Identity& name) {
  auto it(data_.find(name.string()));
  if (it != std::end(data_))
    return routing::HandleGetReturn(std::vector<byte>(it->second.begin(), it->second.end()));
  return boost::make_unexpected(MakeError(CommonErrors::no_such_element));
}

// Helper function to parse data name and contents
// FIXME this need discussion, adding it temporarily to progress
template <typename ParsedType>
ParsedType ParseData(const SerialisedData& serialised_data) {
  InputVectorStream binary_input_stream{serialised_data};
  typename ParsedType::Name name;
  typename ParsedType::serialised_type contents;
  Parse(binary_input_stream, name, contents);
  return ParsedType(name, contents);
}

template <>
ImmutableData ParseData<ImmutableData>(const SerialisedData& serialised_data);

class FakeVaultFacade : public MaidManager<FakeVaultFacade>,
                        public DataManager<FakeVaultFacade>,
                        public routing::RoutingNode<FakeVaultFacade> {
 public:
  FakeVaultFacade()
    : MaidManager<FakeVaultFacade>(),
      DataManager<FakeVaultFacade>(),
      routing::RoutingNode<FakeVaultFacade>() {}

  ~FakeVaultFacade() = default;

  void HandleConnectionAdded(routing::Address /*address*/) {}

  routing::HandleGetReturn HandleGet(routing::SourceAddress from, routing::Authority authority,
                                     DataTagValue data_type, Identity data_name);

  routing::HandlePutPostReturn HandlePut(routing::SourceAddress from,
      routing::Authority from_authority, routing::Authority authority, DataTagValue data_type,
          SerialisedData serialised_data);

  bool HandlePost(const routing::SerialisedMessage& message);
  // not in local cache do upper layers have it (called when we are in target group)
   template <typename DataType>
  boost::expected<routing::SerialisedMessage, maidsafe_error> HandleGet(routing::Address) {
    return boost::make_unexpected(MakeError(CommonErrors::no_such_element));
  }
  // default put is allowed unless prevented by upper layers
  bool HandlePut(routing::Address, routing::SerialisedMessage);
  // if the implementation allows any put of data in unauthenticated mode
  bool HandleUnauthenticatedPut(routing::Address, routing::SerialisedMessage);
  void HandleChurn(routing::CloseGroupDifference diff);
};

}  // namespace test

}  // namespace vault

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_TESTS_UTILS_FAKE_VAULT_FACADE_H_
