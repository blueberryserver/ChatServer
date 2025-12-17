#include "RedisClient.h"
#include <random>

namespace cache {

static sw::redis::Redis make_redis(const RedisConfig& cfg) {
    using namespace sw::redis;

    // URL 우선. url이 설정되어 있으면 그대로 사용.
    if (!cfg.url.empty()) {
        // 풀 옵션 설정 위해 객체 생성자 2-arg 버전 사용
        ConnectionOptions copts;
        if (!cfg.host.empty() && cfg.port > 0) {
            copts.host = cfg.host;
            copts.port = cfg.port;
        }
        // url이 있으면 sw::redis::Redis(url) 생성도 가능하지만,
        // pool 옵션을 지정하려면 opts+pool로 가는게 일반적.
        // 여기서는 host/port 우선 구성 + 필요시 password/db 적용.
        if (!cfg.password.empty()) copts.password = cfg.password;
        if (cfg.db >= 0) copts.db = cfg.db;
        copts.socket_timeout = cfg.socket_timeout;

        ConnectionPoolOptions popts;
        popts.size = cfg.pool_size;
        popts.wait_timeout = cfg.pool_wait;
        popts.connection_lifetime = cfg.connection_lifetime;

        if (!cfg.host.empty() && cfg.port > 0) {
            return Redis(copts, popts);
        } else {
            // url만 있는 경우: url 문자열로 직접 생성
            // (pool 옵션도 쓰고 싶다면 상단처럼 copts/popts 경로 권장)
            return Redis(cfg.url);
        }
    }

    // URL이 비어 있으면 host/port 기반
    sw::redis::ConnectionOptions opts;
    if (!cfg.host.empty()) opts.host = cfg.host; else opts.host = "127.0.0.1";
    if (cfg.port > 0) opts.port = cfg.port; else opts.port = 6379;
    if (!cfg.password.empty()) opts.password = cfg.password;
    if (cfg.db >= 0) opts.db = cfg.db;
    opts.socket_timeout = cfg.socket_timeout;

    sw::redis::ConnectionPoolOptions popts;
    popts.size = cfg.pool_size;
    popts.wait_timeout = cfg.pool_wait;
    popts.connection_lifetime = cfg.connection_lifetime;

    return sw::redis::Redis(opts, popts);
}

RedisClient::RedisClient(const RedisConfig& cfg)
    : redis_(make_redis(cfg)) {}

bool RedisClient::Set(const std::string& key, const std::string& value,
                      std::optional<std::chrono::seconds> ttl) {
    try {
        if (ttl && ttl->count() > 0) {
            redis_.set(key, value, *ttl);
        } else {
            redis_.set(key, value);
        }
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::string> RedisClient::Get(const std::string& key) {
    try {
        auto v = redis_.get(key);
        if (v) return *v;
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

void RedisClient::Del(const std::string& key) {
    try { redis_.del(key); } catch (...) {}
}

bool RedisClient::AcquireLock(const std::string& lockKey, std::chrono::seconds ttl) {
    try {
        // SET key "1" NX EX ttl
        return redis_.set(lockKey, "1", ttl, sw::redis::UpdateType::NOT_EXIST);
    } catch (...) {
        return false;
    }
}

void RedisClient::ReleaseLock(const std::string& lockKey) {
    try { redis_.del(lockKey); } catch (...) {}
}

std::optional<std::chrono::seconds> RedisClient::jitteredTTL(
    std::chrono::seconds ttl_base,
    std::optional<std::chrono::seconds> jitter
) {
    if (!jitter) return ttl_base;
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(-jitter->count(), jitter->count());
    auto s = ttl_base.count() + dist(rng);
    if (s < 10) s = 10;
    return std::chrono::seconds{s};
}

RedisClusterClient::RedisClusterClient(const RedisClusterConfig& cfg) {
    using namespace sw::redis;

    ConnectionOptions opts;
    opts.socket_timeout = cfg.socket_timeout;
    if (!cfg.password.empty()) opts.password = cfg.password;

    // cluster 노드 목록 지정
    cluster_ = std::make_unique<RedisCluster>(cfg.nodes);
}

bool RedisClusterClient::Set(const std::string& key, const std::string& value,
                             std::optional<std::chrono::seconds> ttl) {
    try {
        if (ttl && ttl->count() > 0)
            cluster_->set(key, value, *ttl);
        else
            cluster_->set(key, value);
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<std::string> RedisClusterClient::Get(const std::string& key) {
    try {
        auto v = cluster_->get(key);
        if (v) return *v;
    } catch (...) {}
    return std::nullopt;
}

void RedisClusterClient::Del(const std::string& key) {
    try { cluster_->del(key); } catch (...) {}
}

} // namespace cache