// src/db/AccountDb.h
#pragma once
#include "models.h"
#include <optional>
#include <soci/soci.h>
#include <string>

class AccountDb {
public:
  explicit AccountDb(const std::string &conninfo);

  std::optional<db::User> getUser(const std::string &username);
  int getShardId(int user_id);
  std::optional<db::ShardInfo> getShardInfo(int shard_id);

  // 신규 유저 생성 (username 고유, email NULL 가능). 성공 시 생성된 전체 레코드
  // 반환
  std::optional<db::User> createUser(const std::string &username,
                                     const std::string &password_hash,
                                     std::optional<std::string> email,
                                     int shard_id);

  // username으로 샤드 정보 조회 (users → shards join)
  std::optional<db::ShardInfo> getShardForUser(const std::string &username);

  // user_id 기준으로 해당 샤드 세션을 열어 콜백 실행 (샤드 conninfo 사용)
  // 콜백 내부에서 쿼리 실행. 세션은 함수 호출 동안만 유효.
  // user_id 기준으로 해당 샤드 세션을 열어 콜백 실행 (샤드 conninfo 사용)
  // 콜백 내부에서 쿼리 실행. 세션은 함수 호출 동안만 유효.
  bool withUserShardSession(int user_id,
                            const std::function<void(soci::session &)> &fn);

  // TCC Transaction Coordinator
  std::string startTransaction();
  bool commitTransaction(const std::string &tx_id);
  bool cancelTransaction(const std::string &tx_id);

private:
  soci::session sql_;
};