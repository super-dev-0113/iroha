/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "consensus/yac/transport/impl/network_impl.hpp"

#include <grpc++/grpc++.h>
#include <memory>

#include <fmt/core.h>
#include "consensus/yac/storage/yac_common.hpp"
#include "consensus/yac/transport/yac_pb_converters.hpp"
#include "consensus/yac/vote_message.hpp"
#include "interfaces/common_objects/peer.hpp"
#include "logger/logger.hpp"
#include "network/impl/client_factory.hpp"
#include "yac.pb.h"

namespace iroha {
  namespace consensus {
    namespace yac {
      // ----------| Public API |----------

      NetworkImpl::NetworkImpl(
          std::shared_ptr<network::AsyncGrpcClient<google::protobuf::Empty>>
              async_call,
          std::unique_ptr<ClientFactory> client_factory,
          logger::LoggerPtr log)
          : async_call_(async_call),
            client_factory_(std::move(client_factory)),
            log_(std::move(log)) {}

      void NetworkImpl::subscribe(
          std::shared_ptr<YacNetworkNotifications> handler) {
        handler_ = handler;
      }

      void NetworkImpl::stop() {
        std::lock_guard<std::mutex> stop_lock(stop_mutex_);
        stop_requested_ = true;
      }

      void NetworkImpl::sendState(const shared_model::interface::Peer &to,
                                  const std::vector<VoteMessage> &state) {
        std::lock_guard<std::mutex> stop_lock(stop_mutex_);
        if (stop_requested_) {
          log_->warn("Not sending state to {} because stop was requested.", to);
          return;
        }

        proto::State request;
        for (const auto &vote : state) {
          auto pb_vote = request.add_votes();
          *pb_vote = PbConverters::serializeVote(vote);
        }

        client_factory_->createClient(to).match(
            [&](auto client) {
              async_call_->Call(
                  [client = std::move(client.value),
                   request = std::move(request),
                   log = log_,
                   log_sending_msg = fmt::format(
                       "Send votes bundle[size={}] to {}", state.size(), to)](
                      auto context, auto cq) {
                    log->info(log_sending_msg);
                    return client->AsyncSendState(context, request, cq);
                  });
            },
            [&](const auto &error) {
              log_->error("Could not send state to {}: {}", to, error.error);
            });
      }

      grpc::Status NetworkImpl::SendState(
          ::grpc::ServerContext *context,
          const ::iroha::consensus::yac::proto::State *request,
          ::google::protobuf::Empty *response) {
        std::vector<VoteMessage> state;
        for (const auto &pb_vote : request->votes()) {
          if (auto vote = PbConverters::deserializeVote(pb_vote, log_)) {
            state.push_back(*vote);
          }
        }
        if (state.empty()) {
          log_->info("Received an empty votes collection");
          return grpc::Status::CANCELLED;
        }
        if (not sameKeys(state)) {
          log_->info(
              "Votes are statelessly invalid: proposal rounds are different");
          return grpc::Status::CANCELLED;
        }

        log_->info(
            "Received votes[size={}] from {}", state.size(), context->peer());

        if (auto notifications = handler_.lock()) {
          notifications->onState(std::move(state));
        } else {
          log_->error("Unable to lock the subscriber");
        }
        return grpc::Status::OK;
      }

    }  // namespace yac
  }    // namespace consensus
}  // namespace iroha
