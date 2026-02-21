#!/bin/bash
# ======================================================================
# buildACM.sh - Linux shell script to run buildACM.pl
# ======================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Run the Perl script with verbose output
perl "${SCRIPT_DIR}/buildACM.pl" --verbose "$@"

EXIT_CODE=$?

if [ $EXIT_CODE -ne 0 ]; then
    echo ""
    echo "Build failed with error code $EXIT_CODE"
    exit $EXIT_CODE
fi

echo ""
echo "Build completed successfully!"
exit 0
