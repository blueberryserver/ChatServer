// db/models.h
#pragma once

#include <chrono>
#include <optional>
#include <soci/soci.h>
#include <string>

namespace db {

// 공통 타입 별칭 (실제 기본 타입은 그대로 유지)
using UserId = int;        // account_db.users.id
using ShardId = int;       // account_db.shards.id
using RoomId = long long;  // chatdb_N.chat_rooms.id (또는 messages.room_id)
using MessageId = int;     // chatdb_N.messages.id
using Timestamp = std::tm; // SOCI 가 기본 지원하는 시간 타입

// ========================
// User (account_db.users)
// ========================
struct User {
  int id{};
  std::string username;
  int shard_id{};
  std::optional<std::string> email;
  std::string password_hash;
  int money = 0;      // 실제 잔액
  int held_money = 0; // 트랜잭션 등에서 '묶인' 금액
  // std::chrono::system_clock::time_point created_at{};
  std::tm created_at{};
};

// ========================
// ShardInfo (account_db.shards)
// ========================
struct ShardInfo {
  int id{};
  std::string name;
  std::string conninfo;
  // std::chrono::system_clock::time_point created_at{};
  std::tm created_at{};
};

// ========================
// Message (chatdb_N.messages)
// ========================
struct Message {
  int id{};
  long long room_id{};
  int user_id{};
  std::string content;
  // std::chrono::system_clock::time_point created_at{};
  std::tm created_at{};
};

// ========================
// ChatRoom (chatdb_N.chat_rooms)
// ========================
struct ChatRoom {
  int id{};
  std::string name;
  // std::chrono::system_clock::time_point created_at{};
  std::tm created_at{};
};

// ========================
// Transaction (account_db.transactions)
// ========================
enum class TransactionStatus : int { PENDING = 0, CONFIRMED = 1, CANCELED = 2 };

struct Transaction {
  std::string id;
  int status; // 0=PENDING, 1=CONFIRMED, 2=CANCELED
  std::tm created_at{};
};

// ========================
// Wallet (shard_db.wallets)
// ========================
struct Wallet {
  int user_id;
  int money;
  int held_money;
};

} // namespace db
// ==================================================
// SOCI type_conversion 특수화 (각 구조체를 DB ↔ C++ 매핑)
// ==================================================
namespace soci {

// User
template <> struct type_conversion<db::User> {
  typedef values base_type;

  static void from_base(values const &v, indicator /* ind */, db::User &u) {
    u.id = v.get<int>("id");
    u.username = v.get<std::string>("username");
    u.shard_id = v.get<int>("shard_id");
    if (v.get_indicator("email") == i_ok)
      u.email = v.get<std::string>("email");
    else
      u.email.reset();
    u.password_hash = v.get<std::string>("password_hash");
    u.money = v.get<int>("money", 0);
    u.held_money = v.get<int>("held_money", 0);
    u.created_at = v.get<std::tm>("created_at");
  }

  static void to_base(const db::User &u, values &v, indicator &ind) {
    v.set("id", u.id);
    v.set("username", u.username);
    v.set("shard_id", u.shard_id);
    if (u.email)
      v.set("email", *u.email);
    v.set("password_hash", u.password_hash);
    v.set("money", u.money);
    v.set("held_money", u.held_money);
    v.set("created_at", u.created_at);
    ind = i_ok;
  }
};

// ShardInfo
template <> struct type_conversion<db::ShardInfo> {
  typedef values base_type;

  static void from_base(values const &v, indicator, db::ShardInfo &s) {
    s.id = v.get<int>("id");
    s.name = v.get<std::string>("name");
    s.conninfo = v.get<std::string>("conninfo");
    s.created_at = v.get<std::tm>("created_at");
  }

  static void to_base(const db::ShardInfo &s, values &v, indicator &ind) {
    v.set("id", s.id);
    v.set("name", s.name);
    v.set("conninfo", s.conninfo);
    v.set("created_at", s.created_at);
    ind = i_ok;
  }
};

// Message
template <> struct type_conversion<db::Message> {
  typedef values base_type;

  static void from_base(values const &v, indicator, db::Message &m) {
    m.id = v.get<int>("id");
    m.room_id = v.get<long long>("room_id");
    m.user_id = v.get<int>("user_id");
    m.content = v.get<std::string>("content");
    m.created_at = v.get<std::tm>("created_at");
  }

  static void to_base(const db::Message &m, values &v, indicator &ind) {
    v.set("id", m.id);
    v.set("room_id", m.room_id);
    v.set("user_id", m.user_id);
    v.set("content", m.content);
    v.set("created_at", m.created_at);
    ind = i_ok;
  }
};

// ChatRoom
template <> struct type_conversion<db::ChatRoom> {
  typedef values base_type;

  static void from_base(values const &v, indicator, db::ChatRoom &r) {
    r.id = v.get<long long>("id");
    r.name = v.get<std::string>("name");
    r.created_at = v.get<std::tm>("created_at");
  }

  static void to_base(const db::ChatRoom &r, values &v, indicator &ind) {
    v.set("id", r.id);
    v.set("name", r.name);
    v.set("created_at", r.created_at);
    ind = i_ok;
  }
};

// Transaction
template <> struct type_conversion<db::Transaction> {
  typedef values base_type;

  static void from_base(values const &v, indicator, db::Transaction &t) {
    t.id = v.get<std::string>("id");
    t.status = v.get<int>("status");
    t.created_at = v.get<std::tm>("created_at");
  }

  static void to_base(const db::Transaction &t, values &v, indicator &ind) {
    v.set("id", t.id);
    v.set("status", t.status);
    v.set("created_at", t.created_at);
    ind = i_ok;
  }
};

// Wallet
template <> struct type_conversion<db::Wallet> {
  typedef values base_type;

  static void from_base(values const &v, indicator, db::Wallet &w) {
    w.user_id = v.get<int>("user_id");
    w.money = v.get<int>("money");
    w.held_money = v.get<int>("held_money");
  }

  static void to_base(const db::Wallet &w, values &v, indicator &ind) {
    v.set("user_id", w.user_id);
    v.set("money", w.money);
    v.set("held_money", w.held_money);
    ind = i_ok;
  }
};

} // namespace soci