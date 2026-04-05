# 🎯 BRAM硬件验证快速参考卡

**FPGA器件**: xcku3p-ffvb676-2-e (已连接JTAG)  
**Bitstream**: 已生成 (14.72MB)  
**验证目标**: 测试BRAM算法硬件功能

---

## ⚡ 快速执行 (三种方法)

### 方法1: 一键执行脚本 ⭐推荐

```bash
cd /root/project/FPGA/vitis/FPGA-Litho
bash script/verify/run_board_verify.sh
```

脚本会自动：
- ✅ 检查bitstream文件
- ✅ 检查Vivado环境
- ✅ 询问是否需要重新下载bitstream
- ✅ 执行完整硬件验证

---

### 方法2: Vivado Tcl Console

**在Vivado GUI中**:
```
1. 打开Vivado
2. Tools → Tcl Console
3. 输入:
   cd /root/project/FPGA/vitis/FPGA-Litho
   source script/verify/board_verify_complete_v2.tcl
```

---

### 方法3: 终端直接执行

```bash
vivado -mode tcl <<'EOF'
cd /root/project/FPGA/vitis/FPGA-Litho
source script/verify/board_verify_complete_v2.tcl
EOF
```

---

## 📋 测试流程预览

验证脚本执行7个步骤：

| Step | 功能 | 验证目标 |
|------|------|---------|
| 0 | 硬件初始化 | JTAG连接、器件识别、AXI接口 |
| 1 | 内核状态 | AP_CTRL寄存器、内核空闲状态 |
| 2 | 复位测试 | RESET操作执行 |
| 3 | 参数配置 | N/M/Nx/Ny参数写入 |
| 4 | 数据加载 | LOAD_SOURCE操作 |
| 5 | 计算测试 | COMPUTE_SOCS操作 |
| 6 | 结果读取 | READ_IMG_OUT操作 |
| 7 | 寄存器检查 | 所有关键寄存器状态 |

---

## ✅ 成功标志

测试通过的特征：

- ✅ **硬件连接**: 
  - JTAG识别器件 `xcku3p_0`
  - AXI接口可用 `get_hw_axis *`

- ✅ **内核响应**: 
  - `AP_CTRL = 0x04` (AP_IDLE状态)
  - 写入值能正确读回

- ✅ **操作执行**: 
  - 内核完成标志 `AP_DONE=1`
  - 计算返回有效数据

---

## ⚠️ 常见问题快速修复

### 问题: 未找到AXI接口

```bash
# 检查Block Design中是否有jtag_axi IP
vivado -mode tcl <<'EOF'
open_project /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.xpr
open_bd_design design_1
get_bd_cells jtag_axi*
EOF
```

**如果不存在**:
需要在Block Design中添加 `jtag_axi` IP并重新生成bitstream

---

### 问题: 地址访问失败

```bash
# 查看实际地址映射
vivado -mode tcl <<'EOF'
open_project /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.xpr
open_bd_design design_1
get_bd_addr_segs -of_objects [get_bd_cells hls_litho_system_bram_1]
EOF
```

---

## 📊 保存验证日志

```bash
# 保存完整日志到文件
bash script/verify/run_board_verify.sh > validation_log.txt 2>&1

# 查看日志
cat validation_log.txt
```

---

## 🔍 下一步工作

验证通过后：

1. **完整数据测试**
   ```bash
   cd host
   python test_bram_interface.py
   ```

2. **性能测量**
   - 记录计算时间
   - 计算吞吐量

3. **精度验证**
   - 对比HLS C仿真结果

---

## 📁 关键文件位置

```
Bitstream:
  /root/project/FPGA/vivado/test_bram_litho/.../design_1_wrapper.bit

验证脚本:
  /root/project/FPGA/vitis/FPGA-Litho/script/verify/board_verify_complete_v2.tcl

测试数据:
  /root/project/FPGA/vitis/FPGA-Litho/data/bram_test/

文档:
  /root/project/FPGA/vitis/FPGA-Litho/script/verify/README_BOARD_VERIFY.md
```

---

## 🆘 需要帮助？

- 查看 `script/verify/README_BOARD_VERIFY.md` 详细指南
- 检查 `doc/BRAM_BOARD_VALIDATION_GUIDE.md` 完整流程
- 查看 Vivado Tcl Console 输出的错误信息

---

**快速开始**: `bash script/verify/run_board_verify.sh` 🚀