@echo off
echo ================================================
echo Loading AMD Vitis 2025.2 Environment...
echo ================================================

call "C:\AMDDesignTools\2025.2\Vitis\settings64.bat"

echo.
echo Vitis environment loaded successfully.
echo Current directory: %CD%
echo.

cd /d "E:\1.Project\4.FPGA\vitis\FPGA_Litho\FPGA-Litho"

echo.
echo You can now run:
echo vitis-run --mode hls --csim --config script\hls_config.cfg --work_dir hls_top_simple
echo ================================================

cmd /k