# BRAM算法板级硬件验证指南

**阶段**: Phase 6 - 板级验证  
**日期**: 2026-04-04  
**状态**: Bitstream已生成，待硬件验证

---

## 📋 验证流程概览

```
1. 硬件环境准备
   ├─ 确认FPGA板卡类型
   ├─ 安装XRT驱动 (Alveo) 或 Vitis运行时 (Zynq)
   └─ 连接硬件设备

2. 生成硬件二进制文件
   ├─ Alveo卡: 生成 .xclbin 文件
   └─ Zynq板: 使用 .bit + .hdf 文件

3. 运行测试程序
   ├─ TCC模式验证
   └─ SOCS模式验证

4. 性能测试与分析
   ├─ 执行时间测量
   ├─ 吞吐量计算
   └─ 与CPU对比
```

---

## 1️⃣ 硬件环境准备

### 选项A: Alveo加速卡 (推荐)

**支持的Alveo卡**:
- Alveo U200, U250, U280
- Alveo U50, U55C

**环境要求**:
```bash
# 检查XRT版本
xbutil version
# 需要XRT 2022.1+

# 检查设备
xbutil scan
# 应显示FPGA设备信息

# 安装XRT (如果未安装)
# Ubuntu/RedHat:
# https://www.xilinx.com/products/boards-and-kits/alveo.html
```

### 选项B: Zynq UltraScale+ MPSoC

**支持的Zynq板**:
- ZCU102, ZCU104, ZCU106
- ZCU111 (RFSoC)

**环境要求**:
```bash
# 安装Vitis嵌入式开发环境
# 需要 Vitis 2022.1+

# 准备SD卡启动镜像
# 或使用JTAG调试模式
```

### 选项C: 无硬件环境 - Mock验证

如果暂时没有硬件，可以使用Mock驱动验证接口逻辑：

```bash
cd /root/project/FPGA/vitis/FPGA-Litho/host

# 运行Mock测试
python test_bram_interface.py

# 预期输出:
# ✓ BRAM接口模拟测试通过
# ✓ 数据加载验证通过
# ✓ 计算逻辑验证通过
```

---

## 2️⃣ 生成硬件二进制文件

### Alveo卡: 生成XCLBIN

**当前状态**: 已有bitstream，需要转换为xclbin

```bash
# 1. 检查Vivado工程
ls -lh /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/design_1_wrapper.bit
# 输出: 15M bitstream文件

# 2. 生成XCLBIN (方法1: 使用Vitis v++)
cd /root/project/FPGA/vitis/FPGA-Litho

# 创建link配置文件 (如果不存在)
cat > vitis_build/link_config_bram.ini << 'EOF'
[connectivity]
# 流接口连接配置 (如果有)

[advanced]
# 高级参数
param=compiler.addOutputTypes=hw_emu
EOF

# 编译硬件二进制
v++ --link \
  --target hw \
  --platform xilinx_u200_gen3x16_xdma_1_20211020 \
  --kernel hls_litho_system_bram \
  --output hls_litho_system_bram.xclbin \
  vivado://design_1_wrapper \
  --config vitis_build/link_config_bram.ini

# 3. 方法2: 从Vivado导出 (更简单)
# 在Vivado中:
# File -> Export -> Export Hardware
# Include Bitstream: Yes
# 导出位置: vitis_build/hardware/

# 然后在Vitis中创建Platform Project
```

**简化版生成脚本**:

创建文件 `script/build/generate_xclbin.sh`:

```bash
#!/bin/bash
# 为Alveo卡生成xclbin文件

VIVADO_BIT="/root/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/design_1_wrapper.bit"
OUTPUT_XCLBIN="hls_litho_system_bram.xclbin"

# 检查输入文件
if [ ! -f "$VIVADO_BIT" ]; then
    echo "Error: Bitstream not found at $VIVADO_BIT"
    exit 1
fi

# 方法1: 使用xclbinutil工具 (XRT自带)
echo "Generating XCLBIN from bitstream..."
xclbinutil --input "$VIVADO_BIT" --output "$OUTPUT_XCLBIN" \
  --platform xilinx_u200_gen3x16_xdma_1_20211020 \
  --kernel hls_litho_system_bram

# 方法2: 如果xclbinutil不支持，需要完整的Vitis链接流程
# v++ --link ...

echo "Generated: $OUTPUT_XCLBIN"
```

### Zynq板: 准备启动镜像

```bash
# 1. 导出硬件定义 (在Vivado中)
# File -> Export -> Export Hardware
# Include Bitstream: Yes
# 输出: design_1_wrapper.hdf

# 2. 生成设备树 (可选)
# hsi open_hw_design design_1_wrapper.hdf
# set_repo_path <device-tree-repo>
# create_device_tree -dts_name system.dts

# 3. 编译设备树
# dtc -I dts -O dtb -o system.dtb system.dts

# 4. 准备启动文件
# boot.bin (FSBL + bitstream + u-boot)
# image.ub (kernel + device tree)
```

