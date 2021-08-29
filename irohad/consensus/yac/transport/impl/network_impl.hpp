/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_NETWORK_IMPL_HPP
#define IROHA_NETWORK_IMPL_HPP

#include "consensus/yac/transport/yac_network_interface.hpp"  // for YacNetwork
#include "yac.grpc.pb.h"

#include <memory>
#include <mutex>
#include <unordered_map>

#include "consensus/yac/outcome_messages.hpp"
#include "consensus/yac/vote_message.hpp"
#include "interfaces/common_objects/peer.hpp"
#include "interfaces/common_objects/types.hpp"
#include "logger/logger_fwd.hpp"
#include "network/impl/async_grpc_client.hpp"
#include "network/impl/client_factory.hpp"

namespace iroha {
  namespace consensus {
    namespace yac {

      /**
       * Class which provides implementation of transport for consensus based on
       * grpc
       */
      class NetworkImpl : public YacNetwork, public proto::Yac::Service {
       public:
        using Service = proto::Yac;
        using ClientFactory = iroha::network::ClientFactory<Service>;

        explicit NetworkImpl(
            std::shared_ptr<network::AsyncGrpcClient<google::protobuf::Empty>>
                async_call,
            std::unique_ptr<iroha::network::ClientFactory<
                ::iroha::consensus::yac::proto::Yac>> client_factory,
            logger::LoggerPtr log);

        void subscribe(
            std::shared_ptr<YacNetworkNotifications> handler) override;

        void sendState(const shared_model::interface::Peer &to,
                       const std::vector<VoteMessage> &state) override;

        /**
         * Receive votes from another peer;
         * Naming is confusing, because this is rpc call that
         * perform on another machine;
         */
        grpc::Status SendState(
            ::grpc::ServerContext *context,
            const ::iroha::consensus::yac::proto::State *request,
            ::google::protobuf::Empty *response) override;

        void stop() override;

       private:
        /**
         * Subscriber of network messages
         */
        std::weak_ptr<YacNetworkNotifications> handler_;

        /**
         * Rpc call to provide an ability to perform call grpc endpoints
         */
        std::shared_ptr<network::AsyncGrpcClient<google::protobuf::Empty>>
            async_call_;

        /**
         * Yac stub creator
         */
        std::unique_ptr<ClientFactory> client_factory_;

        std::mutex stop_mutex_;
        bool stop_requested_{false};

        logger::LoggerPtr log_;
      };

    }  // namespace yac
  }    // namespace consensus
}  // namespace iroha

#endif  // IROHA_NETWORK_IMPL_HPP
