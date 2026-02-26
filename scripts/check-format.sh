#!/bin/bash
#
# clang-format check script
# Usage: ./scripts/check-format.sh [--fix]
#
# Options:
#   --fix    Automatically fix formatting issues
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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

# Check if clang-format is installed
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}Error: clang-format is not installed${NC}"
    echo "Please install clang-format:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-format"
    echo "  Fedora: sudo dnf install clang-tools-extra"
    echo "  macOS: brew install clang-format"
    exit 1
fi

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

echo "Checking format for $(echo "$SOURCE_FILES" | wc -w) files..."
echo ""

HAS_ISSUES=false

if [[ "$FIX_MODE" == true ]]; then
    echo -e "${YELLOW}Fix mode enabled - modifying files in place${NC}"
    echo ""
    for FILE in $SOURCE_FILES; do
        clang-format -i "$FILE"
        echo -e "  Fixed: ${GREEN}$FILE${NC}"
    done
    echo ""
    echo -e "${GREEN}All files have been formatted${NC}"
    exit 0
fi

# Check each file
for FILE in $SOURCE_FILES; do
    # Get the formatted version
    FORMATTED=$(clang-format "$FILE" 2>/dev/null)
    ORIGINAL=$(cat "$FILE" 2>/dev/null)

    # Compare
    if [[ "$FORMATTED" != "$ORIGINAL" ]]; then
        if [[ "$HAS_ISSUES" == false ]]; then
            echo -e "${RED}Format check failed!${NC}"
            echo ""
            HAS_ISSUES=true
        fi

        REL_PATH="${FILE#$PROJECT_ROOT/}"
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
    echo "Or format specific files with:"
    echo -e "  ${GREEN}clang-format -i <file>${NC}"
    echo ""
    exit 1
else
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}Format check passed!${NC}"
    echo -e "${GREEN}All files are properly formatted.${NC}"
    exit 0
fi
