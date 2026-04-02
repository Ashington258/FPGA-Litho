# Vitis HLS C Synthesis Script
# Run: vitis -s run_csynth.py

from vitis import Vitis

# Create Vitis object
vitis_obj = Vitis()

# Set workspace
vitis_obj.set_workspace("hls_top_simple")

# Get HLS component
comp = vitis_obj.get_component("hls_top_simple")

# Run C Synthesis
print("Starting C Synthesis...")
comp.csynth()
print("C Synthesis completed!")

# Generate report
print("\n========================================")
print("C Synthesis Results:")
print("========================================")

# Get synthesis results
report = comp.get_report("synthesis")
if report:
    print(report)

# Close Vitis
vitis_obj.close()
