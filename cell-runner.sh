#!/bin/bash
# =============================================================================
# Cell-based Job Runner Framework
# =============================================================================
#
# This framework provides a Docker-like layer execution system for bash scripts.
# It allows you to define "cells" (discrete build steps) that can be executed
# selectively, similar to Docker build layers.
#
# =============================================================================
# HOW TO USE IN YOUR RUN SCRIPT
# =============================================================================
#
# 1. Define custom variables at the top of your script:
#
#    #!/bin/bash
#    # PROJECT: my_project_name
#
#    # Custom variables
#    BUILD_TYPE="Release"
#    VERBOSE=false
#
# 2. Define custom argument parser (BEFORE sourcing this file):
#
#    parse_custom_args() {
#        local arg="$1"
#        case "$arg" in
#            --build-type=*)
#                BUILD_TYPE="${arg#*=}"
#                return 0  # Arg was handled
#                ;;
#            --verbose)
#                VERBOSE=true
#                return 0  # Arg was handled
#                ;;
#            *)
#                return 1  # Arg was NOT handled
#                ;;
#        esac
#    }
#
# 3. Define custom usage text (BEFORE sourcing this file):
#
#    show_custom_usage() {
#        echo "[--build-type=Release|Debug] [--verbose]"
#    }
#
# 4. Source this framework file:
#
#    source "$(dirname "${BASH_SOURCE[0]}")/cell-runner.sh"
#
# 5. Define your cells using labeled blocks:
#
#    # ===== CELL: configure =====
#    # Description: Run CMake configuration
#    if has_cell "configure"; then
#        echo "==> Running cell: configure"
#        cmake -B ./build -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
#        if [ $? -ne 0 ]; then
#            echo "Error: Cell 'configure' failed"
#            exit 1
#        fi
#    fi
#
# 6. Use the script:
#
#    ./run.sh --cell=configure,build,run
#    ./run.sh --cell=build --build-type=Debug --verbose
#    ./run.sh --cell=configure,build --tmux
#
# =============================================================================
# PROVIDED FEATURES
# =============================================================================
#
# - Automatic argument parsing for --cell and --tmux
# - Auto-generated help/usage message
# - Tmux session launcher with:
#   - Timestamped session names
#   - Large scrollback buffer (100k lines)
#   - Persistent shell after completion
#   - Working directory preservation
# - has_cell() function to check if a cell should run
# - Execution context header (working dir, cells, start time)
#
# =============================================================================

# Hook for custom argument parsing (override this in your main script)
if ! declare -f parse_custom_args > /dev/null; then
    parse_custom_args() {
        # Default: no custom args
        # Override this function in your main script to handle custom arguments
        # Arguments:
        #   $1 - The argument to parse
        # Return:
        #   0 if arg was handled, 1 if not
        return 1
    }
fi

# =============================================================================
# Argument Parsing
# =============================================================================

# Built-in arguments handled by the framework
cells=""        # Comma-separated list of cells to execute
tmux_mode=false # Whether to launch in a tmux session

# Save all original arguments (needed for tmux to pass through custom args)
original_args=("$@")

# Parse all command-line arguments
for arg in "$@"; do
    case $arg in
        --cell=*)
            # Extract cell names from --cell=configure,build,run
            cells="${arg#*=}"
            shift
            ;;
        --tmux)
            # Enable tmux mode
            tmux_mode=true
            shift
            ;;
        *)
            # Try custom argument parser (defined in your run script)
            # If it returns 1 (not handled), we just continue
            if ! parse_custom_args "$arg"; then
                # Unknown argument, keep it for potential error handling
                :
            fi
            shift
            ;;
    esac
done

# =============================================================================
# Hook for custom usage text (override this in your main script)
# =============================================================================
if ! declare -f show_custom_usage > /dev/null; then
    show_custom_usage() {
        # Default: no custom usage
        # Override this function to add custom usage text
        # Example:
        #   echo "[--build-type=Release|Debug] [--verbose]"
        # Return:
        #   String to append to usage line, or empty string
        :
    }
fi

# =============================================================================
# Auto-generated Usage / Help
# =============================================================================

