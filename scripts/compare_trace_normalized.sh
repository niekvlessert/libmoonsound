#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "Usage: $0 <trace_a.csv> <trace_b.csv> [max_tick]"
  exit 1
fi

TRACE_A="$1"
TRACE_B="$2"
MAX_TICK="${3:-0}"

if [[ ! -f "$TRACE_A" ]]; then
  echo "Missing trace file: $TRACE_A"
  exit 1
fi
if [[ ! -f "$TRACE_B" ]]; then
  echo "Missing trace file: $TRACE_B"
  exit 1
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

normalize_trace() {
  local in_csv="$1"
  local out_norm="$2"
  local max_tick="$3"

  awk -F',' -v max_tick="$max_tick" '
    function norm_hex(v) {
      gsub(/^0x/, "", v);
      gsub(/^0X/, "", v);
      return toupper(v);
    }
    $1 == "REG" {
      tick = $2 + 0;
      if (max_tick > 0 && tick > max_tick) next;

      reg = norm_hex($4);
      val = norm_hex($5);

      mkey = tick SUBSEP reg SUBSEP val;
      mcount[mkey]++;

      rkey = tick SUBSEP reg;
      last[rkey] = val;
    }
    END {
      for (k in mcount) {
        split(k, p, SUBSEP);
        printf("M,%d,%s,%s,%d\n", p[1], p[2], p[3], mcount[k]);
      }
      for (k in last) {
        split(k, p, SUBSEP);
        printf("L,%d,%s,%s\n", p[1], p[2], last[k]);
      }
    }
  ' "$in_csv" | sort -t',' -k1,1 -k2,2n -k3,3 -k4,4 -k5,5n > "$out_norm"
}

find_first_diff() {
  local norm_a="$1"
  local norm_b="$2"
  local mode="$3"

  awk -F',' -v mode="$mode" '
    NR == FNR {
      if ($1 != mode) next;
      key = $2 SUBSEP $3;
      if (mode == "M") key = key SUBSEP $4;
      a[key] = $0;
      all[key] = 1;
      next;
    }
    $1 == mode {
      key = $2 SUBSEP $3;
      if (mode == "M") key = key SUBSEP $4;
      b[key] = $0;
      all[key] = 1;
    }
    END {
      best_tick = -1;
      best_key = "";
      for (k in all) {
        av = ((k in a) ? a[k] : "");
        bv = ((k in b) ? b[k] : "");
        if (av == bv) continue;

        split(k, p, SUBSEP);
        tick = p[1] + 0;
        if (best_tick < 0 || tick < best_tick) {
          best_tick = tick;
          best_key = k;
        }
      }

      if (best_tick < 0) {
        printf("FIRST_%s_DIFF=NONE\n", mode);
        exit 0;
      }

      av = ((best_key in a) ? a[best_key] : "(missing)");
      bv = ((best_key in b) ? b[best_key] : "(missing)");
      printf("FIRST_%s_DIFF_TICK=%d\n", mode, best_tick);
      printf("FIRST_%s_DIFF_A=%s\n", mode, av);
      printf("FIRST_%s_DIFF_B=%s\n", mode, bv);
    }
  ' "$norm_a" "$norm_b"
}

NORM_A="$TMPDIR/a.norm.csv"
NORM_B="$TMPDIR/b.norm.csv"

normalize_trace "$TRACE_A" "$NORM_A" "$MAX_TICK"
normalize_trace "$TRACE_B" "$NORM_B" "$MAX_TICK"

echo "A_TRACE=$TRACE_A"
echo "B_TRACE=$TRACE_B"
echo "MAX_TICK=$MAX_TICK"

find_first_diff "$NORM_A" "$NORM_B" "L"
find_first_diff "$NORM_A" "$NORM_B" "M"

echo "DIFF_PREVIEW_L:"
diff -u <(grep '^L,' "$NORM_A") <(grep '^L,' "$NORM_B") | sed -n '1,60p' || true

