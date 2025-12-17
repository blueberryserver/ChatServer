// src/db/DbRouter.h
#pragma once
#include "AccountDb.h"
#include "ShardDb.h"
#include <memory>
#include <optional>

class DbRouter {
public:
    explicit DbRouter(const std::string& account_conninfo);

    std::optional<db::User> getUser(const std::string& username);
    std::shared_ptr<ShardDb> getShardForUser(int user_id);
    std::shared_ptr<AccountDb> getAccountDb();
    
private:
    AccountDb account_;
};