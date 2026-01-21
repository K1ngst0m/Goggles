## 1. Implement Generic Post-Chain Infrastructure

- [x] 1.1 Add `m_postchain_passes` vector to `FilterChain` (type: `std::vector<std::unique_ptr<Pass>>`)
- [x] 1.2 Add `m_postchain_framebuffers` vector to `FilterChain` for intermediate results
- [x] 1.3 Rename `PreChainResult` to `ChainResult` (used by both pre-chain and post-chain)
- [x] 1.4 Implement `record_postchain()` to iterate all post-chain passes

## 2. Refactor OutputPass Integration

- [x] 2.1 Remove `m_output_pass` single member pointer from `FilterChain`
- [x] 2.2 Add `OutputPass` to `m_postchain_passes` vector during `FilterChain::create()`
- [x] 2.3 Update `record()` to call `record_postchain()` instead of `m_output_pass->record()`
- [x] 2.4 Update `shutdown()` to iterate `m_postchain_passes` vector

## 3. Update Recording Flow

- [x] 3.1 Ensure `record_postchain()` chains pass outputs correctly (source_view progression)
- [x] 3.2 Final pass (OutputPass) renders to swapchain via `ctx.target_image_view`
- [x] 3.3 Intermediate passes render to `m_postchain_framebuffers[i]`

## 4. Verification

- [x] 4.1 Verify quality build passes
- [x] 4.2 Verify existing behavior unchanged (OutputPass still renders correctly)
- [x] 4.3 Verify no validation errors with RetroArch shader presets