---

## 3️⃣ 运行测试程序

### Alveo卡测试流程

#### Step 1: 准备测试数据

```bash
cd /root/project/FPGA/vitis/FPGA-Litho

# 检查测试数据
ls -lh data/bram_test/
# 输出:
# source.bin (2.0K) - 光源数据
# mask.bin (2.0K) - 掩模数据
# kernels.bin (1.6K) - SOCS核
# scales.bin (16B) - SOCS权重
# config.json - 测试配置

# 查看配置
cat data/bram_test/config.json
{
  "Lx": 16, "Ly": 16,
  "Nx": 3, "Ny": 3,
  "srcSize": 16,
  "nkernels": 4
}
```

#### Step 2: 运行Python驱动程序

```bash
cd host

# 测试TCC模式
python litho_host_bram.py \
  --xclbin ../hls_litho_system_bram.xclbin \
  --mode 1 \
  --Lx 16 --Ly 16 \
  --Nx 3 --Ny 3 \
  --srcSize 16 \
  --verbose

# 测试SOCS模式
python litho_host_bram.py \
  --xclbin ../hls_litho_system_bram.xclbin \
  --mode 2 \
  --Lx 16 --Ly 16 \
  --nkernels 4 \
  --verbose
```

**预期输出**:

```
✓ Device: xilinx_u200_gen3x16_xdma_1_20211020
✓ Loaded xclbin: hls_litho_system_bram.xclbin
✓ Kernel: hls_litho_system_bram

[TCC Mode Test]
Loading source data: 256 elements... (0.15s)
Loading mask data: 256 elements... (0.12s)
Executing TCC computation...
Result: 70 non-zero TCC elements
Read imgf: 256 elements
✓ TCC mode validation PASSED

[Performance]
Execution time: 0.234ms
Throughput: 1.09M elem/s
```

#### Step 3: 运行C++驱动程序 (可选)

```bash
# 编译C++主机程序
cd host
make

# 运行
./litho_host_bram \
  --xclbin ../hls_litho_system_bram.xclbin \
  --mode 1 \
  --runs 10 \
  --verbose
```

### Zynq板测试流程

#### Step 1: 部署到SD卡

```bash
# 1. 准备SD卡分区
# Boot partition (FAT32): 存放boot.bin, image.ub
# Root partition (EXT4): Linux文件系统

# 2. 复制文件
mount /dev/sdX1 /mnt/boot
mount /dev/sdX2 /mnt/root

# 复制启动文件
cp boot.bin /mnt/boot/
cp image.ub /mnt/boot/

# 复制应用程序和数据
cp -r host/litho_host_bram /mnt/root/
cp -r data/bram_test /mnt/root/

umount /mnt/boot /mnt/root
```

#### Step 2: 运行测试

```bash
# 在Zynq板上执行
cd /root/bram_test

# 运行TCC模式
./litho_host_bram \
  --mode 1 \
  --Lx 16 --Ly 16 \
  --Nx 3 --Ny 3 \
  --verbose

# 运行SOCS模式
./litho_host_bram \
  --mode 2 \
  --Lx 16 --Ly 16 \
  --nkernels 4 \
  --verbose
```

---

## 4️⃣ 验证测试用例

### 测试数据说明

根据 `data/bram_test/config.json`:

**TCC模式测试**:
- 数据尺寸: 16x16 (Lx=16, Ly=16)
- TCC矩阵大小: 7x7 (Nx=3, Ny=3)
- 光源尺寸: 16x16
- 预期结果: 70个非零TCC元素

**SOCS模式测试**:
- 数据尺寸: 16x16
- SOCS核数量: 4个
- 每个核大小: 7x7
- 权重: [0.25, 0.25, 0.25, 0.25]

### 验证检查点

#### ✅ TCC模式验证

```python
# 预期输出检查
assert result['tcc_nonzero_count'] == 70
assert result['imgf_size'] == (16, 16)
assert np.all(np.isfinite(result['imgf']))  # 无NaN/Inf

# 精度检查 (与C仿真对比)
assert np.max(np.abs(result['imgf'] - expected_imgf)) < 1e-3
```

#### ✅ SOCS模式验证

```python
# 预期输出检查
assert result['img_out_size'] == (29, 29)  # (2*Nx+Lx-1, 2*Ny+Ly-1)
assert np.all(np.isfinite(result['img_out']))

# 精度检查
assert np.max(np.abs(result['img_out'] - expected_img_out)) < 1e-3
```

---

