# documentation Specification

## Purpose
TBD - created by archiving change add-documentation-structure. Update Purpose after archive.
## Requirements
### Requirement: Documentation Policy

The project SHALL define a documentation policy that specifies:
- What types of content to document (architecture, design rationale, constraints)
- What types of content NOT to document (implementation details AI can regenerate)
- Standard document pattern for consistency
- File naming convention (`snake_case.md`)
- Maintenance expectations

#### Scenario: New contributor reads documentation policy
- **WHEN** a new contributor wants to add documentation
- **THEN** they can find clear guidelines in `openspec/project.md` under Documentation Policy

### Requirement: Documentation Naming Convention

All documentation files in `docs/` SHALL use `snake_case.md` naming, consistent with the project's code file naming conventions.

#### Scenario: New documentation file created
- **WHEN** a contributor creates a new documentation file
- **THEN** the file name uses snake_case (e.g., `filter_chain.md`, not `FilterChain.md` or `FILTER_CHAIN.md`)

### Requirement: Architecture Overview Document

The project SHALL provide a high-level architecture overview document at `docs/architecture.md` that serves as the entry point for understanding the codebase.

The document SHALL include:
- System context (what Goggles does, how it fits in the environment)
- Module overview with responsibilities
- Data flow between major components
- Links to topic-specific documentation

#### Scenario: Maintainer needs to understand codebase
- **WHEN** a maintainer wants to understand how the project is structured
- **THEN** they can read `docs/architecture.md` and get a high-level map in under 10 minutes

#### Scenario: Maintainer needs deeper topic knowledge
- **WHEN** a maintainer needs to understand a specific subsystem (threading, DMA-BUF, filter chain)
- **THEN** the architecture doc links to the relevant topic-specific document

### Requirement: README Documentation Section

The project README SHALL include a Documentation section that:
- Points to `docs/architecture.md` as the entry point
- Briefly describes what documentation is available

#### Scenario: User discovers documentation from README
- **WHEN** a user reads the README
- **THEN** they can find a link to project documentation

### Requirement: Consistent Document Pattern

All topic-specific documentation SHALL follow a consistent pattern:
1. Purpose (1-2 sentences)
2. Overview (with diagram if helpful)
3. Key Components
4. How They Connect (data/control flow)
5. Constraints/Decisions (the "why")

#### Scenario: Reading any topic doc feels familiar
- **WHEN** a maintainer reads any topic documentation
- **THEN** they find a consistent structure that matches other docs

