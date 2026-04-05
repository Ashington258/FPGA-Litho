# Complete Vivado HLS IP Update Procedure
# This script properly updates HLS IP in Vivado Block Design

puts "=========================================="
puts "Vivado HLS IP Update Procedure"
puts "=========================================="

# Step 1: Open Vivado project
puts "\n[Step 1] Opening Vivado project..."
open_project /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.xpr

# Step 2: Update IP Catalog
puts "\n[Step 2] Updating IP Catalog..."
set ip_repo_path "/root/project/FPGA/vitis/FPGA-Litho/hls_litho_system_bram_proj/solution1/impl/ip"
set_property ip_repo_paths $ip_repo_path [current_project]
update_ip_catalog -rebuild

puts "IP Catalog updated with new HLS IP"

# Step 3: Check if HLS IP needs upgrade
puts "\n[Step 3] Checking IP status..."
set bd_design [open_bd_design /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.srcs/sources_1/bd/design_1/design_1.bd]

# Get HLS IP instance
set hls_ip [get_bd_cells -filter "NAME == hls_litho_system_bram_0"]
if {$hls_ip != ""} {
    puts "Found HLS IP instance: $hls_ip"
    
    # Lock/Unlock the BD design for modification
    set_property NEEDS_REFRESH true $bd_design
    
    # Upgrade the IP if needed
    report_ip_status -name ip_status_report
    
    # Check for upgrade requirement
    set ip_status [get_property IP_STATUS $hls_ip]
    puts "Current IP status: $ip_status"
    
    if {[string match "*upgrade*" $ip_status] || [string match "*refresh*" $ip_status]} {
        puts "IP needs upgrade/refresh, updating..."
        upgrade_ip $hls_ip
        puts "HLS IP upgraded"
    } else {
        puts "IP does not need automatic upgrade"
        puts "Removing and re-adding IP to force update..."
        
        # Remove old IP instance
        delete_bd_objs $hls_ip
        
        # Re-create IP instance
        create_bd_cell -type ip -vlnv xilinx.com:hls:hls_litho_system_bram:1.0 hls_litho_system_bram_0
        puts "HLS IP re-created"
        
        # Need to reconnect in Vivado GUI or use connection script
        puts "⚠️  WARNING: IP was removed and re-added"
        puts "   You need to manually reconnect it in Vivado Block Design GUI"
    }
} else {
    puts "❌ HLS IP instance not found in Block Design!"
}

# Step 4: Regenerate Block Design outputs
puts "\n[Step 4] Regenerating Block Design..."
generate_target all $bd_design
puts "Block Design targets regenerated"

# Step 5: Regenerate synthesis outputs
puts "\n[Step 5] Regenerating synthesis outputs..."
export_ip_user_files $bd_design -no_ip_version

# Step 6: Reset and run synthesis
puts "\n[Step 6] Running synthesis..."
reset_run synth_1
launch_runs synth_1 -jobs 4
wait_on_run synth_1

set synth_status [get_property STATUS [get_runs synth_1]]
puts "Synthesis status: $synth_status"

# Step 7: Reset and run implementation
puts "\n[Step 7] Running implementation..."
reset_run impl_1
launch_runs impl_1 -jobs 4
wait_on_run impl_1

# Step 8: Generate bitstream
puts "\n[Step 8] Generating bitstream..."
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

set impl_status [get_property STATUS [get_runs impl_1]]
puts "Implementation status: $impl_status"

puts "\n=========================================="
puts "Update Procedure Complete"
puts "=========================================="

if {$impl_status == "write_bitstream Complete!"} {
    puts "✅ Bitstream successfully generated with updated HLS IP"
    set bitstream "/root/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/design_1_wrapper.bit"
    puts "Bitstream location: $bitstream"
} else {
    puts "❌ Implementation did not complete successfully"
}

close_project
exit