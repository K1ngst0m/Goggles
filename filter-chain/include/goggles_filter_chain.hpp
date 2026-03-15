#pragma once

/// @file goggles_filter_chain.hpp
/// @brief Single-include C++ header for the GogglesFilterChain library.
///
/// Consumers can include just this file instead of individual headers.
/// Individual headers remain available for selective inclusion.

// C ABI — opaque handles, POD structs, function declarations
#include <goggles_filter_chain.h>

// C++ foundation — error codes, Result<T>, macros
#include <goggles/filter_chain/error.hpp>

// C++ convenience alias
#include <goggles/filter_chain/result.hpp>

// C++ enums — LogLevel, Stage, Provenance, Extent2D
#include <goggles/filter_chain/common.hpp>

// Host-facing scale mode enum
#include <goggles/filter_chain/scale_mode.hpp>

// RAII wrappers — Instance, Device, Program, Chain
#include <goggles/filter_chain/api.hpp>

// Filter control descriptors
#include <goggles/filter_chain/filter_controls.hpp>

// Vulkan device context struct for host-library handoff
#include <goggles/filter_chain/vulkan_context.hpp>
