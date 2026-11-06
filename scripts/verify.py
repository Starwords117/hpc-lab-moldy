#!/usr/bin/env python3
"""verify.py -- 比较 Moldy 运行输出与基准输出是否统计一致。

用法:
    python3 verify.py <你的输出> <基准输出> [--rel 2e-2] [--abs 2.0] [--strict]

规则:
  * 过滤含日期页眉和计时统计的行（这些天然不可复现）;
  * 纯数字行（能量轨迹 / RDF 表）逐 token 容差比较:
        |a-b| <= max(abs, rel * max(|a|, |b|))
    MD 是混沌系统, 浮点运算顺序一变轨迹即发散, 但统计量必须一致——
    这正是并行化练习要理解的核心现象;
  * 其余行（控制回显、Timestep 头等）必须逐字节一致;
  * --strict: 数字行也要求逐位一致（用于核对未改动的串行重建）。
"""
import argparse
import re
import sys

DATE_RE = re.compile(r"\w{3} \w{3} [ \d]\d \d\d:\d\d:\d\d \d{4}")


def load(path):
    lines = []
    with open(path, errors="replace") as f:
        for ln in f:
            s = ln.rstrip("\n")
            if DATE_RE.search(s) or "Run used" in s:
                continue
            lines.append(s)
    return lines


def isnum(t):
    try:
        float(t)
        return True
    except ValueError:
        return False


def main():
    p = argparse.ArgumentParser()
    p.add_argument("student", help="你的运行输出")
    p.add_argument("reference", help="基准输出 (baseline/ 下)")
    p.add_argument("--rel", type=float, default=2e-2, help="相对容差")
    p.add_argument("--abs", type=float, default=2.0, help="绝对容差")
    p.add_argument("--strict", action="store_true", help="要求数值逐位一致")
    a = p.parse_args()

    A, B = load(a.student), load(a.reference)
    if len(A) != len(B):
        print("FAIL: 行数不同 student=%d reference=%d —— 问题规模或输出设置被改动?" %
              (len(A), len(B)))
        sys.exit(1)

    maxdev, maxctx = 0.0, ""
    for i, (x, y) in enumerate(zip(A, B), 1):
        tx, ty = x.split(), y.split()
        if tx == ty:
            continue
        if len(tx) == len(ty) and tx and all(map(isnum, tx)) and all(map(isnum, ty)):
            for us, vs in zip(tx, ty):
                u, v = float(us), float(vs)
                dev = abs(u - v)
                if a.strict and dev > 0:
                    print("FAIL(strict): 第 %d 行数值逐位不一致: %s vs %s" % (i, us, vs))
                    sys.exit(1)
                lim = max(a.abs, a.rel * max(abs(u), abs(v)))
                if dev > lim:
                    print("FAIL: 第 %d 行数值超出容差: %s vs %s (dev=%.4g, lim=%.3g)\n  行: %s" %
                          (i, us, vs, dev, lim, x.strip()[:80]))
                    sys.exit(1)
                if dev > maxdev:
                    maxdev, maxctx = dev, x.strip()[:60]
        else:
            print("FAIL: 第 %d 行不一致:\n  student  : %s\n  reference: %s" % (i, x, y))
            sys.exit(1)

    ctx = (" @ " + maxctx) if maxctx else ""
    print("PASS: 与基准统计一致 ✔ (最大偏差 %.4g%s)" % (maxdev, ctx))


if __name__ == "__main__":
    main()
