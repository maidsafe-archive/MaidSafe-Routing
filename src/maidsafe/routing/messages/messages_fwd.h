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

#ifndef MAIDSAFE_ROUTING_MESSAGES_MESSAGES_FWD_H_
#define MAIDSAFE_ROUTING_MESSAGES_MESSAGES_FWD_H_

#include <cstdint>

namespace maidsafe {

namespace routing {

enum class MessageTypeTag : uint16_t {
  Connect,
  ConnectResponse,
  FindGroup,
  FindGroupResponse,
  GetData,
  GetDataResponse,
  GetKey,
  GetKeyResponse,
  GetGroupKey,
  GetGroupKeyResponse,
  PutData,
  PutKey,
  PutDataResponse,
  ClientPost,
  PostMessage,
  RequestMessage,
  ResponseMessage
};

class Connect;
class ConnectResponse;
class FindGroup;
class FindGroupResponse;
class GetData;
class GetDataResponse;
class PutData;
struct PutDataResponse;
class PostMessage;
struct RequestMessage;
struct ResponseMessage;

template <class T>
struct MessageToTag;
template <>
struct MessageToTag<Connect> {
  static MessageTypeTag value() { return MessageTypeTag::Connect; }
};
template <>
struct MessageToTag<ConnectResponse> {
  static MessageTypeTag value() { return MessageTypeTag::ConnectResponse; }
};
template <>
struct MessageToTag<FindGroup> {
  static MessageTypeTag value() { return MessageTypeTag::FindGroup; }
};
template <>
struct MessageToTag<FindGroupResponse> {
  static MessageTypeTag value() { return MessageTypeTag::FindGroupResponse; }
};
template <>
struct MessageToTag<GetData> {
  static MessageTypeTag value() { return MessageTypeTag::GetData; }
};
template <>
struct MessageToTag<GetDataResponse> {
  static MessageTypeTag value() { return MessageTypeTag::GetDataResponse; }
};
template <>
struct MessageToTag<PutData> {
  static MessageTypeTag value() { return MessageTypeTag::PutData; }
};
template <>
struct MessageToTag<PutDataResponse> {
  static MessageTypeTag value() { return MessageTypeTag::PutDataResponse; }
};
template <>
struct MessageToTag<PostMessage> {
  static MessageTypeTag value() { return MessageTypeTag::PostMessage; }
};
template <>
struct MessageToTag<RequestMessage> {
  static MessageTypeTag value() { return MessageTypeTag::RequestMessage; }
};
template <>
struct MessageToTag<ResponseMessage> {
  static MessageTypeTag value() { return MessageTypeTag::ResponseMessage; }
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_MESSAGES_FWD_H_
