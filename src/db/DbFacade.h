// src/db/DbFacade.h
#pragma once
#include "DbRouter.h"
#include "models.h"
#include <optional>
#include <vector>

class DbFacade {
public:
  explicit DbFacade(const std::string &account_conninfo);

  std::optional<db::User> findUser(const std::string &username);
  // 신규 유저 생성 (AccountDb::createUser 위임)
  std::optional<db::User> createUser(const std::string &username,
                                     const std::string &password_hash,
                                     std::optional<std::string> email,
                                     int shard_id);
  bool saveMessage(int user_id, long long room_id, const std::string &content);
  bool saveMessage(int user_id, long long room_id, const std::string &content);
  std::vector<db::Message> loadMessages(int user_id, long long room_id);

  // TCC Orchestration
  bool transferMoney(const std::string &from_username,
                     const std::string &to_username, int amount);

private:
  DbRouter router_;
};