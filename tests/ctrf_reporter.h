#pragma once

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>

#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

static std::string ctrf_json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static int64_t ctrf_now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

struct CtrfTestResult {
    std::string name;
    std::string status;
    int64_t     duration_ms = 0;
    int64_t     start_ms = 0;
    int64_t     stop_ms = 0;
    std::string message;
    std::vector<std::string> tags;
};

class CtrfReporter : public Catch::EventListenerBase {
public:
    using EventListenerBase::EventListenerBase;

    static std::string getDescription() {
        return "Outputs test results in CTRF JSON format to ctrf-report.json";
    }

    void testRunStarting(Catch::TestRunInfo const&) override {
        run_start_ = ctrf_now_ms();
        results_.clear();
    }

    void testCaseStarting(Catch::TestCaseInfo const& info) override {
        current_ = CtrfTestResult{};
        current_.name = info.name;
        for (auto const& tag : info.tags) {
            current_.tags.push_back(std::string(tag.original.data(), tag.original.size()));
        }
        current_.start_ms = ctrf_now_ms();
    }

    void testCaseEnded(Catch::TestCaseStats const& stats) override {
        current_.stop_ms = ctrf_now_ms();
        current_.duration_ms = current_.stop_ms - current_.start_ms;

        if (stats.totals.testCases.skipped > 0) {
            current_.status = "skipped";
        } else if (stats.totals.testCases.failed > 0) {
            current_.status = "failed";
        } else {
            current_.status = "passed";
        }

        results_.push_back(current_);
    }

    void assertionEnded(Catch::AssertionStats const& stats) override {
        if (!stats.assertionResult.isOk()) {
            auto const& result = stats.assertionResult;
            std::string msg;
            if (result.hasExpression()) {
                msg += result.getExpression();
            }
            if (result.hasExpandedExpression()) {
                msg += " expanded to: ";
                msg += result.getExpandedExpression();
            }
            auto const& src = result.getSourceInfo();
            msg += " at ";
            msg += src.file;
            msg += ":";
            msg += std::to_string(src.line);

            if (!current_.message.empty()) current_.message += "; ";
            current_.message += msg;
        }
    }

    void testRunEnded(Catch::TestRunStats const&) override {
        int64_t run_stop = ctrf_now_ms();

        int passed = 0, failed = 0, skipped = 0, other = 0;
        for (auto const& r : results_) {
            if (r.status == "passed")       ++passed;
            else if (r.status == "failed")  ++failed;
            else if (r.status == "skipped") ++skipped;
            else                            ++other;
        }

        std::ofstream out("ctrf-report.json");
        out << "{\n";
        out << "  \"reportFormat\": \"CTRF\",\n";
        out << "  \"specVersion\": \"0.0.1\",\n";
        out << "  \"results\": {\n";
        out << "    \"tool\": {\n";
        out << "      \"name\": \"catch2\",\n";
        out << "      \"version\": \"3.7.1\"\n";
        out << "    },\n";
        out << "    \"summary\": {\n";
        out << "      \"tests\": "   << static_cast<int>(results_.size()) << ",\n";
        out << "      \"passed\": "  << passed  << ",\n";
        out << "      \"failed\": "  << failed  << ",\n";
        out << "      \"skipped\": " << skipped << ",\n";
        out << "      \"pending\": 0,\n";
        out << "      \"other\": "   << other   << ",\n";
        out << "      \"start\": "   << run_start_ << ",\n";
        out << "      \"stop\": "    << run_stop   << "\n";
        out << "    },\n";
        out << "    \"tests\": [\n";
        for (size_t i = 0; i < results_.size(); ++i) {
            auto const& t = results_[i];
            out << "      {\n";
            out << "        \"name\": \""     << ctrf_json_escape(t.name)   << "\",\n";
            out << "        \"status\": \""   << t.status              << "\",\n";
            out << "        \"duration\": "   << t.duration_ms         << ",\n";
            out << "        \"start\": "      << t.start_ms            << ",\n";
            out << "        \"stop\": "       << t.stop_ms;
            if (!t.message.empty()) {
                out << ",\n        \"message\": \"" << ctrf_json_escape(t.message) << "\"";
            }
            if (!t.tags.empty()) {
                out << ",\n        \"tags\": [";
                for (size_t j = 0; j < t.tags.size(); ++j) {
                    out << "\"" << ctrf_json_escape(t.tags[j]) << "\"";
                    if (j + 1 < t.tags.size()) out << ", ";
                }
                out << "]";
            }
            out << "\n      }";
            if (i + 1 < results_.size()) out << ",";
            out << "\n";
        }
        out << "    ]\n";
        out << "  }\n";
        out << "}\n";
        out.close();
    }

private:
    int64_t run_start_ = 0;
    CtrfTestResult current_;
    std::vector<CtrfTestResult> results_;
};

CATCH_REGISTER_LISTENER(CtrfReporter)
