#!/bin/bash
# ======================================================================
#
# run_example.sh
#
# Example script showing how to use AcmBuildTool
#
# ======================================================================

# Set the base path to your compiled game directory
BASE_PATH="../../sku.0/sys.shared/compiled/game/"

# Check if the tool exists
if [ ! -f "bin/AcmBuildTool" ]; then
    echo "ERROR: AcmBuildTool not found!"
    echo "Please build the tool first by running: ./build.sh"
    exit 1
fi

# Check if base path exists
if [ ! -d "$BASE_PATH" ]; then
    echo "ERROR: Base path not found: $BASE_PATH"
    echo "Please update the BASE_PATH variable in this script to point to your compiled game directory."
    exit 1
fi

echo "Running ACM Build Tool..."
echo "Base Path: $BASE_PATH"
echo ""

# Run the tool
./bin/AcmBuildTool "$BASE_PATH"

# Check exit code
if [ $? -eq 0 ]; then
    echo ""
    echo "ACM build completed successfully!"
    echo ""
    echo "Output files:"
    echo "  - client/customization/asset_customization_manager.iff"
    echo "  - client/customization/customization_id_manager.iff"
else
    echo ""
    echo "ACM build failed! Check the error messages above."
    exit 1
fi
