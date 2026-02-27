#!/usr/bin/env bash
set -euo pipefail

# ============================================================
#  五语言开销追踪器 性能对比 Benchmark (优化版 · 顺序执行)
#  Languages: C++, Zig, Rust, Go, TypeScript
#  Compatible with Bash 3.2+ (macOS default)
# ============================================================

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"

# ---------- PATH setup ----------
export PATH="$HOME/.local/go/current/bin:/usr/local/bin:/usr/local/go/bin:$HOME/go/bin:$HOME/.cargo/bin:/opt/homebrew/bin:$PATH"

# ---------- Colours ----------
BOLD='\033[1m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# ---------- Helper: millisecond timestamp ----------
now_ms() {
  python3 -c "import time; print(int(time.time()*1000))"
}

# ---------- Helper: median of a file of numbers ----------
median() {
  sort -n "$1" | awk '{a[NR]=$1} END{if(NR%2==1) print a[(NR+1)/2]; else print int((a[NR/2]+a[NR/2+1])/2)}'
}

# ---------- Helper: file size in bytes ----------
file_size_bytes() {
  stat -f%z "$1" 2>/dev/null || stat -c%s "$1" 2>/dev/null
}

# ---------- Temp dir ----------
BENCH_TMP=$(mktemp -d /tmp/bench_run_XXXXXX)
trap 'rm -rf "$BENCH_TMP"' EXIT

# ---------- Language labels ----------
ALL_LABELS_cpp="C++"
ALL_LABELS_zig="Zig"
ALL_LABELS_rust="Rust"
ALL_LABELS_go="Go"
ALL_LABELS_ts="TypeScript"
get_label() { eval echo "\$ALL_LABELS_$1"; }

# ---------- Detect available languages ----------
LANGS=""
detect() {
  local id="$1" check="$2"
  if eval "$check" >/dev/null 2>&1; then
    LANGS="$LANGS $id"
  else
    printf "${YELLOW}[SKIP]${NC} %s — toolchain not found\n" "$(get_label "$id")" >&2
  fi
}

detect cpp   "command -v g++"
detect zig   "command -v zig"
detect rust  "command -v cargo"
detect go    "command -v go"
detect ts    "command -v node && command -v npx"

LANGS=$(echo $LANGS)

if [ -z "$LANGS" ]; then
  echo "No language toolchains found. Exiting." >&2
  exit 1
fi

printf "${CYAN}Benchmarking: %s${NC}\n" "$LANGS" >&2
printf "${CYAN}Mode: sequential (one language at a time)${NC}\n\n" "$LANGS" >&2

# ============================================================
#  Generate test input files
# ============================================================
printf "${CYAN}Generating test inputs...${NC}\n" >&2

COLD_INPUT="$BENCH_TMP/input_cold.txt"
printf '6\n' > "$COLD_INPUT"

BULK_INPUT="$BENCH_TMP/input_bulk.txt"
{
  for i in $(seq 1 500); do
    printf '1\n\n\n\n描述%d\n%d.%02d\n测试类别\n' "$i" "$((i % 1000))" "$((i % 100))"
  done
  printf '2\n'
  printf '3\n2026\n2\n'
  printf '4\n1\n2026\n0\n'
  printf '6\n'
} > "$BULK_INPUT"

# ============================================================
#  Per-language helpers
# ============================================================

build_cpp() {
  local dir="$ROOT_DIR/cpp_expense_tracker"
  rm -f "$dir/expense_tracker_cpp"
  local t0 t1
  t0=$(now_ms)
  g++ -O2 -march=native -mtune=native -flto "$dir/main.cpp" -o "$dir/expense_tracker_cpp" 2>/dev/null
  t1=$(now_ms)
  echo $((t1 - t0))
}
binary_cpp() { echo "$ROOT_DIR/cpp_expense_tracker/expense_tracker_cpp"; }
run_cpp()    { "$ROOT_DIR/cpp_expense_tracker/expense_tracker_cpp"; }
strip_cpp()  { local b; b=$(binary_cpp); strip -o "${b}.stripped" "$b" 2>/dev/null && echo "${b}.stripped" || echo "$b"; }

build_zig() {
  local dir="$ROOT_DIR/zig_expense_tracker"
  rm -rf "$dir/zig-out" "$dir/.zig-cache"
  local t0 t1
  t0=$(now_ms)
  (cd "$dir" && zig build -Doptimize=ReleaseFast 2>/dev/null)
  t1=$(now_ms)
  echo $((t1 - t0))
}
binary_zig() { echo "$ROOT_DIR/zig_expense_tracker/zig-out/bin/expense_tracker"; }
run_zig()    { "$ROOT_DIR/zig_expense_tracker/zig-out/bin/expense_tracker"; }
strip_zig()  { local b; b=$(binary_zig); strip -o "${b}.stripped" "$b" 2>/dev/null && echo "${b}.stripped" || echo "$b"; }

