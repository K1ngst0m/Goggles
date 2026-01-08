## ADDED Requirements

### Requirement: Child Process Death Signal

The application SHALL configure spawned child processes to receive SIGTERM when the parent process terminates unexpectedly.

#### Scenario: Parent crash terminates child

- **GIVEN** a child process was spawned via `-- <app>` mode
- **WHEN** the parent goggles process is killed (SIGKILL, crash, or abnormal termination)
- **THEN** the child process SHALL receive SIGTERM automatically
- **AND** the child process SHALL terminate

#### Scenario: Parent PID 1 reparenting race

- **GIVEN** a child process is being spawned
- **WHEN** the parent dies between `fork()` and `prctl()` setup
- **THEN** the child SHALL detect reparenting to PID 1
- **AND** SHALL exit immediately with failure code