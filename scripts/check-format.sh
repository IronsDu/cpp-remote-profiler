#!/bin/bash
#
# clang-format check script
# Usage: ./scripts/check-format.sh [--fix]
#
# Options:
#   --fix    Automatically fix formatting issues
#
# This script uses Docker/Podman to ensure clang-format version consistency with CI.
# If no container runtime is available, it falls back to local clang-format.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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

# Detect container runtime (docker or podman)
CONTAINER_RUNTIME=""
if command -v docker &> /dev/null; then
    CONTAINER_RUNTIME="docker"
elif command -v podman &> /dev/null; then
    CONTAINER_RUNTIME="podman"
fi

# Function to run clang-format
run_clang_format() {
    local args="$1"
    if [[ -n "$CONTAINER_RUNTIME" ]]; then
        $CONTAINER_RUNTIME run --rm --privileged \
            -v "$PROJECT_ROOT:/workspace" \
            -w /workspace \
            "$CLANG_FORMAT_IMAGE" \
            clang-format $args
    else
        clang-format $args
    fi
}

# Setup clang-format command
if [[ -n "$CONTAINER_RUNTIME" ]]; then
    echo "Using container runtime: $CONTAINER_RUNTIME"
    echo "clang-format image: $CLANG_FORMAT_IMAGE"

    # Pull image if not exists
    echo "Ensuring image is available..."
    $CONTAINER_RUNTIME pull "$CLANG_FORMAT_IMAGE" 2>/dev/null || true

    # Show version
    echo "clang-format version:"
    run_clang_format "--version"
    echo ""
else
    # Fall back to local clang-format
    if ! command -v clang-format &> /dev/null; then
        echo -e "${RED}Error: clang-format is not installed and no container runtime found${NC}"
        echo ""
        echo "Please install one of the following:"
        echo "  - Docker: https://docs.docker.com/get-docker/"
        echo "  - Podman: https://podman.io/getting-started/installation"
        echo "  - clang-format: apt-get install clang-format / dnf install clang-tools-extra"
        exit 1
    fi

    echo -e "${YELLOW}Warning: No container runtime found, using local clang-format${NC}"
    echo -e "${YELLOW}Warning: Version mismatch may cause CI failures!${NC}"
    echo ""
    echo "Local clang-format version:"
    clang-format --version
    echo ""
    echo -e "${YELLOW}Consider installing Docker or Podman for consistent formatting.${NC}"
    echo ""
fi

echo "Checking format for $(echo "$SOURCE_FILES" | wc -w) files..."
echo ""

HAS_ISSUES=false

if [[ "$FIX_MODE" == true ]]; then
    echo -e "${YELLOW}Fix mode enabled - modifying files in place${NC}"
    echo ""

    for FILE in $SOURCE_FILES; do
        REL_PATH="${FILE#$PROJECT_ROOT/}"
        if [[ -n "$CONTAINER_RUNTIME" ]]; then
            run_clang_format "-i /workspace/$REL_PATH"
        else
            clang-format -i "$FILE"
        fi
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
    if [[ -n "$CONTAINER_RUNTIME" ]]; then
        FORMATTED=$(run_clang_format "/workspace/$REL_PATH" 2>/dev/null)
    else
        FORMATTED=$(clang-format "$FILE" 2>/dev/null)
    fi
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
