// src/db/AccountDb.cpp
#include "AccountDb.h"
#include "SpdlogLoggerImpl.h"
#include "models.h"
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <soci/postgresql/soci-postgresql.h>

AccountDb::AccountDb(const std::string &conninfo)
    : sql_(soci::postgresql, conninfo) {
  soci::logger slog(new SpdlogLoggerImpl());
  sql_.set_logger(slog);
}
std::optional<db::User> AccountDb::getUser(const std::string &username) {
  SPDLOG_INFO("getUser: {}", username);

  db::User u;
  try {
    sql_ << "SELECT id, username, shard_id, email, password_hash, created_at "
            "FROM users WHERE username = :name",
        soci::use(username, "name"), soci::into(u);

    SPDLOG_INFO("User loaded: {}", u.username);
    return u;
  } catch (const soci::soci_error &e) {
    SPDLOG_ERROR("SOCI error: {}", e.what());
  }

  SPDLOG_WARN("No user found for username={}", username);
  return std::nullopt;
}

int AccountDb::getShardId(int user_id) {
  int shard_id = -1;
  sql_ << "SELECT shard_id FROM users WHERE id = :id", soci::use(user_id, "id"),
      soci::into(shard_id);
  return shard_id;
}

std::optional<db::ShardInfo> AccountDb::getShardInfo(int shard_id) {
  SPDLOG_INFO("shard_id: {}", shard_id);

  db::ShardInfo s;
  try {
    soci::indicator ind;
    sql_ << "SELECT id, name, conninfo, created_at FROM shards WHERE id = :id",
        soci::use(shard_id, "id"), soci::into(s);
    // SPDLOG_INFO("Shard: {}", s.conninfo );
    return s;
  } catch (const soci::soci_error &e) {
    SPDLOG_ERROR("SOCI error: {}", e.what());
  }

  SPDLOG_WARN("No Shard found for shard_id={}", shard_id);
  return std::nullopt;
}

std::optional<db::User> AccountDb::createUser(const std::string &username,
                                              const std::string &password_hash,
                                              std::optional<std::string> email,
                                              int shard_id) {
  SPDLOG_INFO("createUser: username={}, shard_id={}", username, shard_id);

  db::User u;
  try {
    std::string emailBuf;                    // 실제 값 저장 버퍼
    soci::indicator emailInd = soci::i_null; // NULL 전달용 indicator
    if (email.has_value()) {
      emailBuf = *email;
      emailInd = soci::i_ok;
    }

    sql_
        << "INSERT INTO users(username, shard_id, email, password_hash) "
           "VALUES(:u, :s, :e, :p) "
           "RETURNING id, username, shard_id, email, password_hash, created_at",
        soci::use(username, "u"), soci::use(shard_id, "s"),
        soci::use(emailBuf, emailInd, "e"), soci::use(password_hash, "p"),
        soci::into(u);

    SPDLOG_INFO("User created: id={}, username={}", u.id, u.username);
    return u;
  } catch (const soci::soci_error &e) {
    SPDLOG_ERROR("createUser failed: {}", e.what());
    return std::nullopt;
  }
}

std::optional<db::ShardInfo>
AccountDb::getShardForUser(const std::string &username) {
  SPDLOG_INFO("getShardForUser: {}", username);
  db::ShardInfo s;
  try {
    sql_ << "SELECT s.id, s.name, s.conninfo, s.created_at "
            "FROM users u JOIN shards s ON s.id = u.shard_id "
            "WHERE u.username = :name",
        soci::use(username, "name"), soci::into(s);
    return s;
  } catch (const soci::soci_error &e) {
    SPDLOG_ERROR("getShardForUser error: {}", e.what());
    return std::nullopt;
  }
}

bool AccountDb::withUserShardSession(
    int user_id, const std::function<void(soci::session &)> &fn) {
  try {
    int shard_id = getShardId(user_id);
    if (shard_id < 0) {
      SPDLOG_WARN("withUserShardSession: invalid shard_id for user_id={}",
                  user_id);
      return false;
    }
    auto shard = getShardInfo(shard_id);
    if (!shard.has_value()) {
      SPDLOG_WARN("withUserShardSession: shard not found: {}", shard_id);
      return false;
    }

    SPDLOG_INFO("withUserShardSession: connecting to shard {} ({}).", shard->id,
                shard->name);
    soci::session shard_sql(soci::postgresql, shard->conninfo);
    soci::logger slog(new SpdlogLoggerImpl());
    shard_sql.set_logger(slog);

    fn(shard_sql);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("withUserShardSession error: {}", e.what());
    return false;
  }
}

// TCC Implementation
std::string AccountDb::startTransaction() {
  // Generate simple ID
  std::string tx_id =
      "TX_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(rand());

  try {
    int status = (int)db::TransactionStatus::PENDING;
    sql_ << "INSERT INTO transactions(id, status, created_at) VALUES(:id, :st, "
            "NOW())",
        soci::use(tx_id), soci::use(status);
    SPDLOG_INFO("Transaction started: {}", tx_id);
    return tx_id;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("startTransaction failed: {}", e.what());
    return "";
  }
}

bool AccountDb::commitTransaction(const std::string &tx_id) {
  try {
    int status = (int)db::TransactionStatus::CONFIRMED;
    sql_ << "UPDATE transactions SET status = :st WHERE id = :id",
        soci::use(status), soci::use(tx_id);
    SPDLOG_INFO("Transaction confirmed: {}", tx_id);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("commitTransaction failed: {}", tx_id);
    return false;
  }
}

bool AccountDb::cancelTransaction(const std::string &tx_id) {
  try {
    int status = (int)db::TransactionStatus::CANCELED;
    sql_ << "UPDATE transactions SET status = :st WHERE id = :id",
        soci::use(status), soci::use(tx_id);
    SPDLOG_INFO("Transaction canceled: {}", tx_id);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("cancelTransaction failed: {}", tx_id);
    return false;
  }
}