## 5️⃣ 性能测试与分析

### 测量指标

#### 执行时间测量

```python
# Python代码示例
import time

# TCC模式
start = time.time()
kernel.execute_operation(OP_COMPUTE_TCC, mode=1, Lx=16, Ly=16, Nx=3, Ny=3, srcSize=16)
elapsed_tcc = time.time() - start

# SOCS模式
start = time.time()
kernel.execute_operation(OP_COMPUTE_SOCS, mode=2, Lx=16, Ly=16, nkernels=4)
elapsed_socs = time.time() - start

print(f"TCC execution time: {elapsed_tcc*1000:.2f} ms")
print(f"SOCS execution time: {elapsed_socs*1000:.2f} ms")
```

#### 性能计数器 (Alveo)

```bash
# 使用xbutil监控性能
xbutil top -d 0

# 查看详细性能计数器
xbutil query -d 0

# 查看计算单元状态
xbutil validate -d 0
```

### 性能基准

**预期性能** (基于HLS估算):

| 模块 | 时钟频率 | 目标加速比 | 预期执行时间 (16x16) |
|------|----------|-----------|---------------------|
| calcTCC | 342 MHz | 100-500x | ~0.1-0.5 ms |
| calcImage | 274 MHz | 50-200x | ~0.2-0.8 ms |
| calcSOCS | 290 MHz | 30-100x | ~0.15-0.5 ms |

**CPU对比基准** (Intel i7-9700K):

```bash
# 运行CPU基准测试
cd host
python benchmark_cpu.py

# 预期CPU时间 (16x16):
# TCC: ~50-100 ms
# calcImage: ~30-60 ms
# SOCS: ~20-40 ms
```

**加速比计算**:

```
加速比 = CPU执行时间 / FPGA执行时间

目标加速比:
- TCC模式: 100x - 500x
- SOCS模式: 30x - 100x
```

---

## 6️⃣ 故障排查

### 问题1: 找不到设备

```bash
# 检查XRT服务
sudo systemctl status xrt

# 重启XRT
sudo systemctl restart xrt

# 检查设备
xbutil scan
```

### 问题2: 加载xclbin失败

```bash
# 检查xclbin格式
xclbinutil --info hls_litho_system_bram.xclbin

# 检查平台兼容性
xbutil query -d 0 | grep "Platform"

# 重新生成xclbin (如果需要)
v++ --link ...
```

### 问题3: 内核执行超时

```python
# 增加超时时间
kernel = BRAMKernel(device, timeout=10000)  # 10秒

# 检查内核状态
status = kernel.get_status()
print(f"Kernel status: {status}")
```

### 问题4: 数据加载慢

```python
# 使用批量加载优化
kernel.load_source_batch(source_data, Lx=16, Ly=16)
kernel.load_mask_batch(mask_data, Lx=16, Ly=16)

# 预期加载速度: >1000 elem/s
```

### 问题5: 结果不正确

```bash
# 1. 检查HLS C仿真结果
cd /root/project/FPGA/vitis/FPGA-Litho
cat hls_litho_system_bram_proj/solution1/csim/report/hls_litho_system_bram_csim.log

# 2. 对比RTL仿真结果
cat hls_litho_system_bram_proj/solution1/sim/report/hls_litho_system_bram_cosim.log

# 3. 检查数据格式
python -c "
import numpy as np
source = np.fromfile('data/bram_test/source.bin', dtype=np.complex64)
print(f'Source shape: {source.shape}')
print(f'Source dtype: {source.dtype}')
print(f'Sample values: {source[:5]}')
"
```

---

## 7️⃣ 下一步工作

### 性能优化

- [ ] PCIe数据传输优化 (使用DMA)
- [ ] 多核并行测试
- [ ] 不同数据尺寸测试 (32x32, 64x64)
- [ ] 功耗测量与分析

### 功能扩展

- [ ] 支持更大的Nx值 (需要优化BRAM存储)
- [ ] 添加更多光源类型
- [ ] 支持动态配置参数
- [ ] 集成到完整光刻流程

### 文档完善

- [ ] 性能测试报告
- [ ] 用户手册
- [ ] API文档

---

## 8️⃣ 联系支持

**项目仓库**: https://github.com/Ashington258/FPGA-Litho

**问题反馈**: 
- GitHub Issues: https://github.com/Ashington258/FPGA-Litho/issues
- 邮件: k-litho@example.org

**相关文档**:
- [BRAM接口映射](../BRAM_INTERFACE_MAPPING.md)
- [主机程序说明](../../host/README.md)
- [项目总结](../PHASE_SUMMARY_REPORT.md)

---

**更新记录**:
- 2026-04-04: 初始版本，Bitstream已生成
- 待更新: 板级验证结果