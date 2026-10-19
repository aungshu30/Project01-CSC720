#!/bin/bash
# ==============================================================================
# @file record_env.sh
# @author Sirisha Konda
# @course CSC 720: Storage Systems - Milestone 1
# @brief Captures system and environment metadata to ensure reproducible benchmarks.
# ==============================================================================

OUTPUT_FILE="run_metadata.txt"

echo "=========================================="
echo "  Collecting System Environment Data      "
echo "=========================================="

# Initialize or overwrite the metadata report file
echo "=== System & Environment Metadata ===" > "$OUTPUT_FILE"
echo "Execution Timestamp: $(date)" >> "$OUTPUT_FILE"
echo "Target Hostname: $(hostname)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Capture Linux Kernel Version
echo "--- Kernel Version ---" >> "$OUTPUT_FILE"
uname -r >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Capture GCC Compiler Version
echo "--- GCC Version ---" >> "$OUTPUT_FILE"
gcc --version | head -n 1 >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Capture liburing Version (checks pkg-config with fallback)
echo "--- liburing Version ---" >> "$OUTPUT_FILE"
if pkg-config --modversion liburing >/dev/null 2>&1; then
    pkg-config --modversion liburing >> "$OUTPUT_FILE"
else
    echo "liburing linked directly from source / header path" >> "$OUTPUT_FILE"
fi
echo "" >> "$OUTPUT_FILE"

# Capture current Git commit hash for version tracking
echo "--- Git Repository Status ---" >> "$OUTPUT_FILE"
if git rev-parse HEAD >/dev/null 2>&1; then
    git rev-parse HEAD >> "$OUTPUT_FILE"
else
    echo "Not a git repository" >> "$OUTPUT_FILE"
fi

echo "[Success] Environment metadata successfully written to '$OUTPUT_FILE'."