build_rust() {
  local dir="$ROOT_DIR/rust_expense_tracker"
  (cd "$dir" && cargo clean 2>/dev/null)
  local t0 t1
  t0=$(now_ms)
  (cd "$dir" && cargo build --release 2>/dev/null)
  t1=$(now_ms)
  echo $((t1 - t0))
}
binary_rust() { echo "$ROOT_DIR/rust_expense_tracker/target/release/rust_expense_tracker"; }
run_rust()    { "$ROOT_DIR/rust_expense_tracker/target/release/rust_expense_tracker"; }
strip_rust()  { local b; b=$(binary_rust); strip -o "${b}.stripped" "$b" 2>/dev/null && echo "${b}.stripped" || echo "$b"; }

build_go() {
  local dir="$ROOT_DIR/go_expense_tracker"
  rm -f "$dir/expense_tracker_go"
  local t0 t1
  t0=$(now_ms)
  (cd "$dir" && go build -ldflags="-s -w" -o expense_tracker_go 2>/dev/null)
  t1=$(now_ms)
  echo $((t1 - t0))
}
binary_go() { echo "$ROOT_DIR/go_expense_tracker/expense_tracker_go"; }
run_go()    { "$ROOT_DIR/go_expense_tracker/expense_tracker_go"; }
strip_go()  { local b; b=$(binary_go); strip -o "${b}.stripped" "$b" 2>/dev/null && echo "${b}.stripped" || echo "$b"; }

build_ts() {
  local dir="$ROOT_DIR/ts_expense_tracker"
  rm -rf "$dir/dist"
  local t0 t1
  t0=$(now_ms)
  (cd "$dir" && npx tsc 2>/dev/null)
  t1=$(now_ms)
  echo $((t1 - t0))
}
binary_ts()  { echo "$ROOT_DIR/ts_expense_tracker/dist/main.js"; }
run_ts()     { node "$ROOT_DIR/ts_expense_tracker/dist/main.js"; }
strip_ts()   { echo ""; }

# ============================================================
#  Run all 5 dimensions for ONE language, then move to next
# ============================================================

