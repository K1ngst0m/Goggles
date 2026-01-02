## ADDED Requirements

### Requirement: Factory Pattern for Complex Initialization
Major subsystem classes with fallible initialization SHALL use static factory methods returning `ResultPtr<T>` instead of two-phase initialization (constructor + `init()`).

**Applicability**: This requirement applies to core subsystem classes (backends, chains, passes, runtimes, receivers) that must be fully initialized before use. It does NOT apply to:
- Optional/lazy-initialized components that can validly exist in inactive states
- C API boundary code (Vulkan layer)
- Utility classes where default construction is semantically valid

#### Scenario: Successful initialization
- **WHEN** a factory method is called with valid parameters
- **THEN** the method SHALL return a `ResultPtr<T>` containing a fully initialized object
- **AND** the object SHALL be ready for immediate use without additional initialization calls

#### Scenario: Initialization failure
- **WHEN** initialization fails for any reason
- **THEN** the factory method SHALL return an error using `make_result_ptr_error<T>()`
- **AND** the error SHALL include a clear error code and descriptive message
- **AND** all partially allocated resources SHALL be cleaned up automatically

#### Scenario: Constructor visibility
- **WHEN** a class uses factory pattern
- **THEN** the constructor SHALL be private
- **AND** direct construction SHALL be prevented at compile time

### Requirement: ResultPtr Type Alias
The codebase SHALL provide a `ResultPtr<T>` type alias for `Result<std::unique_ptr<T>>` to simplify factory method signatures.

#### Scenario: Type alias usage
- **WHEN** declaring a factory method
- **THEN** the return type SHALL use `ResultPtr<T>` instead of `Result<std::unique_ptr<T>>`
- **AND** the code SHALL use `make_result_ptr()` for success cases
- **AND** the code SHALL use `make_result_ptr_error<T>()` for error cases

### Requirement: Classes Using Factory Pattern
The following classes SHALL use factory pattern instead of two-phase initialization:
- VulkanBackend
- FilterChain
- FilterPass
- ShaderRuntime
- Framebuffer
- OutputPass
- CaptureReceiver
- InputForwarder (already implemented)

#### Scenario: Factory method signature
- **WHEN** implementing a factory method
- **THEN** it SHALL be declared as `[[nodiscard]] static auto create(...) -> ResultPtr<ClassName>;`
- **AND** it SHALL accept all necessary initialization parameters
- **AND** it SHALL use `GOGGLES_TRY()` for nested factory calls where applicable

## REMOVED Requirements

### Requirement: Two-Phase Initialization Pattern (for Major Subsystems)
**Reason**: Two-phase initialization (constructor + `init()`) allows objects to exist in uninitialized states, violating RAII principles and project error handling policies for classes that must be fully initialized before use.

**Migration**: Replace with static factory methods that return `ResultPtr<T>` for affected classes. Update all call sites to use `ClassName::create()` instead of `ClassName obj; obj.init()`.

**Exception**: Two-phase initialization remains acceptable for optional/lazy-initialized components where an "inactive" or "uninitialized" state is a valid operational mode (e.g., FrameHistory with depth=0).
