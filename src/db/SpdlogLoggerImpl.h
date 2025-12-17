#pragma once
#include <soci/soci.h>
#include <spdlog/spdlog.h>

class SpdlogLoggerImpl : public soci::logger_impl {
public:
    SpdlogLoggerImpl() {}
    SpdlogLoggerImpl(const SpdlogLoggerImpl&) {}   // 복사 생성자
    SpdlogLoggerImpl& operator=(const SpdlogLoggerImpl&) {
        return *this;
    }

    soci::logger_impl* do_clone() const override {
        return new SpdlogLoggerImpl(*this);
    }

    void start_query(std::string const &query) override {
        spdlog::info("SOCI query: {}", query);
    }

    void add_query_parameter(std::string name, std::string value) override {
        spdlog::info("  param {} = {}", name, value);
    }

    void clear_query_parameters() override {
        spdlog::debug("  clear query params");
    }
};