bench_one_lang() {
  local lang="$1"
  local label
  label=$(get_label "$lang")

  printf "\n${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n" >&2
  printf "${BOLD}${CYAN}  Benchmarking: %s${NC}\n" "$label" >&2
  printf "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n" >&2

  # ---- [1] Compile Time (5 runs) ----
  printf "  ${GREEN}[1/5] Compile time${NC}\n" >&2
  local results_file="$BENCH_TMP/compile_${lang}.txt"
  : > "$results_file"

  "build_${lang}" > /dev/null  # warmup

  for run in $(seq 1 5); do
    ms=$("build_${lang}")
    echo "$ms" >> "$results_file"
    printf "        run %d: %dms\n" "$run" "$ms" >&2
  done

  local val
  val=$(median "$results_file")
  eval "R_COMPILE_${lang}=$val"
  printf "        → median: ${BOLD}%dms${NC}\n" "$val" >&2

  # ---- [2] Binary Size ----
  printf "  ${GREEN}[2/5] Binary size${NC}\n" >&2
  "build_${lang}" > /dev/null

  local bin_path size_bytes size_kb
  bin_path=$("binary_${lang}")
  size_bytes=$(file_size_bytes "$bin_path")
  size_kb=$(( (size_bytes + 512) / 1024 ))

  if [ "$lang" != "ts" ]; then
    local stripped stripped_bytes stripped_kb
    stripped=$("strip_${lang}")
    if [ -n "$stripped" ] && [ -f "$stripped" ]; then
      stripped_bytes=$(file_size_bytes "$stripped")
      stripped_kb=$(( (stripped_bytes + 512) / 1024 ))
      printf "        %dKB (stripped: %dKB)\n" "$size_kb" "$stripped_kb" >&2
      eval "R_SIZE_${lang}='${size_kb} (${stripped_kb})'"
      rm -f "$stripped"
    else
      printf "        %dKB\n" "$size_kb" >&2
      eval "R_SIZE_${lang}='${size_kb}'"
    fi
  else
    printf "        %dKB (JS bundle)\n" "$size_kb" >&2
    eval "R_SIZE_${lang}='${size_kb}'"
  fi

  # ---- [3] Startup Time (10 runs) ----
  printf "  ${GREEN}[3/5] Startup time${NC}\n" >&2
  results_file="$BENCH_TMP/startup_${lang}.txt"
  : > "$results_file"

  local work_dir
  work_dir=$(mktemp -d "$BENCH_TMP/su_w_XXXXXX")
  (cd "$work_dir" && "run_${lang}" < "$COLD_INPUT" > /dev/null 2>&1) || true
  rm -rf "$work_dir"

  for run in $(seq 1 10); do
    work_dir=$(mktemp -d "$BENCH_TMP/su_XXXXXX")
    local t0 t1 ms
    t0=$(now_ms)
    (cd "$work_dir" && "run_${lang}" < "$COLD_INPUT" > /dev/null 2>&1) || true
    t1=$(now_ms)
    ms=$((t1 - t0))
    echo "$ms" >> "$results_file"
    printf "        run %d: %dms\n" "$run" "$ms" >&2
    rm -rf "$work_dir"
  done

  val=$(median "$results_file")
  eval "R_STARTUP_${lang}=$val"
  printf "        → median: ${BOLD}%dms${NC}\n" "$val" >&2

  # ---- [4] Bulk Performance (10 runs) ----
  printf "  ${GREEN}[4/5] Bulk performance (500 records)${NC}\n" >&2
  results_file="$BENCH_TMP/bulk_${lang}.txt"
  : > "$results_file"

  work_dir=$(mktemp -d "$BENCH_TMP/bk_w_XXXXXX")
  (cd "$work_dir" && "run_${lang}" < "$BULK_INPUT" > /dev/null 2>&1) || true
  rm -rf "$work_dir"

  for run in $(seq 1 10); do
    work_dir=$(mktemp -d "$BENCH_TMP/bk_XXXXXX")
    t0=$(now_ms)
    (cd "$work_dir" && "run_${lang}" < "$BULK_INPUT" > /dev/null 2>&1) || true
    t1=$(now_ms)
    ms=$((t1 - t0))
    echo "$ms" >> "$results_file"
    printf "        run %d: %dms\n" "$run" "$ms" >&2
    rm -rf "$work_dir"
  done

  val=$(median "$results_file")
  eval "R_BULK_${lang}=$val"
  printf "        → median: ${BOLD}%dms${NC}\n" "$val" >&2

  # ---- [5] Peak Memory (5 runs) ----
  printf "  ${GREEN}[5/5] Peak memory${NC}\n" >&2
  results_file="$BENCH_TMP/mem_${lang}.txt"
  : > "$results_file"

  work_dir=$(mktemp -d "$BENCH_TMP/mm_w_XXXXXX")
  (cd "$work_dir" && "run_${lang}" < "$BULK_INPUT" > /dev/null 2>&1) || true
  rm -rf "$work_dir"

  for run in $(seq 1 5); do
    work_dir=$(mktemp -d "$BENCH_TMP/mm_XXXXXX")

    local mem_output mem_bytes mem_kb
    if [ "$lang" = "ts" ]; then
      mem_output=$( (cd "$work_dir" && /usr/bin/time -l node "$ROOT_DIR/ts_expense_tracker/dist/main.js" < "$BULK_INPUT" > /dev/null) 2>&1 )
    else
      bin_path=$("binary_${lang}")
      mem_output=$( (cd "$work_dir" && /usr/bin/time -l "$bin_path" < "$BULK_INPUT" > /dev/null) 2>&1 )
    fi

    mem_bytes=$(echo "$mem_output" | grep "maximum resident set size" | awk '{print $1}')
    mem_kb=$((mem_bytes / 1024))
    echo "$mem_kb" >> "$results_file"
    printf "        run %d: %dKB\n" "$run" "$mem_kb" >&2
    rm -rf "$work_dir"
  done

  val=$(median "$results_file")
  eval "R_MEMORY_${lang}=$val"
  printf "        → median: ${BOLD}%dKB${NC}\n" "$val" >&2

  printf "  ${GREEN}Done: %s${NC}\n" "$label" >&2
}

# ============================================================
#  Main: run each language sequentially
# ============================================================

for lang in $LANGS; do
  bench_one_lang "$lang"
done

# ============================================================
#  Output: Markdown Table
# ============================================================
printf "\n${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n" >&2
printf "${BOLD}${CYAN}  Final Results${NC}\n" >&2
printf "${BOLD}${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}\n\n" >&2

header="| 指标 |"
sep="|------|"
for lang in $LANGS; do
  header="$header $(get_label $lang) |"
  sep="$sep--------|"
done
echo "$header"
echo "$sep"

print_row() {
  local metric="$1" prefix="$2"
  local row="| $metric |"
  for lang in $LANGS; do
    val=$(eval echo "\$${prefix}_${lang}")
    row="$row $val |"
  done
  echo "$row"
}

print_row "编译时间 (ms)" "R_COMPILE"
print_row "产物大小 (KB)" "R_SIZE"
print_row "启动时间 (ms)" "R_STARTUP"
print_row "批量耗时 (ms)" "R_BULK"
print_row "峰值内存 (KB)" "R_MEMORY"

printf "\n${GREEN}Benchmark complete!${NC}\n" >&2
