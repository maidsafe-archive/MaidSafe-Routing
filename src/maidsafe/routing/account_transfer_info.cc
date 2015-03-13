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

#include "maidsafe/routing/account_transfer_info.h"

#include <limits>
#include <utility>

namespace maidsafe {

namespace routing {

AccountTransferInfo::NameAndTypeId::NameAndTypeId(Identity name_in, DataTypeId type_id_in)
    : name(std::move(name_in)), type_id(type_id_in) {}

AccountTransferInfo::NameAndTypeId::NameAndTypeId()
    : name(), type_id(std::numeric_limits<DataTypeId::value_type>::max()) {}

AccountTransferInfo::NameAndTypeId::NameAndTypeId(const NameAndTypeId&) = default;

AccountTransferInfo::NameAndTypeId::NameAndTypeId(NameAndTypeId&& other) MAIDSAFE_NOEXCEPT
    : name(std::move(other.name)), type_id(std::move(other.type_id)) {}

AccountTransferInfo::NameAndTypeId& AccountTransferInfo::NameAndTypeId::operator=(
    const AccountTransferInfo::NameAndTypeId&) = default;

AccountTransferInfo::NameAndTypeId& AccountTransferInfo::NameAndTypeId::operator=(
    AccountTransferInfo::NameAndTypeId&& other) MAIDSAFE_NOEXCEPT {
  name = std::move(other.name);
  type_id = std::move(other.type_id);
  return *this;
}

AccountTransferInfo::AccountTransferInfo(Identity name) : name_(std::move(name)) {
  if (!IsInitialised())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
}

AccountTransferInfo::AccountTransferInfo() = default;

AccountTransferInfo::AccountTransferInfo(const AccountTransferInfo&) = default;

AccountTransferInfo::AccountTransferInfo(AccountTransferInfo&& other)
    : name_(std::move(other.name_)) {}

AccountTransferInfo& AccountTransferInfo::operator=(const AccountTransferInfo&) = default;

AccountTransferInfo& AccountTransferInfo::operator=(AccountTransferInfo&& other) {
  name_ = std::move(other.name_);
  return *this;
}

AccountTransferInfo::~AccountTransferInfo() = default;

bool AccountTransferInfo::IsInitialised() const { return name_.IsInitialised(); }

const Identity& AccountTransferInfo::Name() const {
  if (!IsInitialised())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
  return name_;
}

DataTypeId AccountTransferInfo::TypeId() const {
  if (!IsInitialised())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
  return DataTypeId(ThisTypeId());
}

AccountTransferInfo::NameAndTypeId AccountTransferInfo::NameAndType() const {
  if (!IsInitialised())
    BOOST_THROW_EXCEPTION(MakeError(CommonErrors::uninitialised));
  return NameAndTypeId(name_, TypeId());
}

bool operator==(const AccountTransferInfo::NameAndTypeId& lhs,
               const AccountTransferInfo::NameAndTypeId& rhs) {
  return std::tie(lhs.name, lhs.type_id) == std::tie(rhs.name, rhs.type_id);
}

bool operator!=(const AccountTransferInfo::NameAndTypeId& lhs,
                const AccountTransferInfo::NameAndTypeId& rhs) {
  return !operator==(lhs, rhs);
}

bool operator<(const AccountTransferInfo::NameAndTypeId& lhs,
               const AccountTransferInfo::NameAndTypeId& rhs) {
  return std::tie(lhs.name, lhs.type_id) < std::tie(rhs.name, rhs.type_id);
}

bool operator>(const AccountTransferInfo::NameAndTypeId& lhs,
               const AccountTransferInfo::NameAndTypeId& rhs) {
  return operator<(rhs, lhs);
}

bool operator<=(const AccountTransferInfo::NameAndTypeId& lhs,
                const AccountTransferInfo::NameAndTypeId& rhs) {
  return !operator>(lhs, rhs);
}

bool operator>=(const AccountTransferInfo::NameAndTypeId& lhs,
                const AccountTransferInfo::NameAndTypeId& rhs) {
  return !operator<(lhs, rhs);
}

}  // namespace routing

}  // namespace maidsafe
