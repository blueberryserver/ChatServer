// src/db/DbFacade.cpp
#include "DbFacade.h"
#include <iostream>
#include <spdlog/spdlog.h>

DbFacade::DbFacade(const std::string &account_conninfo)
    : router_(account_conninfo) {}

std::optional<db::User> DbFacade::findUser(const std::string &username) {
  return router_.getUser(username);
}
std::optional<db::User> DbFacade::createUser(const std::string &username,
                                             const std::string &password_hash,
                                             std::optional<std::string> email,
                                             int shard_id) {

  auto accountDb = router_.getAccountDb(); // getAccountDb()는 AccountDb 참조
                                           // 또는 포인터를 반환한다고 가정
  if (!accountDb) {
    return std::nullopt;
  }
  return accountDb->createUser(username, password_hash, email, shard_id);
}

bool DbFacade::saveMessage(int user_id, long long room_id,
                           const std::string &content) {
  auto shard = router_.getShardForUser(user_id);
  if (!shard)
    return false;
  return shard->insertMessage(room_id, user_id, content);
}

std::vector<db::Message> DbFacade::loadMessages(int user_id,
                                                long long room_id) {
  auto shard = router_.getShardForUser(user_id);
  return shard->getMessages(room_id);
}

// TCC Orchestration
bool DbFacade::transferMoney(const std::string &from_username,
                             const std::string &to_username, int amount) {
  SPDLOG_INFO("transferMoney: {} -> {}, amount={}", from_username, to_username,
              amount);

  auto fromUser = findUser(from_username);
  auto toUser = findUser(to_username);

  if (!fromUser) {
    SPDLOG_WARN("transferMoney: Sender not found {}", from_username);
    return false;
  }
  if (!toUser) {
    SPDLOG_WARN("transferMoney: Receiver not found {}", to_username);
    return false;
  }

  // 1. Start Global Transaction (Coordinator)
  auto accountDb = router_.getAccountDb();
  std::string tx_id = accountDb->startTransaction();
  if (tx_id.empty()) {
    SPDLOG_ERROR("transferMoney: Failed to start transaction");
    return false;
  }

  auto shardA = router_.getShardForUser(fromUser->id);
  auto shardB = router_.getShardForUser(toUser->id);

  if (!shardA || !shardB) {
    SPDLOG_ERROR("transferMoney: Failed to connect to shards");
    accountDb->cancelTransaction(tx_id);
    return false;
  }

  // 2. Try Phase (Prepare)
  // 2-1. Reserve from Sender
  bool resA = shardA->prepareTransfer(fromUser->id, amount, true, tx_id);
  if (!resA) {
    SPDLOG_WARN("transferMoney: Sender prepare failed");
    accountDb->cancelTransaction(tx_id);
    return false;
  }

  // 2-2. Check Receiver
  bool resB = shardB->prepareTransfer(toUser->id, amount, false, tx_id);
  if (!resB) {
    SPDLOG_WARN("transferMoney: Receiver prepare failed");
    // Rollback Sender
    shardA->rollbackTransfer(fromUser->id, amount, true, tx_id);
    accountDb->cancelTransaction(tx_id);
    return false;
  }

  // 3. Confirm Phase (Commit)
  // 3-1. Mark Global Tx CONFIRMED
  bool commitMeta = accountDb->commitTransaction(tx_id);
  if (!commitMeta) {
    SPDLOG_ERROR("transferMoney: Meta commit failed. Rolling back.");
    shardA->rollbackTransfer(fromUser->id, amount, true, tx_id);
    shardB->rollbackTransfer(toUser->id, amount, false, tx_id);
    return false;
  }

  // 3-2. Commit Shards
  shardA->commitTransfer(fromUser->id, amount, true, tx_id);
  shardB->commitTransfer(toUser->id, amount, false, tx_id);

  SPDLOG_INFO("transferMoney: Success. tx_id={}", tx_id);
  return true;
}