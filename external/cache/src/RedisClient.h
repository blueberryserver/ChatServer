#pragma once
#include <sw/redis++/redis++.h>
#include <optional>
#include <string>
#include <chrono>
#include <functional>
#include <random>
#include <vector>
#include <unordered_map>
#include <utility>

namespace cache {

struct RedisConfig {
    // 기본 tcp url: "tcp://127.0.0.1:6379"
    std::string url = "tcp://127.0.0.1:6379";
    // 풀 크기, 타임아웃 등은 ConnectionOptions/PoolOptions로 커스터마이즈
    std::chrono::milliseconds socket_timeout{200};
    std::size_t pool_size = 6;
    std::chrono::milliseconds pool_wait{100};
    std::chrono::minutes connection_lifetime{10};

    // 편의상 비밀번호/DB 등을 URL이 아닌 개별 옵션으로도 지원
    std::string host;     // ex) "127.0.0.1"
    int         port = 0; // ex) 6379
    std::string password;
    int         db = -1;  // -1이면 설정하지 않음
};

class RedisClient {
public:
    explicit RedisClient(const RedisConfig& cfg);

    // -----------------------------
    // Get/Set APIs
    // -----------------------------
    bool Set(const std::string& key, const std::string& value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);
    std::optional<std::string> Get(const std::string& key);
    void Del(const std::string& key);

    // 캐시-어사이드: miss면 loader()로 로드 후 캐시에 set
    template <typename T, typename Loader, typename Encoder, typename Decoder>
    std::optional<T> GetOrLoadJson(
        const std::string& key,
        std::chrono::seconds ttl_base,
        Loader&& loader,   // () -> std::optional<T>
        Encoder&& encode,  // (const T&) -> std::string(JSON)
        Decoder&& decode,  // (const std::string&) -> T
        std::optional<std::chrono::seconds> ttl_jitter = std::nullopt
    ) {
        auto v = Get(key);
        if (v) {
            try {
                return std::optional<T>(decode(*v));
            } catch (...) {
                // 파싱 실패 시 캐시 무효화
                Del(key);
            }
        }

        // miss: 원천 조회
        auto loaded = loader();
        if (!loaded) return std::nullopt;

        auto ttl = jitteredTTL(ttl_base, ttl_jitter);
        try {
            auto json = encode(*loaded);
            Set(key, json, ttl);
        } catch (...) {
            // 캐시에 못 넣어도 기능은 계속
        }
        return loaded;
    }

    // write-through: 원천 write 성공 후 캐시에 반영
    template <typename T, typename Writer, typename Encoder>
    bool WriteThroughJson(
        const std::string& key,
        const T& value,
        Writer&& writer,   // (const T&) -> bool
        Encoder&& encode,  // (const T&) -> std::string(JSON)
        std::chrono::seconds ttl = std::chrono::seconds(600)
    ) {
        if (!writer(value)) return false;
        try {
            auto json = encode(value);
            Set(key, json, ttl);
        } catch (...) {}
        return true;
    }

