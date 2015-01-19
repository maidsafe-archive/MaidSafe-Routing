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
  ClientConnectResponse,
  ClientConnect,
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
  ClientPutData,
  PutDataResponse,
  ClientPost,
  Post,
  ClientRequest,
  Request,
  ClientResponse,
  Response
};

struct Connect;
struct ConnectResponse;
struct ClientConnectResponse;
struct ClientConnect;
struct FindGroup;
struct FindGroupResponse;
struct GetData;
struct GetDataResponse;
struct GetKey;
struct GetKeyResponse;
struct GetGroupKey;
struct GetGroupKeyResponse;
struct ClientPutData;
struct PutData;
struct PutKey;
struct PutDataResponse;
struct ClientPost;
struct Post;
struct ClientRequest;
struct Request;
struct Response;
struct ClientResponse;

template<class T> struct MessageToTag;
template<> struct MessageToTag<Connect> { static MessageTypeTag value() { return MessageTypeTag::Connect; } };
template<> struct MessageToTag<ConnectResponse> { static MessageTypeTag value() { return MessageTypeTag::ConnectResponse; } };
template<> struct MessageToTag<ClientConnectResponse> { static MessageTypeTag value() { return MessageTypeTag::ClientConnectResponse; } };
template<> struct MessageToTag<ClientConnect> { static MessageTypeTag value() { return MessageTypeTag::ClientConnect; } };
template<> struct MessageToTag<FindGroup> { static MessageTypeTag value() { return MessageTypeTag::FindGroup; } };
template<> struct MessageToTag<FindGroupResponse> { static MessageTypeTag value() { return MessageTypeTag::FindGroupResponse; } };
template<> struct MessageToTag<GetData> { static MessageTypeTag value() { return MessageTypeTag::GetData; } };
template<> struct MessageToTag<GetDataResponse> { static MessageTypeTag value() { return MessageTypeTag::GetDataResponse; } };
template<> struct MessageToTag<GetKey> { static MessageTypeTag value() { return MessageTypeTag::GetKey; } };
template<> struct MessageToTag<GetKeyResponse> { static MessageTypeTag value() { return MessageTypeTag::GetKeyResponse; } };
template<> struct MessageToTag<GetGroupKey> { static MessageTypeTag value() { return MessageTypeTag::GetGroupKey; } };
template<> struct MessageToTag<GetGroupKeyResponse> { static MessageTypeTag value() { return MessageTypeTag::GetGroupKeyResponse; } };
template<> struct MessageToTag<ClientPutData> { static MessageTypeTag value() { return MessageTypeTag::ClientPutData; } };
template<> struct MessageToTag<PutData> { static MessageTypeTag value() { return MessageTypeTag::PutData; } };
template<> struct MessageToTag<PutKey> { static MessageTypeTag value() { return MessageTypeTag::PutKey; } };
template<> struct MessageToTag<PutDataResponse> { static MessageTypeTag value() { return MessageTypeTag::PutDataResponse; } };
template<> struct MessageToTag<ClientPost> { static MessageTypeTag value() { return MessageTypeTag::ClientPost; } };
template<> struct MessageToTag<Post> { static MessageTypeTag value() { return MessageTypeTag::Post; } };
template<> struct MessageToTag<ClientRequest> { static MessageTypeTag value() { return MessageTypeTag::ClientRequest; } };
template<> struct MessageToTag<Request> { static MessageTypeTag value() { return MessageTypeTag::Request; } };
template<> struct MessageToTag<Response> { static MessageTypeTag value() { return MessageTypeTag::Response; } };
template<> struct MessageToTag<ClientResponse> { static MessageTypeTag value() { return MessageTypeTag::ClientResponse; } };

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_MESSAGES_FWD_H_
