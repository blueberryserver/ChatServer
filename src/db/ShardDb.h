// src/db/ShardDb.h
#pragma once
#include "models.h"
#include <soci/soci.h>
#include <string>
#include <vector>

class ShardDb {
public:
  explicit ShardDb(const std::string &conninfo);

  bool insertMessage(long long room_id, int user_id,
                     const std::string &content);
  std::vector<db::Message> getMessages(long long room_id);

  // TCC for Wallet
  std::optional<db::Wallet> getWallet(int user_id);
  bool prepareTransfer(int user_id, int amount, bool is_deduct,
                       const std::string &tx_id);
  bool commitTransfer(int user_id, int amount, bool is_deduct,
                      const std::string &tx_id);
  bool rollbackTransfer(int user_id, int amount, bool is_deduct,
                        const std::string &tx_id);

private:
  soci::session sql_;
};