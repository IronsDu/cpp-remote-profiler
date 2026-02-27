#!/bin/bash
#
# clang-format check script
# Usage: ./scripts/check-format.sh [--fix]
#
# Options:
#   --fix    Automatically fix formatting issues
#
# This script prefers local clang-format if version matches CI.
# Otherwise, it falls back to Docker/Podman container.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Required clang-format major version (must match CI)
REQUIRED_VERSION=18

# clang-format Docker image (must match CI)
CLANG_FORMAT_IMAGE="docker.io/silkeh/clang:18"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
FIX_MODE=false
while [[ $# -gt 0 ]]; do
    case $1 in
        --fix)
            FIX_MODE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--fix]"
            exit 1
            ;;
    esac
done

# Check if .clang-format exists
if [[ ! -f "$PROJECT_ROOT/.clang-format" ]]; then
    echo -e "${RED}Error: .clang-format file not found in project root${NC}"
    exit 1
fi

# Directories to check (relative to project root)
CHECK_DIRS=(
    "src"
    "include"
    "tests"
    "example"
    "cmake/examples"
)

# Build find command for specified directories
FIND_PATHS=()
for DIR in "${CHECK_DIRS[@]}"; do
    if [[ -d "$PROJECT_ROOT/$DIR" ]]; then
        FIND_PATHS+=("$PROJECT_ROOT/$DIR")
    fi
done

# Find all source files in specified directories
if [[ ${#FIND_PATHS[@]} -eq 0 ]]; then
    echo -e "${YELLOW}Warning: No source directories found${NC}"
    exit 0
fi

SOURCE_FILES=$(find "${FIND_PATHS[@]}" \
    \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cc" -o -name "*.cxx" \) \
    2>/dev/null)

if [[ -z "$SOURCE_FILES" ]]; then
    echo -e "${YELLOW}Warning: No source files found${NC}"
    exit 0
fi

# Check local clang-format version
USE_CONTAINER=false
LOCAL_VERSION=""

if command -v clang-format &> /dev/null; then
    VERSION_OUTPUT=$(clang-format --version 2>/dev/null || echo "")
    # Extract major version (e.g., "18" from "clang-format version 18.1.8")
    LOCAL_VERSION=$(echo "$VERSION_OUTPUT" | grep -oP 'version \K\d+' | head -1)

    if [[ "$LOCAL_VERSION" == "$REQUIRED_VERSION" ]]; then
        echo "Using local clang-format (version $LOCAL_VERSION)"
        echo ""
    else
        echo -e "${YELLOW}Local clang-format version: ${LOCAL_VERSION:-unknown}${NC}"
        echo -e "${YELLOW}Required version: $REQUIRED_VERSION${NC}"
        echo ""
        USE_CONTAINER=true
    fi
else
    echo -e "${YELLOW}clang-format not found locally${NC}"
    echo ""
    USE_CONTAINER=true
fi

# Setup container if needed
CONTAINER_RUNTIME=""
if [[ "$USE_CONTAINER" == true ]]; then
    # Detect container runtime (docker or podman)
    if command -v docker &> /dev/null; then
        CONTAINER_RUNTIME="docker"
    elif command -v podman &> /dev/null; then
        CONTAINER_RUNTIME="podman"
    fi

    if [[ -z "$CONTAINER_RUNTIME" ]]; then
        echo -e "${RED}Error: clang-format version mismatch and no container runtime found${NC}"
        echo ""
        echo "Please install one of the following:"
        echo "  - clang-format $REQUIRED_VERSION: ensures version consistency"
        echo "  - Docker: https://docs.docker.com/get-docker/"
        echo "  - Podman: https://podman.io/getting-started/installation"
        exit 1
    fi

    echo "Using container runtime: $CONTAINER_RUNTIME"
    echo "clang-format image: $CLANG_FORMAT_IMAGE"

    # Pull image if not exists
    echo "Ensuring image is available..."
    $CONTAINER_RUNTIME pull "$CLANG_FORMAT_IMAGE" 2>/dev/null || true

    # Show version
    echo "clang-format version:"
    $CONTAINER_RUNTIME run --rm "$CLANG_FORMAT_IMAGE" clang-format --version
    echo ""
fi

# Function to run clang-format
run_clang_format() {
    local file="$1"
    local args="$2"

    if [[ -n "$CONTAINER_RUNTIME" ]]; then
        $CONTAINER_RUNTIME run --rm --privileged \
            -v "$PROJECT_ROOT:/workspace" \
            -w /workspace \
            "$CLANG_FORMAT_IMAGE" \
            clang-format $args /workspace/"$file"
    else
        clang-format $args "$PROJECT_ROOT/$file"
    fi
}

echo "Checking format for $(echo "$SOURCE_FILES" | wc -w) files..."
echo ""

HAS_ISSUES=false

if [[ "$FIX_MODE" == true ]]; then
    echo -e "${YELLOW}Fix mode enabled - modifying files in place${NC}"
    echo ""

    for FILE in $SOURCE_FILES; do
        REL_PATH="${FILE#$PROJECT_ROOT/}"
        run_clang_format "$REL_PATH" "-i"
        echo -e "  Fixed: ${GREEN}$REL_PATH${NC}"
    done

    echo ""
    echo -e "${GREEN}All files have been formatted${NC}"
    exit 0
fi

# Check each file
for FILE in $SOURCE_FILES; do
    REL_PATH="${FILE#$PROJECT_ROOT/}"

    # Get the formatted version
    FORMATTED=$(run_clang_format "$REL_PATH" "" 2>/dev/null)
    ORIGINAL=$(cat "$FILE" 2>/dev/null)

    # Compare
    if [[ "$FORMATTED" != "$ORIGINAL" ]]; then
        if [[ "$HAS_ISSUES" == false ]]; then
            echo -e "${RED}Format check failed!${NC}"
            echo ""
            HAS_ISSUES=true
        fi

        echo -e "${RED}File: $REL_PATH${NC}"
        echo "----------------------------------------"

        # Show diff
        DIFF_OUTPUT=$(diff -u "$FILE" <(echo "$FORMATTED") 2>/dev/null || true)

        # Color the diff output
        echo "$DIFF_OUTPUT" | while IFS= read -r line; do
            if [[ "$line" =~ ^- ]]; then
                echo -e "${RED}$line${NC}"
            elif [[ "$line" =~ ^\+ ]]; then
                echo -e "${GREEN}$line${NC}"
            elif [[ "$line" =~ ^@@ ]]; then
                echo -e "${YELLOW}$line${NC}"
            else
                echo "$line"
            fi
        done

        echo ""
    fi
done

if [[ "$HAS_ISSUES" == true ]]; then
    echo -e "${RED}========================================${NC}"
    echo -e "${RED}Format check failed!${NC}"
    echo ""
    echo "To fix formatting issues, run:"
    echo -e "  ${GREEN}./scripts/check-format.sh --fix${NC}"
    echo ""
    exit 1
else
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Format check passed!${NC}"
    echo -e "${GREEN}All files are properly formatted.${NC}"
    exit 0
fi
