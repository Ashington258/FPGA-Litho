#!/bin/bash
# K-Litho BRAM Version xclbin Build Script
# Phase 6F: Compile XO to xclbin
#
# @author K-Litho Team
# @date 2026-04-04

set -e

# Configuration
VITIS_PATH="/root/AMDDesignTools/2025.2/Vitis/bin"
WORK_DIR="/root/project/FPGA/vitis/FPGA-Litho"
XO_FILE="hls_litho_system_bram.xo"
KERNEL_NAME="hls_litho_system_bram"
BUILD_DIR="vitis_build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================"
echo "K-Litho BRAM xclbin Build"
echo "Phase 6F: Hardware Compilation"
echo "========================================"

# Step 1: Check XO file exists
echo ""
echo -e "${YELLOW}[Step 1] Checking XO file...${NC}"
if [ ! -f "$WORK_DIR/$XO_FILE" ]; then
    echo -e "${RED}[ERROR] XO file not found: $XO_FILE${NC}"
    echo "Run HLS package first: vitis-run --tcl script/run_package_bram.tcl"
    exit 1
fi
echo -e "${GREEN}[OK] XO file found: $XO_FILE ($(du -h $WORK_DIR/$XO_FILE | cut -f1))${NC}"

# Step 2: Create build directory
echo ""
echo -e "${YELLOW}[Step 2] Creating build directory...${NC}"
mkdir -p "$WORK_DIR/$BUILD_DIR"
echo -e "${GREEN}[OK] Build directory: $BUILD_DIR${NC}"

# Step 3: Link XO files for v++
echo ""
echo -e "${YELLOW}[Step 3] Preparing kernel link...${NC}"

# Create minimal link configuration file
LINK_CONFIG="$WORK_DIR/$BUILD_DIR/link_config.ini"
cat > "$LINK_CONFIG" << EOF
[Connectivity]
# BRAM-only design - no external memory connections
EOF

echo -e "${GREEN}[OK] Link config created${NC}"

# Step 4: v++ link (create xclbin)
echo ""
echo -e "${YELLOW}[Step 4] Running v++ link...${NC}"
echo "This may take several minutes..."

# v++ link requires platform
# Check if platform is available
echo "Checking available platforms..."
PLATFORMS=$($VITIS_PATH/v++ --list-platforms 2>&1 | grep -E "xcku|kintex" || echo "")

if [ -z "$PLATFORMS" ]; then
    echo -e "${YELLOW}[INFO] No xcku3p platform installed${NC}"
    echo ""
    echo "v++ link requires a target platform. Options:"
    echo ""
    echo "1. Install platform package:"
    echo "   - Download from Xilinx/AMD website"
    echo "   - Platform name format: xilinx_ku3p_<board>_<version>"
    echo ""
    echo "2. Or use generic Vitis platform:"
    echo "   $VITIS_PATH/v++ -l -t hw_emu -k $KERNEL_NAME -o ${KERNEL_NAME}.xclbin $XO_FILE --config $LINK_CONFIG"
    echo ""
    echo "3. For Vivado integration (alternative):"
    echo "   - Import HLS IP: hls_litho_system_bram_proj/solution1/impl/ip/"
    echo "   - Create Block Design in Vivado"
    echo "   - Generate bitstream"
    echo ""
    # Try generic compile test
    echo -e "${YELLOW}[Attempting generic v++ link test...]${NC}"
    $VITIS_PATH/v++ -l -t hw_emu -k $KERNEL_NAME -o ${KERNEL_NAME}.hw_emu.xclbin \
        $XO_FILE --config $LINK_CONFIG 2>&1 | tee "$WORK_DIR/$BUILD_DIR/v++_link.log" || {
        echo -e "${RED}[ERROR] v++ link failed - platform required${NC}"
    }
else
    echo -e "${GREEN}[OK] Found platform: $PLATFORMS${NC}"
    # Use first matching platform
    PLATFORM=$(echo "$PLATFORMS" | head -1 | awk '{print $1}')
    echo "Using platform: $PLATFORM"
    
    $VITIS_PATH/v++ -l -t hw -f "$PLATFORM" -k $KERNEL_NAME -o ${KERNEL_NAME}.xclbin \
        $XO_FILE --config $LINK_CONFIG 2>&1 | tee "$WORK_DIR/$BUILD_DIR/v++_link.log"
fi

# Step 5: Summary
echo ""
echo "========================================"
echo "Build Summary"
echo "========================================"
echo ""
echo "Generated files:"
ls -lh "$WORK_DIR/$BUILD_DIR/" 2>/dev/null || echo "Build directory empty"

echo ""
echo "Next steps for hardware deployment:"
echo "1. Install target platform package"
echo "2. Run: v++ -l --target hw --platform <platform> -o litho_bram.xclbin hls_litho_system_bram.xo"
echo "3. Test with Python driver: python host/litho_host_bram.py"

echo ""
echo -e "${GREEN}[DONE] Phase 6F setup complete${NC}"