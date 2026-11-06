# HPC 多核优化练习 · Moldy

> 进阶题：给一个 1990 年代的经典分子动力学程序做多核优化。

## 背景

Moldy 是英国 CCP5 联合体 1990 年代起发布的经典分子动力学代码（Keith Refson 著，C 语言，GPL），用于模拟周期边界下的固体与液体——本仓库算例是三相点的液态氩。它诞生在单核时代，至今只在 1 个核心上运行。

## 你的任务

1. 编译并跑出基线（见"快速开始"）；
2. 用 `perf` / `gprof` 找出热点，把剖析证据写进报告；
3. 做多核优化（OpenMP / std::thread），**物理结果必须保持统计一致**（见判据）；
4. 测量 1 / 2 / 4 / 8 线程加速比，提交代码 diff + 优化报告。

## 环境

- Linux + gcc（8 核以上 CPU；16G 内存绰绰有余，本题峰值内存 < 50 MB）
- 无任何第三方库依赖（XDR 兼容层已内置）
- **Windows 用户**：请安装 WSL2（管理员 PowerShell 运行 `wsl --install -d Ubuntu`），在 Ubuntu 内按本文档操作——所有命令原样适用。原生 MinGW/MSYS2 未做验证，且缺少 perf，不建议

## 快速开始

```bash
git clone <本仓库 URL> && cd hpc-lab-moldy/src
./configure CFLAGS='-O2 -std=gnu89 -fcommon' && make -j
```

三个算例（都在 `cases/` 下，冒烟 → 自测 → 全量）：

```bash
cd ../cases/smoke     && ../../src/moldy control.smoke   > run.out 2>run.err   # 参考机器 ~17 s
cd ../cases/selftest  && ../../src/moldy control.selftest > run.out 2>run.err  # 参考机器 ~60 s
cd ../cases/baseline  && ../../src/moldy control.baseline > run.out 2>run.err  # 参考机器 ≈5 min（普通笔记本 ≈8 min）
```

## 正确性判据

MD 是混沌系统：浮点求和顺序一变，微观轨迹就会发散——但统计性质必须一致（这正是并行化要理解的核心现象）。用自带脚本校验：

```bash
cd cases/baseline && python3 ../../scripts/verify.py run.out ../../baseline/baseline_2048x80k.out
```

脚本自动跳过时间戳页眉，按输出分区做容差比较：**滚动平均块**（主判据）与运行结束的 **Average values 总结块**（按各量自带的标准差定标，并带 1% 相对下限）、**标准差块**、**RDF 表**均有各自的容差带；瞬时能量/压力块只做防崩坏粗检——混沌轨迹对浮点顺序天然敏感，这正是并行化要理解的核心现象。控制回显与 Timestep 头必须逐字节一致；`--strict` 用于核对同机串行重建的逐位一致。具体容差值见脚本头部注释。

## 计时与提交

```bash
cd cases/baseline && ../../scripts/bench.sh "../../src/moldy control.baseline"
```

- 脚本跑 3 次取中位数，各次输出在 `bench_run_N.out`（第 3 次可直接喂给 `verify.py`）
- 提交：代码 diff（或 git 分支）+ 优化报告（剖析证据、方案、加速比表、讨论）
- 报告请附 `lscpu` 输出与编译器版本

## 规则

- 禁止修改 `cases/` 下任何控制文件或问题规模（`nsteps`、原子数、`seed` 等）
- 禁止一切改变物理结果的做法（跳过计算、缩短模拟、预计算结果）
- 允许：编译器 flag、OpenMP、std::thread 等一切保持判据通过的优化

## 数据与许可

- 代码：Moldy，GPL-2.0+（`LICENSE`）；SG 组件另见 `LICENSE-SGINFO`
- 程序手册（控制文件全部参数说明）：`src/moldy.tex`（LaTeX 源）；官方说明见 `docs/README-upstream.md` 与 `src/BENCHMARK`
- 算例基于上游 `src/examples/control.argon` 调整规模而来
