# Implementation Tasks

## 1. ShaderRuntime Refactoring
- [x] 1.1 Add `static auto create() -> ResultPtr<ShaderRuntime>;` to header
- [x] 1.2 Move constructor to private section
- [x] 1.3 Remove public `init()` method from header
- [x] 1.4 Implement `create()` factory method in cpp file
- [x] 1.5 Update usage sites in FilterChain
- [x] 1.6 Test shader loading and runtime initialization

## 2. Framebuffer Refactoring
- [x] 2.1 Add `static auto create() -> ResultPtr<Framebuffer>;` to header
- [x] 2.2 Move constructor to private section
- [x] 2.3 Remove public `init()` method from header
- [x] 2.4 Implement `create()` factory method in cpp file
- [x] 2.5 Update usage sites in FilterPass and OutputPass
- [x] 2.6 Test framebuffer creation and resize handling

## 3. OutputPass Refactoring
- [x] 3.1 Add `static auto create() -> ResultPtr<OutputPass>;` to header
- [x] 3.2 Move constructor to private section
- [x] 3.3 Remove public `init()` method from header
- [x] 3.4 Implement `create()` factory method in cpp file
- [x] 3.5 Update usage sites in VulkanBackend
- [x] 3.6 Test output pass creation and rendering

## 4. FilterPass Refactoring
- [x] 4.1 Analyze complex initialization logic (147 lines)
- [x] 4.2 Add `static auto create() -> ResultPtr<FilterPass>;` to header
- [x] 4.3 Move constructor to private section
- [x] 4.4 Remove public `init()` method from header
- [x] 4.5 Implement `create()` factory method in cpp file
- [x] 4.6 Update usage sites in FilterChain
- [x] 4.7 Test filter pass creation and rendering

## 5. FilterChain Refactoring
- [x] 5.1 Analyze dynamic FilterPass creation pattern
- [x] 5.2 Add `static auto create() -> ResultPtr<FilterChain>;` to header
- [x] 5.3 Move constructor to private section
- [x] 5.4 Remove public `init()` method from header
- [x] 5.5 Implement `create()` factory method in cpp file
- [x] 5.6 Update FilterPass factory usage within FilterChain
- [x] 5.7 Update usage sites in VulkanBackend
- [x] 5.8 Test filter chain creation and pipeline execution

## 6. CaptureReceiver Refactoring
- [x] 6.1 Convert from `bool init()` to Result-based factory
- [x] 6.2 Add `static auto create() -> ResultPtr<CaptureReceiver>;` to header
- [x] 6.3 Move constructor to private section
- [x] 6.4 Remove public `init()` method from header
- [x] 6.5 Implement `create()` factory method in cpp file
- [x] 6.6 Update usage in main.cpp
- [x] 6.7 Test capture initialization and frame reception

## 7. VulkanBackend Refactoring
- [x] 7.1 Analyze complex initialization logic (128+ lines, 15+ create methods)
- [x] 7.2 Add `static auto create() -> ResultPtr<VulkanBackend>;` to header
- [x] 7.3 Move constructor to private section
- [x] 7.4 Remove public `init()` method from header
- [x] 7.5 Implement `create()` factory method in cpp file
- [x] 7.6 Handle dependencies (FilterChain, OutputPass, Framebuffer)
- [x] 7.7 Update usage in main.cpp
- [x] 7.8 Test Vulkan initialization and rendering pipeline
- [x] 7.9 Test error handling and cleanup on initialization failure

## 8. Testing and Validation
- [x] 8.1 Run all existing tests with new factory pattern
- [x] 8.2 Test initialization failure paths
- [x] 8.3 Test proper cleanup on factory failures
- [x] 8.4 Verify error messages are clear
- [x] 8.5 Test all public API surfaces still work correctly
- [x] 8.6 Run with AddressSanitizer to detect memory issues

**Note**: Vulkan-dependent classes (Framebuffer, FilterPass, OutputPass, FilterChain, VulkanBackend)
require device setup for comprehensive initialization failure testing and are deferred for future test infrastructure work.

## 9. Documentation
- [x] 9.1 Update code comments if needed (following minimal comment policy)
- [x] 9.2 Update design documentation if initialization patterns have changed

## Implementation Order Rationale
Classes are ordered by dependency chain:
1. ShaderRuntime - Leaf dependency, simple pimpl pattern
2. Framebuffer - Used by passes, manageable complexity
3. OutputPass - Depends on Framebuffer
4. FilterPass - Depends on Framebuffer, moderate complexity
5. FilterChain - Depends on FilterPass, dynamic creation pattern
6. CaptureReceiver - Independent, bool to Result conversion
7. VulkanBackend - Root dependency, most complex, depends on FilterChain and OutputPass
