## ADDED Requirements

### Requirement: Standalone Release ABI Guardrail
For standalone `goggles-filter-chain` releases reporting ABI v1, the C API SHALL preserve v1 symbol and struct compatibility and SHALL reject publication when compatibility checks fail.

#### Scenario: ABI compatibility check before release
- **GIVEN** a standalone release candidate and an approved v1 ABI baseline are available
- **WHEN** a standalone release pipeline validates a candidate against the v1 baseline
- **THEN** all required v1 symbols and struct-prefix contracts SHALL remain compatible
- **AND** publication SHALL be blocked if compatibility validation fails

### Requirement: Public Header Packaging Contract
Standalone release artifacts SHALL ship the canonical public C header at `include/goggles_filter_chain.h` with calling-convention/export macros consistent with the declared ABI version.

#### Scenario: Host consumes packaged C header
- **GIVEN** a host integration consumes a standalone release package
- **WHEN** a host integrates a released standalone package
- **THEN** the package SHALL expose `include/goggles_filter_chain.h` as the canonical public C entry point
- **AND** declared ABI/version query functions SHALL match the packaged contract

### Requirement: Consumer Package Metadata Contract
Standalone releases SHALL provide consumer package metadata that declares include/library layout, ABI version linkage metadata, and machine-readable manifest entries for compatibility evidence artifacts.

#### Scenario: Consumer validates package metadata
- **GIVEN** a host integration pipeline resolves a standalone runtime package
- **WHEN** metadata validation runs before linking
- **THEN** include/library paths and ABI linkage metadata SHALL match the declared package contract
- **AND** compatibility evidence artifact references SHALL be present in package metadata or manifest outputs
