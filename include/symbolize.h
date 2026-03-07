/// @file symbolize.h
/// @brief Symbolization utilities for address-to-symbol resolution

#pragma once

#include "profiler_version.h"
#include <memory>
#include <string>
#include <vector>

PROFILER_NAMESPACE_BEGIN

/// @struct SymbolizedFrame
/// @brief Result of symbolizing a single address
///
/// Contains the resolved symbol information for a single instruction pointer.
struct SymbolizedFrame {
    std::string function_name;  ///< Function name (demangled)
    std::string source_file;    ///< Source file path
    unsigned int line = 0;      ///< Line number in source file
    bool is_inlined = false;    ///< Whether this is an inlined function
};

/// @class Symbolizer
/// @brief Abstract interface for symbolization
///
/// Provides a common interface for resolving addresses to symbols.
/// Different implementations can use different backends (e.g., abseil, libbacktrace).
class Symbolizer {
public:
    virtual ~Symbolizer() = default;

    /// @brief Symbolize a single address
    /// @param address The instruction pointer to symbolize
    /// @return Vector of SymbolizedFrame (may contain multiple for inlined frames)
    virtual std::vector<SymbolizedFrame> symbolize(void* address) = 0;

    /// @brief Symbolize multiple addresses in batch
    /// @param addresses Vector of instruction pointers to symbolize
    /// @return Vector of symbolized frame vectors (one per input address)
    virtual std::vector<std::vector<SymbolizedFrame>> symbolizeBatch(const std::vector<void*>& addresses) = 0;
};

/// @class BackwardSymbolizer
/// @brief Symbolizer implementation using backward-cpp library
///
/// Uses the backward-cpp library for address symbolization with support
/// for inline function detection.
class BackwardSymbolizer : public Symbolizer {
public:
    BackwardSymbolizer();
    ~BackwardSymbolizer() override;

    std::vector<SymbolizedFrame> symbolize(void* address) override;
    std::vector<std::vector<SymbolizedFrame>> symbolizeBatch(const std::vector<void*>& addresses) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_; ///< Pimpl pointer for implementation hiding
};

/// @brief Factory function to create a Symbolizer instance
/// @return Unique pointer to a new Symbolizer instance
std::unique_ptr<Symbolizer> createSymbolizer();

PROFILER_NAMESPACE_END
