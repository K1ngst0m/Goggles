## ADDED Requirements

### Requirement: Unsized Array Declaration Fix

The RetroArch preprocessor SHALL transform unsized array declarations from `type[] var = type[](...)` to `const type var[] = type[](...)` to ensure compatibility with Slang's GLSL frontend.

#### Scenario: Unsized int array declaration

- **WHEN** a shader contains `int[] font = int[](...)`
- **THEN** the preprocessor transforms it to `const int font[] = int[](...)`

#### Scenario: Unsized array with explicit size in constructor

- **WHEN** a shader contains `int[] arr = int[N](...)`
- **THEN** the preprocessor transforms it to `const int arr[] = int[](...)`
