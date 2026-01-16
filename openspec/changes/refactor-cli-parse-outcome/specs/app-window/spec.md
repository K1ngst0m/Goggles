# app-window Specification (Delta)

## MODIFIED Requirements

### Requirement: Command Line Interface
The application SHALL support command-line arguments to override default behavior and provide
information without throwing exceptions into the main execution flow.

#### Scenario: Display help
- **WHEN** the application is run with `--help`
- **THEN** it SHALL print usage information
- **AND** CLI parsing SHALL indicate an “exit successfully” disposition without using an error
  sentinel
- **AND** the application SHALL exit with code 0

#### Scenario: Display version
- **WHEN** the application is run with `--version`
- **THEN** it SHALL print version information
- **AND** CLI parsing SHALL indicate an “exit successfully” disposition without using an error
  sentinel
- **AND** the application SHALL exit with code 0

#### Scenario: Exception encapsulation
- **GIVEN** the application uses CLI11 for parsing
- **WHEN** `parse_cli` is called
- **THEN** it MUST catch all library exceptions internally
- **AND** it MUST return a value-based `goggles::Result<>` result
- **AND** “help/version requested” MUST be represented as a successful outcome (not as an error with
  `ErrorCode::ok`)

