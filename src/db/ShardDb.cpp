// src/db/ShardDb.cpp
#include "ShardDb.h"
#include "SpdlogLoggerImpl.h"
#include <iostream>
#include <soci/postgresql/soci-postgresql.h>
#include <spdlog/spdlog.h>

ShardDb::ShardDb(const std::string &conninfo)
    : sql_(soci::postgresql, conninfo) {
  soci::logger slog(new SpdlogLoggerImpl());
  sql_.set_logger(slog);
}

bool ShardDb::insertMessage(long long room_id, int user_id,
                            const std::string &content) {
  try {
    sql_
        << "INSERT INTO messages(room_id, user_id, content) VALUES(:r, :u, :c)",
        soci::use(room_id, "r"), soci::use(user_id, "u"),
        soci::use(content, "c");
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("SOCI error: {}", e.what());
    return false;
  }
}

std::vector<db::Message> ShardDb::getMessages(long long room_id) {
  std::vector<db::Message> msgs;
  try {
    soci::rowset<db::Message> rs =
        (sql_.prepare
             << "SELECT * FROM messages WHERE room_id = :r ORDER BY id",
         soci::use(room_id));
    for (auto &m : rs)
      msgs.push_back(m);
  } catch (const std::exception &e) {
    SPDLOG_ERROR("SOCI error: {}", e.what());
  }
  return msgs;
}

// TCC Implementation
std::optional<db::Wallet> ShardDb::getWallet(int user_id) {
  try {
    db::Wallet w;
    soci::indicator ind;
    sql_ << "SELECT user_id, money, held_money FROM wallets WHERE user_id = :u",
        soci::use(user_id), soci::into(w, ind);

    if (sql_.got_data())
      return w;
    return std::nullopt;
  } catch (const std::exception &e) {
    // Table might not exist or user not found
    return std::nullopt;
  }
}

bool ShardDb::prepareTransfer(int user_id, int amount, bool is_deduct,
                              const std::string &tx_id) {
  try {
    if (is_deduct) {
      // Check balance and reserve
      soci::statement st =
          (sql_.prepare << "UPDATE wallets SET money = money - :a, held_money "
                           "= held_money + :a "
                           "WHERE user_id = :u AND money >= :a",
           soci::use(amount, "a"), soci::use(amount, "a"),
           soci::use(user_id, "u"));
      st.execute(true);
      if (st.get_affected_rows() == 0) {
        SPDLOG_WARN("prepareTransfer: Insufficient funds or user not found. "
                    "user_id={}, amount={}",
                    user_id, amount);
        return false;
      }
    } else {
      // Ensure wallet exists for receiver
      int count = 0;
      sql_ << "SELECT count(*) FROM wallets WHERE user_id = :u",
          soci::use(user_id), soci::into(count);
      if (count == 0) {
        sql_ << "INSERT INTO wallets(user_id, money, held_money) VALUES(:u, 0, "
                "0)",
            soci::use(user_id);
      }
    }
    SPDLOG_INFO("prepareTransfer success: user_id={}, is_deduct={}", user_id,
                is_deduct);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("prepareTransfer error: {}", e.what());
    return false;
  }
}

bool ShardDb::commitTransfer(int user_id, int amount, bool is_deduct,
                             const std::string &tx_id) {
  try {
    if (is_deduct) {
      // Burn held money
      sql_ << "UPDATE wallets SET held_money = held_money - :a WHERE user_id = "
              ":u",
          soci::use(amount), soci::use(user_id);
    } else {
      // Add real money
      sql_ << "UPDATE wallets SET money = money + :a WHERE user_id = :u",
          soci::use(amount), soci::use(user_id);
    }
    SPDLOG_INFO("commitTransfer success: user_id={}, is_deduct={}", user_id,
                is_deduct);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("commitTransfer error: {}", e.what());
    return false;
  }
}

bool ShardDb::rollbackTransfer(int user_id, int amount, bool is_deduct,
                               const std::string &tx_id) {
  try {
    if (is_deduct) {
      // Restore money
      sql_ << "UPDATE wallets SET money = money + :a, held_money = held_money "
              "- :a WHERE user_id = :u",
          soci::use(amount), soci::use(amount), soci::use(user_id);
    } else {
      // No-op for receiver
    }
    SPDLOG_INFO("rollbackTransfer success: user_id={}, is_deduct={}", user_id,
                is_deduct);
    return true;
  } catch (const std::exception &e) {
    SPDLOG_ERROR("rollbackTransfer error: {}", e.what());
    return false;
  }
}