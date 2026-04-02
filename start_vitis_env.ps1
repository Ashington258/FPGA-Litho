# 临时跳过 Anaconda conda hook（避免报错）
$env:CONDA_EXE = $null

Write-Host "========================================" -ForegroundColor Green
Write-Host "Loading AMD Vitis 2025.2 Environment..." -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green

# 调用正确的 settings64.bat（你的真实路径）
& "C:\AMDDesignTools\2025.2\Vitis\settings64.bat"

Write-Host "Vitis environment loaded successfully." -ForegroundColor Green
Write-Host "Current directory: $(Get-Location)" -ForegroundColor Green

# 切换到你的 HLS Component 目录
Set-Location "E:\1.Project\4.FPGA\vitis\FPGA_Litho\FPGA-Litho"

Write-Host ""
Write-Host "You can now run commands such as:" -ForegroundColor Yellow
Write-Host "vitis-run --mode hls --csim --config script\hls_config.cfg --work_dir hls_top_simple" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Green