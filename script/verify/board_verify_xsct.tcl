# K-Litho BRAM Verification using XSCT
#
# XSCT (Xilinx Software Command-line Tool) provides low-level JTAG access
# 
# Usage:
#   xsct
#   source board_verify_xsct.tcl

puts "=========================================="
puts "K-Litho BRAM Verification via XSCT"
puts "=========================================="

# Step 1: Connect to hardware
puts "\n[Step 1] Connecting to hardware server..."
connect

# Step 2: List targets
puts "\n[Step 2] Listing targets..."
targets

# Step 3: Select the Kintex device
puts "\n[Step 3] Selecting xcku3p device..."
targets -set -filter {name =~ "*xcku3p*"}

# Step 4: Stop the processor (if any) and configure
puts "\n[Step 4] Device info..."
puts "IDCODE: [jtag idcode]"
puts "IR Length: [jtag irlen]"

# =============================================
# Memory Access via JTAG
# =============================================
# The jtag_axi IP in the design creates an AXI master interface
# that can be controlled via JTAG

puts "\n=========================================="
puts "HLS Kernel Register Map"
puts "=========================================="
puts "Base Address: 0x40000000"
puts ""
puts "| Offset | Name       | Description      |"
puts "|--------|------------|------------------|"
puts "| 0x00   | AP_CTRL    | Control/Status   |"
puts "| 0x04   | GIER       | Global Interrupt |"
puts "| 0x08   | IP_IER     | IP Interrupt En  |"
puts "| 0x0C   | IP_ISR     | IP Interrupt St  |"
puts "| 0x10   | -          | Reserved         |"
puts "| 0x14   | -          | Reserved         |"
puts "| 0x18   | -          | Reserved         |"
puts "| 0x1C   | OPERATION  | Operation Code   |"
puts "| 0x20   | -          | Reserved         |"
puts "| 0x24   | IDX_LOW    | Index Low        |"
puts "| 0x28   | IDX_HIGH   | Index High       |"
puts "| 0x2C   | VAL_IN     | Input Value      |"
puts "| 0x30   | VAL_OUT    | Output Value     |"
puts "| 0x40   | N_OFFSET   | N dimension      |"
puts "| 0x48   | M_OFFSET   | M dimension      |"
puts "=========================================="

# =============================================
# Using mrd/mwr commands
# =============================================
# Note: mrd/mwr access memory through JTAG
# The jtag_axi IP translates JTAG commands to AXI transactions

puts "\n[Step 5] Testing memory access..."

# Try reading AP_CTRL register
set BASE_ADDR 0x40000000

puts "\nAttempting to read AP_CTRL (0x[format %08X $BASE_ADDR])..."

# Method 1: Direct memory read (if supported by design)
catch {
    set ap_ctrl [mrd $BASE_ADDR]
    puts "AP_CTRL = $ap_ctrl"
} result

if {[string match "*error*" $result] || [string match "*Error*" $result]} {
    puts "\nDirect memory access failed."
    puts "This may require:"
    puts "  1. jtag_axi IP to be configured correctly"
    puts "  2. Design clock to be running"
    puts "  3. Valid AXI transaction handling"
    
    puts "\nAlternative: Use Vivado Hardware Manager"
    puts "  1. Open Vivado"
    puts "  2. Open Hardware Manager"
    puts "  3. Connect to localhost:3121"
    puts "  4. Device -> Run JTAG to AXI Master"
}

# =============================================
# Test Sequence (if access works)
# =============================================
proc test_kernel {} {
    global BASE_ADDR
    
    puts "\n=========================================="
    puts "Kernel Test Sequence"
    puts "=========================================="
    
    # Register addresses
    set AP_CTRL   [expr {$BASE_ADDR + 0x00}]
    set OPERATION [expr {$BASE_ADDR + 0x1C}]
    set IDX       [expr {$BASE_ADDR + 0x24}]
    set VAL_IN    [expr {$BASE_ADDR + 0x2C}]
    set VAL_OUT   [expr {$BASE_ADDR + 0x30}]
    set N         [expr {$BASE_ADDR + 0x40}]
    set M         [expr {$BASE_ADDR + 0x48}]
    
    puts "Step 1: Read AP_CTRL (should be 0x00 or 0x04)"
    # mrd $AP_CTRL
    
    puts "\nStep 2: Set parameters (N=64, M=64)"
    # mwr $N 64
    # mwr $M 64
    
    puts "\nStep 3: Set operation (0=SOC, 1=SHIFT, 2=TCC)"
    # mwr $OPERATION 0
    
    puts "\nStep 4: Set index and input value"
    # mwr $IDX 0
    # mwr $VAL_IN 0
    
    puts "\nStep 5: Start kernel (write AP_START=1)"
    # mwr $AP_CTRL 1
    
    puts "\nStep 6: Wait for completion"
    puts "Poll AP_CTRL until AP_DONE (bit 1) is set"
    # while {[expr {[mrd $AP_CTRL] & 2}] == 0} {
    #     after 10
    # }
    
    puts "\nStep 7: Read output"
    # mrd $VAL_OUT
    
    puts "\n=========================================="
    puts "Test complete!"
    puts "=========================================="
}

# =============================================
# Alternative: Use XSDB for JTAG debugging
# =============================================
# XSDB provides lower-level JTAG access

puts "\n=========================================="
puts "Alternative Methods"
puts "=========================================="
puts ""
puts "If XSCT memory access fails, try:"
puts ""
puts "Method 1: XSDB Debugger"
puts "  xsdb"
puts "  connect"
puts "  targets -set -filter {name =~ '*xcku3p*'}"
puts "  # Use low-level JTAG commands"
puts ""
puts "Method 2: Vivado Hardware Manager GUI"
puts "  - Open Vivado Hardware Manager"
puts "  - Connect to JTAG server"
puts "  - Use 'Run JTAG to AXI Master' feature"
puts ""
puts "Method 3: Add ILA to design"
puts "  - Add ILA core in Vivado BD"
puts "  - Connect to kernel signals"
puts "  - Regenerate bitstream"
puts "  - Use Hardware Manager to monitor"
puts "=========================================="

# Run test if access is available
# test_kernel