#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace config {

class ConfigManager {
public:
    enum class Format { None, JSON, YAML };

    bool load(const std::string& filename) {
        format_ = Format::None;
        jsonConfig_ = nlohmann::json{};
        yamlConfig_ = YAML::Node();

        const auto ext = getFileExtension(filename);
        if (ext == "json") {
            std::ifstream ifs(filename);
            if (!ifs) return false;
            try {
                jsonConfig_ = nlohmann::json::parse(ifs);
                format_ = Format::JSON;
                return true;
            } catch (...) { return false; }
        } else if (ext == "yaml" || ext == "yml") {
            try {
                yamlConfig_ = YAML::LoadFile(filename);
                format_ = Format::YAML;
                return true;
            } catch (...) { return false; }
        }
        return false;
    }

    Format format() const { return format_; }

    // 존재 여부
    bool has(const std::string& key) const {
        if (format_ == Format::JSON) {
            const nlohmann::json* node = getJsonNodeByPath(key);
            return node != nullptr && !node->is_null();
        } else if (format_ == Format::YAML) {
            YAML::Node node = getYamlNodeByPath(key);
            return static_cast<bool>(node);
        }
        return false;
    }

    // 값 가져오기 (기본값 지원)
    template<typename T>
    T get(const std::string& key, const T& defaultValue = T()) const {
        try {
            if (format_ == Format::JSON) {
                const nlohmann::json* node = getJsonNodeByPath(key);
                if (!node || node->is_null()) return defaultValue;
                return node->get<T>();
            } else if (format_ == Format::YAML) {
                YAML::Node node = getYamlNodeByPath(key);
                if (!node) return defaultValue;
                return node.as<T>();
            }
        } catch (...) {}
        return defaultValue;
    }

    // 구조체 매핑 (JSON: from_json, YAML: YAML::convert<T> 필요)
    template<typename T>
    T getStruct(const std::string& key) const {
        if (format_ == Format::JSON) {
            const nlohmann::json* node = getJsonNodeByPath(key);
            if (!node || node->is_null()) return T{};
            return node->get<T>();
        } else if (format_ == Format::YAML) {
            YAML::Node node = getYamlNodeByPath(key);
            if (!node) return T{};
            return node.as<T>();
        }
        return T{};
    }

private:
    Format format_ { Format::None };
    nlohmann::json jsonConfig_;
    YAML::Node     yamlConfig_;

    static std::string getFileExtension(const std::string& filename) {
        const auto pos = filename.find_last_of('.');
        if (pos == std::string::npos) return {};
        std::string ext = filename.substr(pos + 1);
        for (auto& c : ext) c = static_cast<char>(::tolower(c));
        return ext;
    }

    static std::vector<std::string> splitKey(const std::string& key) {
        std::vector<std::string> parts;
        size_t start = 0;
        while (start < key.size()) {
            size_t dot = key.find('.', start);
            if (dot == std::string::npos) {
                parts.emplace_back(key.substr(start));
                break;
            }
            parts.emplace_back(key.substr(start, dot - start));
            start = dot + 1;
        }
        return parts;
    }

    const nlohmann::json* getJsonNodeByPath(const std::string& key) const {
        auto parts = splitKey(key);
        const nlohmann::json* node = &jsonConfig_;
        for (const auto& p : parts) {
            if (!node->contains(p)) return nullptr;
            node = &(*node)[p];
        }
        return node;
    }

    YAML::Node getYamlNodeByPath(const std::string& key) const {
        auto parts = splitKey(key);
        YAML::Node cur = YAML::Clone(yamlConfig_);
        for (const auto& p : parts) {
            const YAML::Node& ccur = cur;
            YAML::Node next = ccur[p];   // 존재하지 않으면 삽입하지 않고 null 반환
            if (!next) {
                return YAML::Node(); // null
            }
            cur = next;
        }
        return cur;
    }
};

} // namespace config