# Show usage if no --cell argument provided
if [ -z "$cells" ]; then
    # Get custom usage text from hook (if defined in your run script)
    custom_usage=$(show_custom_usage)

    # Show usage line
    if [ -n "$custom_usage" ]; then
        echo "Usage: $0 --cell=<cell1,cell2,...> [--tmux] $custom_usage"
    else
        echo "Usage: $0 --cell=<cell1,cell2,...> [--tmux]"
    fi
    echo ""

    # Auto-extract and display available cells from the main script
    echo "Available cells:"
    grep -E "^# ===== CELL:" "$0" | sed 's/# ===== CELL: \(.*\) =====/  \1/'
    echo ""
    grep -A1 -E "^# ===== CELL:" "$0" | grep "^# Description:" | sed 's/# Description:/    →/'
    echo ""

    # Auto-generate example command with all cells
    available=$(grep -E "^# ===== CELL:" "$0" | sed 's/# ===== CELL: \(.*\) =====/\1/' | paste -sd,)
    echo "Example: $0 --cell=$available"
    if [ -n "$custom_usage" ]; then
        echo "         $0 --cell=$available --tmux $custom_usage"
    else
        echo "         $0 --cell=$available --tmux"
    fi
    exit 0
fi

# =============================================================================
# Tmux Session Launcher
# =============================================================================
#
# When --tmux flag is provided, this section:
# 1. Extracts project name from "# PROJECT: name" comment
# 2. Generates timestamped session name
# 3. Launches the script in a detached tmux session
# 4. Passes through all arguments except --tmux
# 5. Keeps shell open after completion for reviewing results
#
# =============================================================================

if [ "$tmux_mode" = true ]; then
    # Extract project name from "# PROJECT: name" comment in the main script
    PROJECT=$(grep "^# PROJECT:" "$0" | head -1 | sed 's/# PROJECT: //' | tr -d ' ')
    TIMESTAMP=$(date +%Y%m%d-%H%M%S)
    SESSION_NAME="${PROJECT}-${TIMESTAMP}"

    # Get absolute path to the main script
    # When this file is sourced, BASH_SOURCE[1] points to the calling script
    MAIN_SCRIPT="${BASH_SOURCE[1]:-$0}"
    SCRIPT_PATH="$(cd "$(dirname "$MAIN_SCRIPT")" && pwd)/$(basename "$MAIN_SCRIPT")"
    WORK_DIR="$PWD"

    # Reconstruct all arguments except --tmux (so custom args are preserved)
    reconstructed_args=""
    for arg in "${original_args[@]}"; do
        if [ "$arg" != "--tmux" ]; then
            reconstructed_args="$reconstructed_args $arg"
        fi
    done

    # Launch tmux session:
    # - cd to working directory (so tmux starts in the right place)
    # - Run the script with reconstructed args
    # - exec zsh to keep shell open after completion (change to bash if needed)
    # - Set large scrollback buffer (100000 lines) for long build outputs
    tmux new-session -d -s "$SESSION_NAME" "cd '$WORK_DIR' && $SCRIPT_PATH $reconstructed_args; exec zsh" \; \
        set-option -t "$SESSION_NAME" history-limit 100000

    echo "Tmux session started: $SESSION_NAME"
    echo "Scrollback: 100000 lines"
    echo "Attach: tmux attach -t $SESSION_NAME"
    exit 0
fi

# =============================================================================
# Cell Checking Function
# =============================================================================
#
# has_cell() - Check if a cell should be executed
#
# Usage in your run script:
#   if has_cell "configure"; then
#       # This code runs only if "configure" is in --cell=configure,build,run
#   fi
#
# =============================================================================

has_cell() {
    local cell_name="$1"

    # Check if the cell is in the comma-separated list
    # Uses pattern matching with added commas to avoid partial matches
    # Example: ",configure,build," contains ",configure,"
    if [[ ",${cells}," == *",${cell_name},"* ]]; then
        return 0  # Cell is enabled
    fi
    return 1  # Cell is not enabled
}

# =============================================================================
# Execution Context Header
# =============================================================================
#
# Display information about the current execution:
# - Working directory (useful when attached to tmux mid-execution)
# - Which cells are being executed
# - Start timestamp
#
# =============================================================================

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Working directory: $PWD"
echo "Cells: $cells"
echo "Started: $(date '+%Y-%m-%d %H:%M:%S')"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
