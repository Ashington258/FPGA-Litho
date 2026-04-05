# HLS IP手动更新指南

## 问题说明
当HLS IP参数修改后重新导出，Vivado Block Design中的IP需要手动更新才能生效。

## 手动更新步骤

### Step 1: 打开Vivado项目
```bash
vivado /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.xpr
```

### Step 2: 更新IP Catalog
1. 在Vivado GUI中，点击 **Tools → Settings → IP → Repository**
2. 确认HLS IP Repository路径：
   ```
   /root/project/FPGA/vitis/FPGA-Litho/hls_litho_system_bram_proj/solution1/impl/ip
   ```
3. 点击 **Update IP Catalog**

### Step 3: 检查IP状态
1. 打开Block Design：**Sources → Design Sources → design_1**
2. 点击 **Report IP Status** 按钮（在Block Design窗口工具栏）
3. 查看HLS IP状态：
   - 如果显示"IP needs upgrade"：需要升级
   - 如果显示"IP is locked"：需要刷新

### Step 4: 升级/刷新HLS IP
有两种方法：

#### 方法A：使用Upgrade IP（推荐，保留连接）
1. 在IP Status报告中，找到 `hls_litho_system_bram_0`
2. 点击 **Upgrade IP** 按钮
3. Vivado会自动更新IP，保留所有连接
4. 点击 **Generate Output Products**

#### 方法B：删除重新添加（如果Upgrade失败）
1. 在Block Design中选中 `hls_litho_system_bram_0`
2. 按 **Delete** 键删除IP
3. 右键空白区域 → **Add IP** → 搜索 `hls_litho_system_bram`
4. 双击添加新的IP实例
5. **手动重新连接所有端口**（这是最繁琐的步骤）

### Step 5: 重新生成Bitstream
1. 在Flow Navigator中点击 **Generate Bitstream**
2. 或者使用Tcl命令：
   ```tcl
   reset_run impl_1
   launch_runs impl_1 -to_step write_bitstream -jobs 4
   wait_on_run impl_1
   ```

## 快速验证方法
更新完成后，连接硬件测试NX参数：
```tcl
# 在Vivado Hardware Manager中
mwr 0x50 300   # 写入NX=300
mrd 0x50       # 读取NX
```
- 如果读回300：✅ IP已更新
- 如果读回3：❌ IP未更新

## 当前状态
- HLS IP导出时间：2026-04-04 14:04
- Vivado ipshared更新：2026-04-04 14:05
- Bitstream生成：2026-04-04 14:57
- **状态：可能未完全更新，需要手动确认**

## 注意事项
- 如果Block Design连接复杂，强烈推荐使用方法A（Upgrade IP）
- 方法B会破坏所有连接，需要手动重新连线，耗时较长