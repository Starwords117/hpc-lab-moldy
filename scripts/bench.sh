#!/usr/bin/env bash
# bench.sh -- 计时脚本: 同一命令跑 3 次, 报告墙钟时间与内存, 取中位数。
#
# 用法:  scripts/bench.sh "<命令>"
# 例:    cd cases/baseline && ../../scripts/bench.sh "../../src/moldy control.baseline"
#
# 每次运行的 stdout 存到 bench_run_N.out (第 3 次结果可直接喂给 verify.py)。
# 系统没有 GNU /usr/bin/time 时自动回退为 date 计时(无 RSS)。

set -u
[ $# -ge 1 ] || { echo "用法: $0 <命令>"; exit 1; }
command="$*"

HAVE_GNU_TIME=0
[ -x /usr/bin/time ] && /usr/bin/time --version > /dev/null 2>&1 && HAVE_GNU_TIME=1

times=()
for i in 1 2 3; do
    if [ "$HAVE_GNU_TIME" = 1 ]; then
        /usr/bin/time -f "%e %M" -o bench_time.$$ $command > bench_run_$i.out 2>bench_run_$i.err
        read -r sec rss < bench_time.$$
        echo "run $i: ${sec} s  (max RSS ${rss} KB)"
    else
        t0=$(date +%s.%N)
        $command > bench_run_$i.out 2>bench_run_$i.err
        t1=$(date +%s.%N)
        sec=$(echo "$t1 $t0" | awk '{printf "%.2f", $1-$2}')
        rss="n/a"
        echo "run $i: ${sec} s  (max RSS ${rss}; 未安装 GNU time)"
    fi
    times+=("$sec")
done
[ -f bench_time.$$ ] && rm -f bench_time.$$
printf "%s\n" "${times[@]}" | sort -n | sed -n 2p | awk '{printf "中位数: %.2f s\n", $1}'