    // -----------------------------
    // List APIs
    // -----------------------------
    bool LPush(const std::string& key, const std::string& value) {
        try { redis_.lpush(key, value); return true; } catch (...) { return false; }
    }
    bool RPush(const std::string& key, const std::string& value) {
        try { redis_.rpush(key, value); return true; } catch (...) { return false; }
    }
    std::optional<std::string> LPop(const std::string& key) {
        try {
            auto v = redis_.lpop(key);
            if (v) return std::optional<std::string>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }
    std::optional<std::string> RPop(const std::string& key) {
        try {
            auto v = redis_.rpop(key);
            if (v) return std::optional<std::string>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }
    std::vector<std::string> LRange(const std::string& key, long long start, long long stop) {
        std::vector<std::string> out;
        try { redis_.lrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    long long LLen(const std::string& key) {
        try { return redis_.llen(key); } catch (...) { return 0; }
    }
    long long LRem(const std::string& key, long long count, const std::string& value) {
        try { return redis_.lrem(key, count, value); } catch (...) { return 0; }
    }
    bool LTrim(const std::string& key, long long start, long long stop) {
        try { redis_.ltrim(key, start, stop); return true; } catch (...) { return false; }
    }

    // -----------------------------
    // Hash APIs
    // -----------------------------
    bool HSet(const std::string& key, const std::string& field, const std::string& value) {
        try { redis_.hset(key, field, value); return true; } catch (...) { return false; }
    }
    std::optional<std::string> HGet(const std::string& key, const std::string& field) {
        try {
            auto v = redis_.hget(key, field);
            if (v) return std::optional<std::string>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }
    long long HDel(const std::string& key, const std::vector<std::string>& fields) {
        try { return redis_.hdel(key, fields.begin(), fields.end()); } catch (...) { return 0; }
    }
    std::unordered_map<std::string, std::string> HGetAll(const std::string& key) {
        std::unordered_map<std::string, std::string> out;
        try { redis_.hgetall(key, std::inserter(out, out.end())); } catch (...) {}
        return out;
    }
    bool HExists(const std::string& key, const std::string& field) {
        try { return redis_.hexists(key, field); } catch (...) { return false; }
    }
    long long HIncrBy(const std::string& key, const std::string& field, long long increment) {
        try { return redis_.hincrby(key, field, increment); } catch (...) { return 0; }
    }
    long long HLen(const std::string& key) {
        try { return redis_.hlen(key); } catch (...) { return 0; }
    }
    bool HMSet(const std::string& key, const std::unordered_map<std::string, std::string>& kvs) {
        try { redis_.hmset(key, kvs.begin(), kvs.end()); return true; } catch (...) { return false; }
    }
    std::vector<std::optional<std::string>> HMGet(const std::string& key, const std::vector<std::string>& fields) {
        std::vector<sw::redis::OptionalString> tmp;
        std::vector<std::optional<std::string>> out;
        try {
            redis_.hmget(key, fields.begin(), fields.end(), std::back_inserter(tmp));
            out.reserve(tmp.size());
            for (auto& it : tmp) {
                if (it) out.emplace_back(*it);
                else out.emplace_back(std::nullopt);
            }
        } catch (...) {}
        return out;
    }

    // -----------------------------
    // ZSet APIs
    // -----------------------------
    // ZADD
    long long ZAdd(const std::string& key, const std::string& member, double score) {
        try { return redis_.zadd(key, member, score); } catch (...) { return 0; }
    }
    long long ZAdd(const std::string& key, const std::vector<std::pair<std::string,double>>& ms) {
        try { return redis_.zadd(key, ms.begin(), ms.end()); } catch (...) { return 0; }
    }

    // ZREM
    long long ZRem(const std::string& key, const std::vector<std::string>& members) {
        try { return redis_.zrem(key, members.begin(), members.end()); } catch (...) { return 0; }
    }

    // ZCARD
    long long ZCard(const std::string& key) {
        try { return redis_.zcard(key); } catch (...) { return 0; }
    }

    // ZSCORE
    std::optional<double> ZScore(const std::string& key, const std::string& member) {
        try {
            auto v = redis_.zscore(key, member);
            if (v) return std::optional<double>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }

    // ZINCRBY
    double ZIncrBy(const std::string& key, double increment, const std::string& member) {
        try { return redis_.zincrby(key, increment, member); } catch (...) { return 0.0; }
    }

    // ZRANGE & ZREVRANGE (members only)
    std::vector<std::string> ZRange(const std::string& key, long long start, long long stop) {
        std::vector<std::string> out; try { redis_.zrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    std::vector<std::string> ZRevRange(const std::string& key, long long start, long long stop) {
        std::vector<std::string> out; try { redis_.zrevrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }

    // ZRANGE & ZREVRANGE with scores
    std::vector<std::pair<std::string,double>> ZRangeWithScores(const std::string& key, long long start, long long stop) {
        std::vector<std::pair<std::string,double>> out; try { redis_.zrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    std::vector<std::pair<std::string,double>> ZRevRangeWithScores(const std::string& key, long long start, long long stop) {
        std::vector<std::pair<std::string,double>> out; try { redis_.zrevrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }

    // ZRANGEBYSCORE (members only, interval API)
    std::vector<std::string> ZRangeByScore(const std::string& key, double min_score, double max_score) {
        std::vector<std::string> out;
        try {
            sw::redis::BoundedInterval<double> interval(min_score, max_score, sw::redis::BoundType::CLOSED);
            redis_.zrangebyscore(key, interval, std::back_inserter(out));
        } catch (...) {}
        return out;
    }
    // ZRANGEBYSCORE with scores (interval API)
    std::vector<std::pair<std::string,double>> ZRangeByScoreWithScores(const std::string& key, double min_score, double max_score) {
        std::vector<std::pair<std::string,double>> out;
        try {
            sw::redis::BoundedInterval<double> interval(min_score, max_score, sw::redis::BoundType::CLOSED);
            redis_.zrangebyscore(key, interval, std::back_inserter(out));
        } catch (...) {}
        return out;
    }

    // ZREMRANGEBYSCORE
    long long ZRemRangeByScore(const std::string& key, double min_score, double max_score) {
        try { 
            sw::redis::BoundedInterval<double> interval(min_score, max_score, sw::redis::BoundType::CLOSED);
            return redis_.zremrangebyscore(key, interval); 
        } catch (...) { return 0; }
    }

    // ZRANK / ZREVRANK
    std::optional<long long> ZRank(const std::string& key, const std::string& member) {
        try {
            auto v = redis_.zrank(key, member);
            if (v) return std::optional<long long>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }
    std::optional<long long> ZRevRank(const std::string& key, const std::string& member) {
        try {
            auto v = redis_.zrevrank(key, member);
            if (v) return std::optional<long long>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }

    // ZPOPMAX / ZPOPMIN (1개 pop)
    std::optional<std::vector<std::pair<std::string,double>>> ZPopMax(const std::string& key, long long count) {
        try {
            std::vector<std::pair<std::string,double>> out;
            redis_.zpopmax(key, count, std::back_inserter(out));            
            return out;
        } catch (...) { return {}; }
    }
    std::optional<std::vector<std::pair<std::string,double>>> ZPopMin(const std::string& key, long long count) {
        try {
            std::vector<std::pair<std::string,double>> out;
            redis_.zpopmin(key, count, std::back_inserter(out));
            return out;
        } catch (...) { return {}; }
    }

    // 분산락 (간단 버전)
    bool AcquireLock(const std::string& lockKey, std::chrono::seconds ttl);
    void ReleaseLock(const std::string& lockKey);

private:
    sw::redis::Redis redis_;

    static std::optional<std::chrono::seconds> jitteredTTL(
        std::chrono::seconds ttl_base,
        std::optional<std::chrono::seconds> jitter
    );
};

struct RedisClusterConfig {
    // 콤마로 구분된 노드 주소들 "tcp://127.0.0.1:7000,127.0.0.1:7001";
    std::string nodes = "tcp://127.0.0.1:7000";
    std::chrono::milliseconds socket_timeout{200};
    std::string password;
};

class RedisClusterClient {
public:
    explicit RedisClusterClient(const RedisClusterConfig& cfg);

    bool Set(const std::string& key, const std::string& value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);
    std::optional<std::string> Get(const std::string& key);
    void Del(const std::string& key);

    bool LPush(const std::string& key, const std::string& value) {
        try { cluster_->lpush(key, value); return true; } catch (...) { return false; }
    }
    bool RPush(const std::string& key, const std::string& value) {
        try { cluster_->rpush(key, value); return true; } catch (...) { return false; }
    }
    std::optional<std::string> LPop(const std::string& key) {
        try {
            auto v = cluster_->lpop(key);
            if (v) return std::optional<std::string>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }
    std::optional<std::string> RPop(const std::string& key) {
        try {
            auto v = cluster_->rpop(key);
            if (v) return std::optional<std::string>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }
    std::vector<std::string> LRange(const std::string& key, long long start, long long stop) {
        std::vector<std::string> out;
        try { cluster_->lrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    long long LLen(const std::string& key) {
        try { return cluster_->llen(key); } catch (...) { return 0; }
    }
    long long LRem(const std::string& key, long long count, const std::string& value) {
        try { return cluster_->lrem(key, count, value); } catch (...) { return 0; }
    }
    bool LTrim(const std::string& key, long long start, long long stop) {
        try { cluster_->ltrim(key, start, stop); return true; } catch (...) { return false; }
    }

    bool HSet(const std::string& key, const std::string& field, const std::string& value) {
        try { cluster_->hset(key, field, value); return true; } catch (...) { return false; }
    }
    std::optional<std::string> HGet(const std::string& key, const std::string& field) {
        try {
            auto v = cluster_->hget(key, field);
            if (v) return std::optional<std::string>(*v);
            return std::nullopt;
        } catch (...) { return std::nullopt; }
    }
    long long HDel(const std::string& key, const std::vector<std::string>& fields) {
        try { return cluster_->hdel(key, fields.begin(), fields.end()); } catch (...) { return 0; }
    }
    std::unordered_map<std::string, std::string> HGetAll(const std::string& key) {
        std::unordered_map<std::string, std::string> out;
        try { cluster_->hgetall(key, std::inserter(out, out.end())); } catch (...) {}
        return out;
    }
    bool HExists(const std::string& key, const std::string& field) {
        try { return cluster_->hexists(key, field); } catch (...) { return false; }
    }
    long long HIncrBy(const std::string& key, const std::string& field, long long increment) {
        try { return cluster_->hincrby(key, field, increment); } catch (...) { return 0; }
    }
    long long HLen(const std::string& key) {
        try { return cluster_->hlen(key); } catch (...) { return 0; }
    }
    bool HMSet(const std::string& key, const std::unordered_map<std::string, std::string>& kvs) {
        try { cluster_->hmset(key, kvs.begin(), kvs.end()); return true; } catch (...) { return false; }
    }
    std::vector<std::optional<std::string>> HMGet(const std::string& key, const std::vector<std::string>& fields) {
        std::vector<sw::redis::OptionalString> tmp;
        std::vector<std::optional<std::string>> out;
        try {
            cluster_->hmget(key, fields.begin(), fields.end(), std::back_inserter(tmp));
            out.reserve(tmp.size());
            for (auto& it : tmp) {
                if (it) out.emplace_back(*it);
                else out.emplace_back(std::nullopt);
            }
        } catch (...) {}
        return out;
    }
        // -----------------------------
    // ZSet APIs (Cluster)
    // -----------------------------
    long long ZAdd(const std::string& key, const std::string& member, double score) {
        try { return cluster_->zadd(key, member, score); } catch (...) { return 0; }
    }
    long long ZAdd(const std::string& key, const std::vector<std::pair<std::string,double>>& ms) {
        try { return cluster_->zadd(key, ms.begin(), ms.end()); } catch (...) { return 0; }
    }
    long long ZRem(const std::string& key, const std::vector<std::string>& members) {
        try { return cluster_->zrem(key, members.begin(), members.end()); } catch (...) { return 0; }
    }
    long long ZCard(const std::string& key) {
        try { return cluster_->zcard(key); } catch (...) { return 0; }
    }
    std::optional<double> ZScore(const std::string& key, const std::string& member) {
        try { auto v = cluster_->zscore(key, member); if (v) return std::optional<double>(*v); return std::nullopt; } catch (...) { return std::nullopt; }
    }
    double ZIncrBy(const std::string& key, double increment, const std::string& member) {
        try { return cluster_->zincrby(key, increment, member); } catch (...) { return 0.0; }
    }
    std::vector<std::string> ZRange(const std::string& key, long long start, long long stop) {
        std::vector<std::string> out; try { cluster_->zrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    std::vector<std::string> ZRevRange(const std::string& key, long long start, long long stop) {
        std::vector<std::string> out; try { cluster_->zrevrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    std::vector<std::pair<std::string,double>> ZRangeWithScores(const std::string& key, long long start, long long stop) {
        std::vector<std::pair<std::string,double>> out; try { cluster_->zrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    std::vector<std::pair<std::string,double>> ZRevRangeWithScores(const std::string& key, long long start, long long stop) {
        std::vector<std::pair<std::string,double>> out; try { cluster_->zrevrange(key, start, stop, std::back_inserter(out)); } catch (...) {}
        return out;
    }
    std::vector<std::string> ZRangeByScore(const std::string& key, double min_score, double max_score) {
        std::vector<std::string> out;
        try {
            sw::redis::BoundedInterval<double> interval(min_score, max_score, sw::redis::BoundType::CLOSED);
            cluster_->zrangebyscore(key, interval, std::back_inserter(out));
        } catch (...) {}
        return out;
    }
    std::vector<std::pair<std::string,double>> ZRangeByScoreWithScores(const std::string& key, double min_score, double max_score) {
        std::vector<std::pair<std::string,double>> out;
        try {
            sw::redis::BoundedInterval<double> interval(min_score, max_score, sw::redis::BoundType::CLOSED);
            cluster_->zrangebyscore(key, interval, std::back_inserter(out));
        } catch (...) {}
        return out;
    }
    long long ZRemRangeByScore(const std::string& key, double min_score, double max_score) {
        try {
            sw::redis::BoundedInterval<double> interval(min_score, max_score, sw::redis::BoundType::CLOSED);
            return cluster_->zremrangebyscore(key, interval); 
        } catch (...) { return 0; }
    }
    std::optional<long long> ZRank(const std::string& key, const std::string& member) {
        try { auto v = cluster_->zrank(key, member); if (v) return std::optional<long long>(*v); return std::nullopt; } catch (...) { return std::nullopt; }
    }
    std::optional<long long> ZRevRank(const std::string& key, const std::string& member) {
        try { auto v = cluster_->zrevrank(key, member); if (v) return std::optional<long long>(*v); return std::nullopt; } catch (...) { return std::nullopt; }
    }
    std::optional<std::vector<std::pair<std::string,double>>> ZPopMax(const std::string& key, long long count) {
        try { 
            std::vector<std::pair<std::string,double>> out; 
            cluster_->zpopmax(key, count, std::back_inserter(out)); 
            return out; 
        } catch (...) { return {}; }
    }
    std::optional<std::vector<std::pair<std::string,double>>> ZPopMin(const std::string& key, long long count) {
        try { 
            std::vector<std::pair<std::string,double>> out; 
            cluster_->zpopmin(key, count, std::back_inserter(out)); 
            return out; 
        } catch (...) { return {}; }
    }


private:
    std::unique_ptr<sw::redis::RedisCluster> cluster_;
};

} // namespace cache