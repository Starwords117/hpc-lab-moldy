#!/usr/bin/env python3
"""verify.py -- 比较 Moldy 运行输出与基准输出是否物理一致。

用法:
    python3 verify.py <你的输出> <基准输出> [--strict]

原理:
  MD 是混沌系统, 浮点运算顺序一变微观轨迹即发散, 瞬时量(每个打印点的
  能量/压力)只在各自涨落范围内有意义; 真正可复现的判据是输出中的统计块:
    * Rolling averages (滚动平均)      -- 主要判据
    * Standard deviations (标准差)     -- 涨落幅度一致
    * RDF 表 (径向分布函数)            -- 结构判据
    * 结尾 Average values 总结块       -- 全程平均, 同滚动平均容差
  瞬时块只做粗检(防止物理彻底崩坏), 不要求逐值一致。

容差默认值(按输出分区):
    rolling avg : |a-b| <= max(45, 1e-2 * max|a|,|b|)
    总结块("X = v +/- s") : |a-b| <= max(10, 2.12*sqrt(s1^2+s2^2), 1e-2*|v|)
                  -- 均值差 3σ 口径 + 1% 相对下限(逐粒子量的报告 σ 因窗口相关而低估)
    stddev      : |a-b| <= max(10, 5e-2 * max|a|,|b|)
    instantaneous: |a-b| <= max(200, 5e-2 * max|a|,|b|)
    RDF rows    : |a-b| <= max(0.15, 5e-2 * max|a|,|b|)
  其余文本行(控制回显、Timestep 头)必须逐字节一致。
  --strict: 所有数值要求逐位一致(仅用于核对同机串行重建)。
"""
import argparse
import re
import sys

DATE_RE = re.compile(r"\w{3} \w{3} [ \d]\d \d\d:\d\d:\d\d \d{4}")
# 结尾 Average values 总结块行, 形如 "Trans KE = 892.24 +/- 16.384  kJ/mol"
AVG_SUM_RE = re.compile(r"=\s*-?\d[\d.eE+-]*\s*\+/-\s*-?\d[\d.eE+-]*")

# 分区标记(逐字节比较)与该区数值行的容差
MARKERS = (
    ("======== Timestep", "inst", (200.0, 5e-2)),
    ("-------- Rolling averages", "avg", (45.0, 1e-2)),
    ("-------- Standard deviations", "std", (10.0, 5e-2)),
)

RDF_ABS, RDF_REL = 0.15, 5e-2


def load(path):
    """返回 (行列表, 每行的容差类)。类: inst/avg/std/rdf/text"""
    lines, kinds = [], []
    section = "text"
    with open(path, errors="replace") as f:
        for ln in f:
            s = ln.rstrip("\n")
            if DATE_RE.search(s) or "Run used" in s:
                continue                      # 页眉/计时, 天然不可复现
            st = s.strip()
            hit = False
            for prefix, kind, tol in MARKERS:
                if st.startswith(prefix):
                    section = kind
                    hit = True
                    break
            if hit or (st.endswith("RDF") and "cutoff" not in st):
                if st.endswith("RDF") and not hit:
                    section = "rdf"
                lines.append(s)
                kinds.append("text")
                continue
            toks = s.split()
            if AVG_SUM_RE.search(s):
                lines.append(s)
                kinds.append("avgsum")
            elif toks and all(_isnum(t) for t in toks):
                lines.append(s)
                kinds.append(section)
            else:
                lines.append(s)
                kinds.append("text")
    return lines, kinds


def _isnum(t):
    try:
        float(t.rstrip(",;:"))      # 数字可带尾标点, 如 "16.535,"
        return True
    except ValueError:
        return False


def _num(t):
    return float(t.rstrip(",;:"))


def _sigma_of(line):
    """提取总结行 '+/-' 之后的 stddev, 无则 0。"""
    toks = line.split()
    for j, t in enumerate(toks):
        if t == "+/-" and j + 1 < len(toks) and _isnum(toks[j + 1]):
            return _num(toks[j + 1])
    return 0.0


def main():
    p = argparse.ArgumentParser()
    p.add_argument("student", help="你的运行输出")
    p.add_argument("reference", help="基准输出 (baseline/ 下)")
    p.add_argument("--strict", action="store_true", help="所有数值要求逐位一致")
    a = p.parse_args()

    A, KA = load(a.student)
    B, KB = load(a.reference)
    if len(A) != len(B):
        print("FAIL: 行数不同 student=%d reference=%d —— 问题规模或输出设置被改动?" %
              (len(A), len(B)))
        sys.exit(1)

    maxdev, maxctx = 0.0, ""
    for i, (x, y, k, k2) in enumerate(zip(A, B, KA, KB), 1):
        tx, ty = x.split(), y.split()
        if tx == ty:
            continue
        if k != k2:
            print("FAIL: 第 %d 行分区不一致:\n  student  : %s\n  reference: %s" % (i, x, y))
            sys.exit(1)
        if k == "text" or len(tx) != len(ty):
            print("FAIL: 第 %d 行不一致(%s):\n  student  : %s\n  reference: %s" %
                  (i, k, x, y))
            sys.exit(1)
        if k == "avgsum":
            s1, s2 = _sigma_of(x), _sigma_of(y)
            tol_abs = max(10.0, 2.12 * (s1 * s1 + s2 * s2) ** 0.5)
            tol_rel = 1e-2
        else:
            tol_abs, tol_rel = (RDF_ABS, RDF_REL) if k == "rdf" else MARKERS[
                [m[1] for m in MARKERS].index(k)][2]
        for us, vs in zip(tx, ty):
            if not (_isnum(us) and _isnum(vs)):
                if us != vs:            # 混合行中的非数值 token 须逐字一致
                    print("FAIL: 第 %d 行不一致[%s]: %s vs %s" % (i, k, us, vs))
                    sys.exit(1)
                continue
            u, v = _num(us), _num(vs)
            dev = abs(u - v)
            if a.strict and dev > 0:
                print("FAIL(strict): 第 %d 行数值逐位不一致: %s vs %s" % (i, us, vs))
                sys.exit(1)
            lim = max(tol_abs, tol_rel * max(abs(u), abs(v)))
            if dev > lim:
                print("FAIL: 第 %d 行数值超出容差[%s]: %s vs %s (dev=%.4g, lim=%.3g)\n  行: %s" %
                      (i, k, us, vs, dev, lim, x.strip()[:80]))
                sys.exit(1)
            if dev > maxdev:
                maxdev, maxctx = dev, "[%s] %s" % (k, x.strip()[:56])

    print("PASS: 与基准物理一致 ✔ (最大偏差 %.4g %s)" % (maxdev, maxctx))


if __name__ == "__main__":
    main()
