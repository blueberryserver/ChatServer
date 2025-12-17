// src/db/DbRouter.cpp
#include "DbRouter.h"
#include <iostream>
#include <spdlog/spdlog.h>

DbRouter::DbRouter(const std::string& account_conninfo)
    : account_(account_conninfo) {}

std::optional<db::User> DbRouter::getUser(const std::string& username) {
    return account_.getUser(username);
}

std::shared_ptr<ShardDb> DbRouter::getShardForUser(int user_id) {
    int shard_id = account_.getShardId(user_id);
    if (shard_id < 0) {
        SPDLOG_WARN("Invalid shard_id for user {}", user_id);
        return nullptr;
    }
    auto info = account_.getShardInfo(shard_id);
    if (!info) {
        SPDLOG_ERROR("Shard not found for user {}", user_id);
        return nullptr;
    }
    //SPDLOG_INFO("Shard Info: {}", info->conninfo);
    return std::make_shared<ShardDb>(info->conninfo);
}

std::shared_ptr<AccountDb> DbRouter::getAccountDb() {
    return std::shared_ptr<AccountDb>(&account_, [](AccountDb*){});
}