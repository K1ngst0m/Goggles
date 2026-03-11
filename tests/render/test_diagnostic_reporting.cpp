#include <catch2/catch_test_macros.hpp>
#include <util/diagnostics/diagnostic_report.hpp>

using namespace goggles::diagnostics;

namespace {

auto make_session(CaptureMode mode) -> std::unique_ptr<DiagnosticSession> {
    DiagnosticPolicy policy{};
    policy.capture_mode = mode;
    auto session = DiagnosticSession::create(policy);

    SessionIdentity identity{};
    identity.generation_id = 17;
    identity.capture_mode = "reporting";
    session->update_identity(std::move(identity));

    auto manifest = std::make_unique<ChainManifest>();
    ManifestPassEntry pass{};
    pass.ordinal = 2;
    pass.shader_path = "probe.slang";
    manifest->add_pass(pass);
    session->set_chain_manifest(std::move(manifest));

    AuthoringVerdict verdict{};
    verdict.result = VerdictResult::degraded;
    session->set_authoring_verdict(verdict);
    session->record_binding({.pass_ordinal = 2,
                             .binding_slot = 1,
                             .status = BindingStatus::substituted,
                             .resource_identity = "Source",
                             .producer_pass_ordinal = LocalizationKey::CHAIN_LEVEL,
                             .alias_name = ""});
    session->record_semantic(
        {.pass_ordinal = 2,
         .member_name = "SourceSize",
         .classification = SemanticClassification::semantic,
         .value = std::array<float, 4>{64.0F, 32.0F, 1.0F / 64.0F, 1.0F / 32.0F},
         .offset = 0});
    TimelineEvent timeline_event{};
    timeline_event.type = TimelineEventType::pass_start;
    timeline_event.pass_ordinal = 2;
    timeline_event.cpu_timestamp_ns = 1234;
    session->record_timeline(timeline_event);
    session->record_degradation({.pass_ordinal = 2,
                                 .expected_resource = "History1",
                                 .substituted_resource = "Source",
                                 .frame_index = 4,
                                 .type = DegradationType::texture_fallback});

    session->emit({.severity = Severity::error,
                   .original_severity = Severity::error,
                   .category = Category::runtime,
                   .localization = {.pass_ordinal = 2, .stage = "bind", .resource = "History1"},
                   .frame_index = 4,
                   .message = "Fallback used",
                   .evidence =
                       [] {
                           BindingEvidence evidence{};
                           evidence.resource_id = "Source";
                           evidence.is_fallback = true;
                           return evidence;
                       }(),
                   .session_identity = std::nullopt});
    session->emit({.severity = Severity::info,
                   .original_severity = Severity::info,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = 2, .stage = "compile", .resource = {}},
                   .message = "Compile succeeded",
                   .evidence = CompileEvidence{.stage = "vertex+fragment",
                                               .success = true,
                                               .messages = {},
                                               .timing_us = 12.5,
                                               .cache_hit = true},
                   .session_identity = std::nullopt});
    session->emit({.severity = Severity::warning,
                   .original_severity = Severity::warning,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = 2, .stage = "reflection", .resource = {}},
                   .message = "Reflection degraded",
                   .evidence = ReflectionEvidence{.stage = "fragment",
                                                  .resource_summary = {"texture:Source@1"},
                                                  .merge_conflicts = {}},
                   .session_identity = std::nullopt});
    session->emit({.severity = Severity::debug,
                   .original_severity = Severity::debug,
                   .category = Category::authoring,
                   .localization = {.pass_ordinal = 2, .stage = "provenance", .resource = {}},
                   .message = "Mapped line info",
                   .evidence = ProvenanceEvidence{.original_file = "probe.slang",
                                                  .original_line = 8,
                                                  .rewrite_applied = false,
                                                  .rewrite_description = {}},
                   .session_identity = std::nullopt});
    session->emit(
        {.severity = Severity::info,
         .original_severity = Severity::info,
         .category = Category::capture,
         .localization = {.pass_ordinal = 2, .stage = "capture", .resource = "image"},
         .frame_index = 4,
         .message = "Captured pass output",
         .evidence =
             CaptureEvidence{.pass_ordinal = 2, .frame_index = 4, .image_ref = "capture.png"},
         .session_identity = std::nullopt});
    return session;
}

} // namespace

TEST_CASE("Minimal reporting keeps only compact verdict data", "[diagnostics][reporting]") {
    auto session = make_session(CaptureMode::minimal);
    const auto report = build_diagnostic_report(*session);

    REQUIRE(report.authoring_verdict.has_value());
    CHECK(report.degraded);
    CHECK(report.error_counts_by_category.runtime == 1);
    CHECK_FALSE(report.manifest.has_value());
    CHECK(report.compile_summaries.empty());
    CHECK(report.selected_captures.empty());
    CHECK(report.event_timeline.empty());
}

TEST_CASE("Standard reporting includes manifest coverage and trace", "[diagnostics][reporting]") {
    auto session = make_session(CaptureMode::standard);
    const auto report = build_diagnostic_report(*session);

    REQUIRE(report.manifest.has_value());
    CHECK(report.manifest->passes().size() == 1);
    CHECK(report.compile_summaries.size() == 1);
    CHECK(report.reflection_summaries.size() == 1);
    CHECK(report.binding_coverage.size() == 1);
    CHECK(report.binding_coverage.front().substituted == 1);
    CHECK(report.semantic_coverage.size() == 1);
    CHECK(report.semantic_coverage.front().semantic == 1);
    CHECK(report.execution_trace.size() == 1);
    CHECK(report.selected_captures.empty());
    CHECK(report.event_timeline.empty());
}

TEST_CASE("Investigate reporting adds captures provenance and degradation details",
          "[diagnostics][reporting]") {
    auto session = make_session(CaptureMode::investigate);
    const auto report = build_diagnostic_report(*session);

    CHECK(report.provenance.size() == 1);
    CHECK(report.degradation_entries.size() == 1);
    CHECK(report.selected_captures.size() == 1);
    CHECK(report.selected_captures.front().image_ref == "capture.png");
    CHECK(report.event_timeline.empty());
    CHECK_FALSE(report.includes_artifact_bundle);
}

TEST_CASE("Forensic reporting includes full event timeline and artifact marker",
          "[diagnostics][reporting]") {
    auto session = make_session(CaptureMode::forensic);
    const auto report = build_diagnostic_report(*session);

    CHECK(report.event_timeline.size() == session->events().size());
    REQUIRE_FALSE(report.event_timeline.empty());
    REQUIRE(report.event_timeline.front().session_identity.has_value());
    CHECK(report.event_timeline.front().session_identity->generation_id == 17);
    CHECK(report.includes_artifact_bundle);
}

TEST_CASE("Forensic reports snapshot event session identity", "[diagnostics][reporting]") {
    DiagnosticReport report;
    {
        auto session = make_session(CaptureMode::forensic);
        report = build_diagnostic_report(*session);
    }

    REQUIRE_FALSE(report.event_timeline.empty());
    REQUIRE(report.event_timeline.front().session_identity.has_value());
    CHECK(report.event_timeline.front().session_identity->generation_id == 17);
}
