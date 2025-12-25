## ADDED Requirements

### Requirement: Shader Caching
The system SHALL cache compiled RetroArch shaders to disk to minimize startup latency and eliminate redundant GPU work.

#### Scenario: Persistent Cache Lookup
- **GIVEN** a shader has been compiled once
- **WHEN** the same shader is requested again (even after app restart)
- **THEN** it SHALL be loaded from disk cache and Slang compilation SHALL be bypassed.

#### Scenario: Serialization of Reflection
- **GIVEN** a RetroArch shader requires complex bindings (UBOs, Textures, Push Constants)
- **WHEN** cached to disk
- **THEN** the cache MUST include full `ReflectionData` and it MUST be restored correctly on cache hit, including all binding offsets and stage flags.

#### Scenario: Automatic Invalidation
- **GIVEN** a cached shader exists
- **WHEN** the source code of that shader is modified
- **THEN** the system SHALL detect the hash mismatch and it SHALL recompile and update the cache.

#### Scenario: Type-Safe Serialization
- **GIVEN** data being serialized to disk
- **WHEN** using `write_pod` or `read_pod`
- **THEN** the system MUST enforce `std::is_standard_layout_v` to ensure memory safety for Vulkan-specific types like bitmasks and handles.

#### Scenario: Atomic Cache Updates
- **GIVEN** the system is writing a new cache file
- **WHEN** a crash or disk-full event occurs during the write
- **THEN** the existing valid cache file MUST NOT be corrupted
- **AND** the system SHALL use a temporary file and atomic rename to ensure cache integrity.

#### Scenario: Integrity Validation
- **GIVEN** a potentially corrupted cache file on disk
- **WHEN** the system attempts to load it
- **THEN** it SHALL validate SPIR-V alignment and header magic/version
- **AND** it SHALL discard the corrupted file and recompile the shader if validation fails.

#### Scenario: Minimal Log Output
- **GIVEN** the system is running at default log levels
- **WHEN** a cache hit occurs
- **THEN** it SHALL NOT output detailed per-parameter or diagnostic logs
- **AND** detailed information SHALL only be available at `TRACE` or `DEBUG` levels.
