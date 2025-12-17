#pragma once
#include <string>
#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <iostream>

struct ServerConfig {
    std::string host;
    int port{};
};

struct DatabaseConfig {
    std::string host;
    int port{};
    std::string user;
    std::string password;
    std::string dbname;
};

struct RedisConfig {
    std::string url;
    int pool_size{};
};

// ✅ YAML 매핑
namespace YAML {
    template<>
    struct convert<ServerConfig> {
        static Node encode(const ServerConfig& rhs) {
            Node node;
            node["host"] = rhs.host;
            node["port"] = rhs.port;
            return node;
        }
        static bool decode(const Node& node, ServerConfig& rhs) {
            if(!node.IsMap()) return false;
            rhs.host = node["host"].as<std::string>();
            rhs.port = node["port"].as<int>();
            return true;
        }
    };

    template<>
    struct convert<DatabaseConfig> {
        static Node encode(const DatabaseConfig& rhs) {
            Node node;
            node["host"] = rhs.host;
            node["port"] = rhs.port;
            node["user"] = rhs.user;
            node["password"] = rhs.password;
            node["dbname"] = rhs.dbname;
            return node;
        }
        static bool decode(const Node& node, DatabaseConfig& rhs) {
            if(!node.IsMap()) {
                std::cout << "node is not map " << node.Type() << std::endl;
                return false;
            }
            rhs.host = node["host"].as<std::string>();
            rhs.port = node["port"].as<int>();
            rhs.user = node["user"].as<std::string>();
            rhs.password = node["password"].as<std::string>();
            rhs.dbname = node["dbname"].as<std::string>();
            return true;
        }
    };

    template<>
    struct convert<RedisConfig> {
        static Node encode(const RedisConfig& rhs) {
            Node node;
            node["url"] = rhs.url;
            node["pool_size"] = rhs.pool_size;
            return node;
        }
        static bool decode(const Node& node, RedisConfig& rhs) {
            if(!node.IsMap()) return false;
            rhs.url = node["url"].as<std::string>();
            rhs.pool_size = node["pool_size"].as<int>();
            return true;
        }
    };
}


// ✅ JSON 매핑
inline void to_json(nlohmann::json& j, const ServerConfig& v) {
    j = nlohmann::json{
        {"host", v.host}, 
        {"port", v.port}};
}

inline void from_json(const nlohmann::json& j, ServerConfig& s) {
    j.at("host").get_to(s.host);
    j.at("port").get_to(s.port);
}

inline void to_json(nlohmann::json& j, const DatabaseConfig& v) {
    j = nlohmann::json{
        {"host", v.host}, 
        {"port", v.port}, 
        {"user", v.user}, 
        {"password", v.password},
        {"dbname", v.dbname}
    };
}

inline void from_json(const nlohmann::json& j, DatabaseConfig& d) {
    j.at("host").get_to(d.host);
    j.at("port").get_to(d.port);
    j.at("user").get_to(d.user);
    j.at("password").get_to(d.password);
    j.at("dbname").get_to(d.dbname);
}

inline void to_json(nlohmann::json& j, const RedisConfig& v) {
    j = nlohmann::json{
        {"url", v.url}, 
        {"pool_size", v.pool_size}};
}

inline void from_json(const nlohmann::json& j, RedisConfig& r) {
    j.at("url").get_to(r.url);
    j.at("pool_size").get_to(r.pool_size);
}
