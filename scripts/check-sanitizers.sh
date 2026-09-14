#!/usr/bin/env bash
# Reproduce the CI sanitizer configurations locally.
#
# CI caught a LeakSanitizer failure in stopHeapProfiler() that local release
# builds never showed, because nothing here ran with -fsanitize=address. This
# script exists so that check is one command instead of a CI round trip.
#
# Usage:
#   scripts/check-sanitizers.sh            # ASan (the job that failed in CI)
#   scripts/check-sanitizers.sh asan ubsan # pick jobs
#   scripts/check-sanitizers.sh all
#
# Requires: cmake, a vcpkg checkout with the project's dependencies installed
#           (same prerequisites as a normal build).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

VCPKG_TOOLCHAIN="$PROJECT_ROOT/vcpkg/scripts/buildsystems/vcpkg.cmake"
VCPKG_TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-linux-release}"
BUILD_ROOT="${SANITIZER_BUILD_ROOT:-$PROJECT_ROOT/build/sanitizers}"
JOBS="$(nproc)"

log() { printf '\n=== %s ===\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ -f "$VCPKG_TOOLCHAIN" ]] || die "vcpkg toolchain not found at $VCPKG_TOOLCHAIN"

# Allow the pre-installed vcpkg tree to be reused instead of installing a
# manifest from scratch (matches how the local dev builds are run).
VCPKG_INSTALLED_DIR="$PROJECT_ROOT/vcpkg_installed"
extra_configure_args=()
[[ -d "$VCPKG_INSTALLED_DIR" ]] && extra_configure_args+=(
    -DVCPKG_MANIFEST_MODE=OFF
    "-DVCPKG_INSTALLED_DIR=$VCPKG_INSTALLED_DIR"
)

# Mirrors the flag sets in .github/workflows/code-quality.yml
run_asan() {
    log "ASan + LSan (mirrors the CI 'Address Sanitizer' job)"
    local dir="$BUILD_ROOT/asan"
    cmake -S . -B "$dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
        "-DVCPKG_TARGET_TRIPLET=$VCPKG_TRIPLET" \
        -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
        -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
        -DCMAKE_MODULE_LINKER_FLAGS="-fsanitize=address" \
        "${extra_configure_args[@]}"
    cmake --build "$dir" -j"$JOBS"
    # detect_leaks + halt_on_error: a leak must fail the run, as it does in CI.
    ( cd "$dir" && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
        LSAN_OPTIONS="suppressions=$PROJECT_ROOT/lsan.supp" \
        ctest --output-on-failure )
}

run_ubsan() {
    log "UBSan (mirrors the CI 'Undefined Behavior Sanitizer' job)"
    local dir="$BUILD_ROOT/ubsan"
    cmake -S . -B "$dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
        "-DVCPKG_TARGET_TRIPLET=$VCPKG_TRIPLET" \
        -DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g" \
        -DCMAKE_C_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" \
        -DCMAKE_MODULE_LINKER_FLAGS="-fsanitize=undefined" \
        "${extra_configure_args[@]}"
    cmake --build "$dir" -j"$JOBS"
    # halt_on_error so a single UB report fails the run rather than scrolling by.
    ( cd "$dir" && UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
        ctest --output-on-failure )
}

run_tsan() {
    log "TSan (mirrors the CI job; disabled there, kept here for manual runs)"
    local dir="$BUILD_ROOT/tsan"
    cmake -S . -B "$dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
        "-DVCPKG_TARGET_TRIPLET=$VCPKG_TRIPLET" \
        -DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
        -DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
        -DCMAKE_MODULE_LINKER_FLAGS="-fsanitize=thread" \
        "${extra_configure_args[@]}"
    cmake --build "$dir" -j"$JOBS"
    ( cd "$dir" && TSAN_OPTIONS=halt_on_error=1 ctest --output-on-failure )
}

targets=("$@")
[[ ${#targets[@]} -eq 0 ]] && targets=(asan)
[[ "${targets[0]}" == "all" ]] && targets=(asan ubsan tsan)

for t in "${targets[@]}"; do
    case "$t" in
        asan)  run_asan ;;
        ubsan) run_ubsan ;;
        tsan)  run_tsan ;;
        *)     die "unknown target '$t' (expected asan, ubsan, tsan or all)" ;;
    esac
done

log "All requested sanitizer runs passed"
