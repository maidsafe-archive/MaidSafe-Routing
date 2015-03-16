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

#include "maidsafe/routing/tests/utils/fake_vault_facade.h"
#include "maidsafe/common/identity.h"
#include "maidsafe/common/types.h"
#include "maidsafe/common/data_types/immutable_data.h"
#include "maidsafe/common/data_types/mutable_data.h"
#include "maidsafe/passport/types.h"

namespace maidsafe {

namespace vault {

namespace test {

//template <>
//ImmutableData ParseData<ImmutableData>(const SerialisedData& serialised_data) {
//  auto digest_size(crypto::SHA512::DIGESTSIZE);
//  std::string name(serialised_data.begin(), serialised_data.begin() + digest_size);
//  std::string content(serialised_data.begin() + digest_size, serialised_data.end());
//  return ImmutableData(ImmutableData::Name(Identity(name)),
//                       ImmutableData::serialised_type(NonEmptyString(content)));
//}

routing::HandlePutPostReturn FakeVaultFacade::HandlePut(routing::SourceAddress /*from*/,
    routing::Authority /*from_authority*/, routing::Authority /*authority*/,
    Data::NameAndTypeId /*name_and_type_id*/, SerialisedData /*serialised_data*/) {
  switch (authority) {
//    case routing::Authority::client_manager:
//      if (from_authority != routing::Authority::client)
//        break;
//      if (name_and_type_id.type_id == detail::TypeId<ImmutableData>::value)
//        return MaidManager::HandlePut(from, Parse<ImmutableData>(serialised_data));
//      else if (name_and_type_id.type_id == detail::TypeId<MutableData>::value)
//        return MaidManager::HandlePut(from, Parse<MutableData>(serialised_data));
//      else if (name_and_type_id.type_id == detail::TypeId<passport::PublicPmid>::value)
//        return MaidManager::HandlePut(from, Parse<passport::PublicPmid>(serialised_data));
    case routing::Authority::nae_manager:
      if (from_authority != routing::Authority::client_manager)
        break;
      if (name_and_type_id.type_id == detail::TypeId<ImmutableData>::value)
        return DataManager::HandlePut(from, Parse<ImmutableData>(serialised_data));
      else if (name_and_type_id.type_id == detail::TypeId<MutableData>::value)
        return DataManager::HandlePut(from, Parse<MutableData>(serialised_data));
      break;
    default:
      break;
  }
  return boost::make_unexpected(MakeError(VaultErrors::failed_to_handle_request));
}

routing::HandleGetReturn FakeVaultFacade::HandleGet(routing::SourceAddress /*from*/,
    routing::Authority /*from_authority*/, routing::Authority /*authority*/,
    Data::NameAndTypeId /*name_and_type_id*/) {
//  switch (authority) {
//    case routing::Authority::nae_manager:
//      if (name_and_type_id.type_id == detail::TypeId<ImmutableData>::value)
//        return DataManager::template HandleGet<ImmutableData>(from, data_name);
//      else if (name_and_type_id.type_id == detail::TypeId<MutableData>::value)
//        return DataManager::template HandleGet<MutableData>(from, data_name);
//      break;
//    default:
//      break;
//  }
  return boost::make_unexpected(MakeError(VaultErrors::failed_to_handle_request));
}

void FakeVaultFacade::HandleChurn(routing::CloseGroupDifference /*diff*/) {
}

}  // namespace test

}  // namespace vault

}  // namespace maidsafe
