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

#ifndef MAIDSAFE_ROUTING_ACCOUNT_TRANSFER_INFO_H_
#define MAIDSAFE_ROUTING_ACCOUNT_TRANSFER_INFO_H_

#include <cstdint>

#include "cereal/archives/binary.hpp"
#include "cereal/types/polymorphic.hpp"

#include "maidsafe/common/config.h"
#include "maidsafe/common/error.h"
#include "maidsafe/common/identity.h"
#include "maidsafe/common/types.h"

namespace maidsafe {

namespace routing {

class AccountTransferInfo {
 public:
  struct NameAndTypeId {
    NameAndTypeId(Identity name_in, DataTypeId type_id_in);
    NameAndTypeId();
    NameAndTypeId(const NameAndTypeId&);
    NameAndTypeId(NameAndTypeId&& other) MAIDSAFE_NOEXCEPT;
    NameAndTypeId& operator=(const NameAndTypeId&);
    NameAndTypeId& operator=(NameAndTypeId&& other) MAIDSAFE_NOEXCEPT;

    template <typename Archive>
    Archive& serialize(Archive& archive) {
      return archive(name, type_id);
    }

    Identity name;
    DataTypeId type_id;
  };

  AccountTransferInfo();
  AccountTransferInfo(const AccountTransferInfo&);
  AccountTransferInfo(AccountTransferInfo&& other);
  AccountTransferInfo& operator=(const AccountTransferInfo&);
  AccountTransferInfo& operator=(AccountTransferInfo&& other);
  virtual ~AccountTransferInfo();

  virtual std::unique_ptr<AccountTransferInfo> Merge(
               const std::vector<std::unique_ptr<AccountTransferInfo>>& accounts) = 0;

  // Returns false for a default-constructed instance of this class, otherwise true.
  bool IsInitialised() const;

  // Throws if IsInitialised() is false.
  const Identity& Name() const;

  // Throws if IsInitialised() is false.
  DataTypeId TypeId() const;

  // Throws if IsInitialised() is false.
  NameAndTypeId NameAndType() const;

  // Throws if IsInitialised() is false.
  template <typename Archive>
  Archive& save(Archive& archive) const {
    if (!IsInitialised())
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
    return archive(name_);
  }

  template <typename Archive>
  Archive& load(Archive& archive) {
    archive(name_);
    if (!IsInitialised())
      BOOST_THROW_EXCEPTION(MakeError(CommonErrors::parsing_error));
    return archive;
  }

 protected:
  // Throws if IsInitialised() is false.
  explicit AccountTransferInfo(Identity name);

  // Doesn't need to throw.
  virtual std::uint32_t ThisTypeId() const = 0;

  Identity name_;
};

bool operator==(const AccountTransferInfo::NameAndTypeId& lhs,
                const AccountTransferInfo::NameAndTypeId& rhs);
bool operator!=(const AccountTransferInfo::NameAndTypeId& lhs,
                const AccountTransferInfo::NameAndTypeId& rhs);
bool operator<(const AccountTransferInfo::NameAndTypeId& lhs,
               const AccountTransferInfo::NameAndTypeId& rhs);
bool operator>(const AccountTransferInfo::NameAndTypeId& lhs,
               const AccountTransferInfo::NameAndTypeId& rhs);
bool operator<=(const AccountTransferInfo::NameAndTypeId& lhs,
                const AccountTransferInfo::NameAndTypeId& rhs);
bool operator>=(const AccountTransferInfo::NameAndTypeId& lhs,
                const AccountTransferInfo::NameAndTypeId& rhs);

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ACCOUNT_TRANSFER_INFO_H_
