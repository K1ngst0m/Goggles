#include <catch2/catch_test_macros.hpp>
#include <util/diagnostics/diagnostic_event.hpp>
#include <util/diagnostics/diagnostic_policy.hpp>

using namespace goggles::diagnostics;

TEST_CASE("Severity ordering", "[diagnostics][event]") {
    REQUIRE(static_cast<uint8_t>(Severity::debug) < static_cast<uint8_t>(Severity::info));
    REQUIRE(static_cast<uint8_t>(Severity::info) < static_cast<uint8_t>(Severity::warning));
    REQUIRE(static_cast<uint8_t>(Severity::warning) < static_cast<uint8_t>(Severity::error));
}

TEST_CASE("LocalizationKey CHAIN_LEVEL sentinel", "[diagnostics][event]") {
    REQUIRE(LocalizationKey::CHAIN_LEVEL == UINT32_MAX);

    LocalizationKey key;
    REQUIRE(key.pass_ordinal == LocalizationKey::CHAIN_LEVEL);
}

TEST_CASE("DiagnosticEvent construction with each evidence variant", "[diagnostics][event]") {
    SECTION("monostate default") {
        DiagnosticEvent event;
        REQUIRE(std::holds_alternative<std::monostate>(event.evidence));
    }

    SECTION("BindingEvidence") {
        DiagnosticEvent event;
        event.evidence = BindingEvidence{.resource_id = "tex0",
                                         .is_fallback = true,
                                         .width = 0,
                                         .height = 0,
                                         .format = 0,
                                         .producer_pass = LocalizationKey::CHAIN_LEVEL,
                                         .alias_name = {}};
        REQUIRE(std::holds_alternative<BindingEvidence>(event.evidence));
        REQUIRE(std::get<BindingEvidence>(event.evidence).is_fallback);
    }

    SECTION("SemanticEvidence") {
        DiagnosticEvent event;
        event.evidence = SemanticEvidence{
            .member_name = "MVP", .classification = "semantic", .value = 0.0f, .offset = 0};
        REQUIRE(std::holds_alternative<SemanticEvidence>(event.evidence));
    }

    SECTION("CompileEvidence") {
        DiagnosticEvent event;
        event.evidence = CompileEvidence{.stage = "vertex",
                                         .success = true,
                                         .messages = {},
                                         .timing_us = 0.0,
                                         .cache_hit = false};
        REQUIRE(std::holds_alternative<CompileEvidence>(event.evidence));
    }

    SECTION("ReflectionEvidence") {
        DiagnosticEvent event;
        event.evidence =
            ReflectionEvidence{.stage = "fragment", .resource_summary = {}, .merge_conflicts = {}};
        REQUIRE(std::holds_alternative<ReflectionEvidence>(event.evidence));
    }

    SECTION("ProvenanceEvidence") {
        DiagnosticEvent event;
        event.evidence = ProvenanceEvidence{.original_file = "test.slang",
                                            .original_line = 42,
                                            .rewrite_applied = false,
                                            .rewrite_description = {}};
        REQUIRE(std::holds_alternative<ProvenanceEvidence>(event.evidence));
    }

    SECTION("CaptureEvidence") {
        DiagnosticEvent event;
        event.evidence = CaptureEvidence{.pass_ordinal = 3, .frame_index = 10, .image_ref = {}};
        REQUIRE(std::holds_alternative<CaptureEvidence>(event.evidence));
    }
}

TEST_CASE("DiagnosticPolicy defaults are compatibility mode", "[diagnostics][policy]") {
    DiagnosticPolicy policy;
    REQUIRE(policy.mode == PolicyMode::compatibility);
    REQUIRE(policy.capture_mode == CaptureMode::standard);
    REQUIRE(policy.tier == ActivationTier::tier0);
    REQUIRE(policy.gpu_timestamp_availability == GpuTimestampAvailabilityMode::auto_detect);
    REQUIRE_FALSE(policy.promote_fallback_to_error);
    REQUIRE_FALSE(policy.reflection_loss_is_fatal);
}

TEST_CASE("Strict policy sets promotion flags", "[diagnostics][policy]") {
    auto policy = make_strict_policy();
    REQUIRE(policy.mode == PolicyMode::strict);
    REQUIRE(policy.promote_fallback_to_error);
    REQUIRE(policy.reflection_loss_is_fatal);
}

TEST_CASE("ActivationTier ordering", "[diagnostics][policy]") {
    REQUIRE(static_cast<uint8_t>(ActivationTier::tier0) <
            static_cast<uint8_t>(ActivationTier::tier1));
    REQUIRE(static_cast<uint8_t>(ActivationTier::tier1) <
            static_cast<uint8_t>(ActivationTier::tier2));
}

TEST_CASE("AuthoringVerdict construction", "[diagnostics][event]") {
    AuthoringVerdict verdict;
    REQUIRE(verdict.result == VerdictResult::pass);
    REQUIRE(verdict.findings.empty());

    verdict.result = VerdictResult::degraded;
    verdict.findings.push_back(AuthoringFinding{
        .severity = Severity::warning, .localization = {}, .message = "reflection loss"});
    REQUIRE(verdict.findings.size() == 1);
}
