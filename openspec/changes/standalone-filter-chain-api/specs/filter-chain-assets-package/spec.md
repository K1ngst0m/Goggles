# Delta for filter-chain-assets-package

## MODIFIED Requirements

### Requirement: Library-Owned Asset Package

The standalone filter-chain project SHALL own built-in internal shaders and runtime assets required
for normal operation. Those built-in assets SHALL be resolved through library-owned packaging and
runtime lookup rules rather than through a required public `shader_dir` input from consumers.

The final package contract MUST remove stale asset-era migration residue. Installed metadata,
consumer docs/examples, and runtime configuration guidance MUST NOT preserve deprecated `shader_dir`
style assumptions, compatibility toggles, or alternate instructions that imply library-owned built-
ins still depend on source-tree-relative or install-only public paths.

#### Scenario: Runtime uses built-in assets without external asset directory input

- GIVEN a host links the standalone library and loads a supported preset
- WHEN the runtime needs built-in internal shader or auxiliary asset content
- THEN it SHALL resolve that content through library-owned packaging
- AND the host SHALL NOT be required to provide a public `shader_dir` configuration value

### Requirement: Asset Resolution Is Package-Oriented

Package-oriented asset resolution SHALL support both file-based and memory-based preset entry paths.
When a preset is loaded from a file, asset resolution MUST use documented package-oriented rules
instead of Goggles-relative paths. When a preset is loaded from memory, the runtime MUST still be
able to resolve any library-owned built-in assets needed for execution.

For file-backed preset sources, relative includes and external texture references MUST resolve from
the parent directory of the supplied preset path. For memory-backed preset sources, relative
includes and external texture references MUST resolve through the registered resolver callback or an
explicit host-supplied base path. If neither is provided, the runtime MUST reject relative external
references deterministically instead of consulting process working-directory state.

#### Scenario: Memory-loaded preset still resolves built-in runtime assets

- GIVEN a host loads a preset from an in-memory byte buffer
- WHEN the runtime evaluates preset dependencies needed for execution
- THEN library-owned built-in assets SHALL remain resolvable through the standalone package contract
- AND successful execution SHALL NOT depend on the host providing a source-tree-relative asset path

### Requirement: Assets Support Public-Surface Validation

The standalone asset package SHALL continue to support installed public-surface validation, while the
minimum normal-operation asset contract for embedded consumers SHALL remain library-owned and
self-contained. Installed validation SHALL prove that both file-source and memory-source preset flows
work without Goggles-owned assets.

Installed validation and package-facing guidance MUST describe the final steady-state contract, not
temporary migration behavior. Obsolete examples, stale package notes, and validation targets that
depend on removed public asset assumptions MUST be deleted or rewritten before the change is
considered complete.

#### Scenario: Installed validation covers both preset source forms

- GIVEN maintainers validate the installed standalone package
- WHEN contract checks exercise preset loading behavior
- THEN validation SHALL cover at least one file-based preset source and one memory-based preset source
- AND neither flow SHALL require Goggles-owned assets or Goggles repository-relative lookup rules

#### Scenario: Package-facing asset guidance contains no stale assumptions

- GIVEN a maintainer reviews installed package metadata and consumer guidance for asset handling
- WHEN the standalone asset redesign is complete
- THEN all package-facing guidance SHALL describe only library-owned built-ins plus the documented file/memory source rules
- AND no stale example, release note, or exported config text SHALL instruct consumers to configure removed public asset-directory inputs

## REMOVED Requirements

### Requirement: Test Fixture Resolution Through Compile Definition

(Reason: Public asset behavior should be defined in terms of package-visible runtime and validation
contracts, not by a specific internal compile-definition mechanism.)
