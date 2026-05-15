#include <catch2/catch_test_macros.hpp>

#include "systems/session_logger_system.h"

TEST_CASE("session log frame advances from central game-loop hook", "[session_logger][issue112]") {
    SessionLog log{};

    CHECK(log.frame == 0);

    session_log_begin_frame(log);
    CHECK(log.frame == 1);

    session_log_begin_frame(log);
    CHECK(log.frame == 2);
}
