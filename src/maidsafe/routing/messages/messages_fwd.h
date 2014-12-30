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

namespace maidsafe {

namespace routing {

enum class MessageTypeTag : uint16_t {
  Connect,
  ForwardConnect,
  FindGroup,
  FindGroupResponse,
  GetData,
  GetDataResponse,
  PutData,
  PutKey,
  ForwardPutData,
  PutDataResponse,
  ForwardPost,
  Post,
  ForwardRequest,
  Request,
  Response
};

struct Connect;
struct ConnectResponse;
struct ForwardConnect;
struct FindGroup;
struct FindGroupResponse;
struct GetData;
struct GetDataResponse;
struct ForwardPutData;
struct PutData;
struct PutKey;
struct PutDataResponse;
struct ForwardPost;
struct Post;
struct ForwardRequest;
struct Request;
struct Response;

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_MESSAGES_MESSAGES_FWD_H_
