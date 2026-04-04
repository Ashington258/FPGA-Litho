# HLS C Synthesis Tcl Script
# Run: vitis-run --mode hls --tcl --work_dir hls_top_simple script/run_csynth.tcl

# Apply config from file
source script/hls_config.cfg

# Run C Synthesis
puts "=========================================="
puts "Running C Synthesis..."
puts "=========================================="

csynth_design

puts "=========================================="
puts "C Synthesis Completed!"
puts "=========================================="

exit