#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
LANE="all"
RUNNER="host"
CACHE_MODE=""
BASE_REF=""
CONTAINER_IMAGE=""
CONTAINER_ENGINE="${GOGGLES_CI_CONTAINER_ENGINE:-auto}"
CI_START_MS=0
declare -a STAGE_LEVELS=()
declare -a STAGE_LABELS=()
declare -a STAGE_ELAPSED_MS=()
declare -a STAGE_STATUS=()

die() {
  echo "Error: $*" >&2
  exit 1
}

now_ms() {
  date +%s%3N
}

format_elapsed_ms() {
  local elapsed_ms="$1"
  local total_seconds=$((elapsed_ms / 1000))
  local milliseconds=$((elapsed_ms % 1000))
  local hours=$((total_seconds / 3600))
  local minutes=$(((total_seconds % 3600) / 60))
  local seconds=$((total_seconds % 60))

  printf '%02d:%02d:%02d.%03d' "$hours" "$minutes" "$seconds" "$milliseconds"
}

append_stage_record() {
  local level="$1"
  local label="$2"
  local elapsed_ms="$3"
  local status="$4"

  STAGE_LEVELS+=("$level")
  STAGE_LABELS+=("$label")
  STAGE_ELAPSED_MS+=("$elapsed_ms")
  STAGE_STATUS+=("$status")
}

run_timed_stage() {
  local level="$1"
  local label="$2"
  local start_ms
  local end_ms
  local elapsed_ms
  local status
  local index
  shift 2

  index="${#STAGE_LABELS[@]}"
  append_stage_record "$level" "$label" 0 0

  start_ms="$(now_ms)"

  set +e
  "$@"
  status=$?
  set -e

  end_ms="$(now_ms)"
  elapsed_ms=$((end_ms - start_ms))

  STAGE_ELAPSED_MS[$index]="$elapsed_ms"
  STAGE_STATUS[$index]="$status"

  return "$status"
}

run_clang_format_i() {
  printf '%s\0' "$@" | xargs -0 clang-format -i --
}

run_taplo_fmt() {
  printf '%s\0' "$@" | xargs -0 taplo fmt
}

write_stage_metrics() {
  local file_path="$1"
  local index

  : >"$file_path"

  for index in "${!STAGE_LABELS[@]}"; do
    printf 'stage\t%s\t%s\t%s\t%s\n' \
      "${STAGE_LEVELS[$index]}" \
      "${STAGE_STATUS[$index]}" \
      "${STAGE_ELAPSED_MS[$index]}" \
      "${STAGE_LABELS[$index]}" >>"$file_path"
  done
}

maybe_write_stage_metrics() {
  if [[ -n "${GOGGLES_CI_STAGE_METRICS_FILE:-}" ]]; then
    write_stage_metrics "$GOGGLES_CI_STAGE_METRICS_FILE"
  fi
}

import_stage_metrics() {
  local file_path="$1"
  local level_offset="$2"
  local record_type
  local level
  local status
  local elapsed_ms
  local label

  [[ -f "$file_path" ]] || return 0

  while IFS=$'\t' read -r record_type level status elapsed_ms label; do
    [[ "$record_type" == "stage" ]] || continue
    append_stage_record "$((level + level_offset))" "$label" "$elapsed_ms" "$status"
  done <"$file_path"
}

print_stage_summary() {
  local index
  local indent
  local level
  local suffix

  ((${#STAGE_LABELS[@]})) || return 0

  printf '\nTiming summary\n'

  for index in "${!STAGE_LABELS[@]}"; do
    indent=""
    level="${STAGE_LEVELS[$index]}"

    while ((level > 0)); do
      indent+="  "
      level=$((level - 1))
    done

    suffix=""
    if [[ "${STAGE_STATUS[$index]}" -ne 0 ]]; then
      suffix=" (failed)"
    fi

    printf '%s- %s: %s%s\n' \
      "$indent" \
      "${STAGE_LABELS[$index]}" \
      "$(format_elapsed_ms "${STAGE_ELAPSED_MS[$index]}")" \
      "$suffix"
  done
}

ensure_repo_tmpdir() {
  local root="$REPO_ROOT/.cache/tmp"

  mkdir -p "$root"
  printf '%s\n' "$root"
}

usage() {
  cat <<'EOF'
Usage:
  pixi run ci [--lane LANE] [--runner RUNNER] [--cache-mode MODE] [--base-ref REF]

Run the main CI lanes locally.

Lanes:
  all              Run every CI lane (default)
  format           Run the format-check lane
  build-test       Run the build-and-test lane
  static-analysis  Run both static-analysis lanes (Semgrep + quality)
  static-analysis-semgrep  Run only the Semgrep static-analysis lane
  static-analysis-quality  Run only the quality-build static-analysis lane
  static-analysis-quality-pr  Run clang-tidy only on changed src files relative to --base-ref

Runners:
  host             Run lanes on the local host (default)
  container        Run lanes inside the pinned CI container

Cache modes (container runner only):
  warm             Reuse container caches across runs (default)
  cold             Use fresh container caches for this run
EOF
}

resolve_container_engine() {
  if [[ "$CONTAINER_ENGINE" != "auto" ]]; then
    command -v "$CONTAINER_ENGINE" >/dev/null 2>&1 || die "Container engine '$CONTAINER_ENGINE' not found"
    printf '%s\n' "$CONTAINER_ENGINE"
    return 0
  fi

  if command -v docker >/dev/null 2>&1; then
    printf '%s\n' docker
    return 0
  fi

  if command -v podman >/dev/null 2>&1; then
    printf '%s\n' podman
    return 0
  fi

  die "No supported container engine found. Install docker or podman, or set GOGGLES_CI_CONTAINER_ENGINE"
}

resolve_container_image() {
  local hash

  if [[ -n "${GOGGLES_CI_CONTAINER_IMAGE:-}" ]]; then
    printf '%s\n' "$GOGGLES_CI_CONTAINER_IMAGE"
    return 0
  fi

  hash="$(sha256sum "$REPO_ROOT/ci/local-runner/Dockerfile" | cut -c1-12)"
  printf 'goggles-ci-local:%s\n' "$hash"
}

ensure_container_image() {
  local engine="$1"

  if "$engine" image inspect "$CONTAINER_IMAGE" >/dev/null 2>&1; then
    return 0
  fi

  echo "==> Building CI container image '$CONTAINER_IMAGE'"
  "$engine" build -t "$CONTAINER_IMAGE" "$REPO_ROOT/ci/local-runner"
}

prepare_container_cache_root() {
  local lane="$1"
  local mode="$2"
  local root

  if [[ "$mode" == "warm" ]]; then
    root="$REPO_ROOT/.cache/ci-container/warm/$lane"
    mkdir -p "$root/shared/home" "$root/shared/pixi-home" "$root/shared/pixi-cache" "$root/shared/tmp" "$root/shared/xdg-runtime"
    mkdir -p "$root/lane/build" "$root/lane/project-pixi" "$root/lane/ccache"
    chmod 700 "$root/shared/xdg-runtime"
    printf '%s\n' "$root"
    return 0
  fi

  mkdir -p "$REPO_ROOT/.cache/ci-container"
  root="$(mktemp -d "$REPO_ROOT/.cache/ci-container/cold.${lane}.XXXXXX")"
  mkdir -p "$root/shared/home" "$root/shared/pixi-home" "$root/shared/pixi-cache" "$root/shared/tmp" "$root/shared/xdg-runtime"
  mkdir -p "$root/lane/build" "$root/lane/project-pixi" "$root/lane/ccache"
  chmod 700 "$root/shared/xdg-runtime"
  printf '%s\n' "$root"
}

container_lane_command() {
  local lane="$1"

  case "$lane" in
    format)
      printf '%s\n' "pixi run --locked -e lint ci-format --runner host"
      ;;
    build-test)
      printf '%s\n' "pixi run --locked ci --lane build-test --runner host"
      ;;
    static-analysis)
      printf '%s\n' "pixi run --locked ci --lane static-analysis --runner host"
      ;;
    static-analysis-semgrep)
      printf '%s\n' "pixi run --locked ci --lane static-analysis-semgrep --runner host"
      ;;
    static-analysis-quality)
      printf '%s\n' "pixi run --locked ci --lane static-analysis-quality --runner host"
      ;;
    static-analysis-quality-pr)
      [[ -n "$BASE_REF" ]] || die "Container lane 'static-analysis-quality-pr' requires --base-ref"
      printf 'pixi run --locked ci --lane static-analysis-quality-pr --runner host --base-ref %q\n' "$BASE_REF"
      ;;
    *)
      die "Unsupported container lane '$lane'"
      ;;
  esac
}

run_container_lane() {
  local lane="$1"
  local engine
  local cache_root
  local command
  local device
  local group_id
  local metrics_host_path
  local metrics_container_path
  local status
  local -a run_args

  engine="$(resolve_container_engine)"
  ensure_container_image "$engine"
  cache_root="$(prepare_container_cache_root "$lane" "$CACHE_MODE")"

  command="$(container_lane_command "$lane")"
  metrics_host_path="$cache_root/shared/tmp/ci-stage-metrics.${lane}.tsv"
  metrics_container_path="/tmp/goggles-tmp/ci-stage-metrics.${lane}.tsv"
  rm -f "$metrics_host_path"
  run_args=(run --rm)

  if [[ -t 0 && -t 1 ]]; then
    run_args+=( -it )
  fi

  run_args+=(
    --user "$(id -u):$(id -g)"
    -e CI=true
    -e GITHUB_ACTIONS=true
    -e HOME=/tmp/goggles-home
    -e PIXI_HOME=/tmp/goggles-pixi-home
    -e PIXI_CACHE_DIR=/tmp/goggles-pixi-cache
    -e CCACHE_DIR=/tmp/goggles-ccache
    -e TMPDIR=/tmp/goggles-tmp
    -e XDG_RUNTIME_DIR=/tmp/goggles-runtime
    -e GOGGLES_CI_IN_CONTAINER=1
    -e GOGGLES_CI_SUPPRESS_SUMMARY=1
    -e GOGGLES_CI_STAGE_METRICS_FILE=$metrics_container_path
    -v "$REPO_ROOT:/work"
    -v "$cache_root/shared/home:/tmp/goggles-home"
    -v "$cache_root/shared/pixi-home:/tmp/goggles-pixi-home"
    -v "$cache_root/shared/pixi-cache:/tmp/goggles-pixi-cache"
    -v "$cache_root/shared/tmp:/tmp/goggles-tmp"
    -v "$cache_root/shared/xdg-runtime:/tmp/goggles-runtime"
    -v "$cache_root/lane/build:/work/build"
    -v "$cache_root/lane/project-pixi:/work/.pixi"
    -v "$cache_root/lane/ccache:/tmp/goggles-ccache"
    -w /work
  )

  for group_id in $(id -G); do
    run_args+=( --group-add "$group_id" )
  done

  for device in /dev/dri/* /dev/kfd; do
    if [[ -c "$device" ]]; then
      run_args+=( --device "$device:$device" )
    fi
  done

  echo "==> Running CI $lane lane in container ($CACHE_MODE cache)"

  set +e
  "$engine" "${run_args[@]}" "$CONTAINER_IMAGE" bash -lc "$command"
  status=$?
  set -e

  import_stage_metrics "$metrics_host_path" 1

  if [[ "$CACHE_MODE" == "cold" ]]; then
    rm -rf "$cache_root"
  fi

  return "$status"
}

run_format_lane_impl() {
  local temp_dir
  local temp_root
  local file
  local formatter_changed=0
  local -a cpp_files=()
  local -a toml_files=()
  local -a temp_cpp_files=()
  local -a temp_toml_files=()

  temp_root="$(ensure_repo_tmpdir)"
  temp_dir="$(mktemp -d "$temp_root/ci-format.XXXXXX")"
  trap 'rm -rf "${temp_dir:-}"' RETURN

  cp "$REPO_ROOT/.clang-format" "$temp_dir/.clang-format"

  mapfile -d '' -t cpp_files < <(git ls-files -z '*.c' '*.h' '*.cpp' '*.hpp')
  mapfile -d '' -t toml_files < <(git ls-files -z '*.toml' | grep -zv 'test_data/' || true)

  for file in "${cpp_files[@]}"; do
    mkdir -p "$temp_dir/$(dirname "$file")"
    cp "$file" "$temp_dir/$file"
    temp_cpp_files+=("$temp_dir/$file")
  done

  for file in "${toml_files[@]}"; do
    mkdir -p "$temp_dir/$(dirname "$file")"
    cp "$file" "$temp_dir/$file"
    temp_toml_files+=("$temp_dir/$file")
  done

  if ((${#temp_cpp_files[@]})); then
    run_timed_stage 1 "clang-format -i" run_clang_format_i "${temp_cpp_files[@]}"
  fi

  if ((${#temp_toml_files[@]})); then
    run_timed_stage 1 "taplo fmt" run_taplo_fmt "${temp_toml_files[@]}"
  fi

  for file in "${cpp_files[@]}" "${toml_files[@]}"; do
    if ! cmp -s "$file" "$temp_dir/$file"; then
      formatter_changed=1
      break
    fi
  done

  if [[ $formatter_changed -eq 0 ]]; then
    echo "Code is properly formatted"
    return 0
  fi

  echo "Code needs formatting. Run 'pixi run format' locally." >&2

  for file in "${cpp_files[@]}" "${toml_files[@]}"; do
    if ! cmp -s "$file" "$temp_dir/$file"; then
      printf '  %s\n' "$file" >&2
    fi
  done

  return 1
}

run_format_lane() {
  echo "==> Running CI format-check lane"
  run_format_lane_impl
}

run_build_test_lane() {
  local status

  echo "==> Running CI build-and-test lane"
  run_timed_stage 1 "bash scripts/task/build.sh -p asan" bash "$SCRIPT_DIR/build.sh" -p asan
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi

  run_timed_stage 1 "bash scripts/task/test.sh -p asan" bash "$SCRIPT_DIR/test.sh" -p asan
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi
}

run_static_analysis_semgrep_lane_steps() {
  local level="$1"
  local status

  run_timed_stage "$level" "semgrep --version" bash -c 'command -v semgrep && semgrep --version'
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi

  run_timed_stage "$level" "semgrep scan" semgrep scan --error --metrics=off --config .semgrep.yml --config .semgrep/rules src tests
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi

  run_timed_stage "$level" "python tests/semgrep/verify_semgrep_fixtures.py" python tests/semgrep/verify_semgrep_fixtures.py
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi
}

run_static_analysis_quality_lane_steps() {
  local level="$1"
  local status

  run_timed_stage "$level" "bash scripts/task/build.sh -p quality" bash "$SCRIPT_DIR/build.sh" -p quality
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi
}

run_static_analysis_semgrep_lane() {
  echo "==> Running CI static-analysis-semgrep lane"
  run_static_analysis_semgrep_lane_steps 1
}

run_static_analysis_quality_lane() {
  echo "==> Running CI static-analysis-quality lane"
  run_static_analysis_quality_lane_steps 1
}

require_quality_pr_base_ref() {
  [[ -n "$BASE_REF" ]] || die "static-analysis-quality-pr requires --base-ref"

  git rev-parse --verify "$BASE_REF^{commit}" >/dev/null 2>&1 ||
    die "Base ref '$BASE_REF' is not available locally"
}

collect_changed_quality_files() {
  git diff --name-only --diff-filter=ACMR "$BASE_REF...HEAD" -- src
}

run_clang_tidy_on_changed_sources() {
  clang-tidy \
    -p "$REPO_ROOT/build/quality" \
    --header-filter="$REPO_ROOT/src/.*" \
    --exclude-header-filter="$REPO_ROOT/src/render/chain/api/c/goggles_filter_chain\\.h" \
    "$@"
}

run_static_analysis_quality_pr_lane_steps() {
  local level="$1"
  local status
  local changed_file
  local -a changed_sources=()
  local -a changed_headers=()

  require_quality_pr_base_ref

  while IFS= read -r changed_file; do
    [[ -n "$changed_file" ]] || continue

    case "$changed_file" in
      src/*.c|src/*.cc|src/*.cpp|src/*.cxx)
        changed_sources+=("$changed_file")
        ;;
      src/*.h|src/*.hh|src/*.hpp|src/*.hxx)
        changed_headers+=("$changed_file")
        ;;
    esac
  done < <(collect_changed_quality_files)

  if ((${#changed_sources[@]} == 0 && ${#changed_headers[@]} == 0)); then
    echo "No src C/C++ files changed relative to $BASE_REF; skipping quality static analysis"
    return 0
  fi

  if ((${#changed_headers[@]} > 0)); then
    echo "Changed src headers detected relative to $BASE_REF; running full quality build"
    run_static_analysis_quality_lane_steps "$level"
    return $?
  fi

  echo "Running clang-tidy on changed src files relative to $BASE_REF:"
  printf '  %s\n' "${changed_sources[@]}"

  mkdir -p "$CCACHE_TEMPDIR"

  run_timed_stage "$level" "cmake --preset quality" cmake --preset quality
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi

  run_timed_stage "$level" "clang-tidy changed src files" run_clang_tidy_on_changed_sources "${changed_sources[@]}"
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi
}

run_static_analysis_quality_pr_lane() {
  echo "==> Running CI static-analysis-quality-pr lane"
  run_static_analysis_quality_pr_lane_steps 1
}

run_static_analysis_lane() {
  local status

  echo "==> Running CI static-analysis lane"

  run_timed_stage 1 "static-analysis-semgrep lane" run_static_analysis_semgrep_lane_steps 2
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi

  run_timed_stage 1 "static-analysis-quality lane" run_static_analysis_quality_lane_steps 2
  status=$?
  if [[ $status -ne 0 ]]; then
    return "$status"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --lane)
      [[ $# -ge 2 ]] || die "--lane requires a lane name"
      LANE="$2"
      shift 2
      ;;
    --lane=*)
      LANE="${1#*=}"
      shift
      ;;
    --runner)
      [[ $# -ge 2 ]] || die "--runner requires a runner name"
      RUNNER="$2"
      shift 2
      ;;
    --runner=*)
      RUNNER="${1#*=}"
      shift
      ;;
    --cache-mode)
      [[ $# -ge 2 ]] || die "--cache-mode requires a mode"
      CACHE_MODE="$2"
      shift 2
      ;;
    --cache-mode=*)
      CACHE_MODE="${1#*=}"
      shift
      ;;
    --base-ref)
      [[ $# -ge 2 ]] || die "--base-ref requires a git ref"
      BASE_REF="$2"
      shift 2
      ;;
    --base-ref=*)
      BASE_REF="${1#*=}"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      die "Unknown option: $1"
      ;;
  esac
done

cd "$REPO_ROOT"
CI_START_MS="$(now_ms)"

case "$LANE" in
  all|format|build-test|static-analysis|static-analysis-semgrep|static-analysis-quality|static-analysis-quality-pr)
    ;;
  *)
    die "Unknown lane '$LANE'. Use all, format, build-test, static-analysis, static-analysis-semgrep, static-analysis-quality, or static-analysis-quality-pr"
    ;;
esac

case "$RUNNER" in
  host|container)
    ;;
  *)
    die "Unknown runner '$RUNNER'. Use host or container"
    ;;
esac

if [[ -n "$CACHE_MODE" ]]; then
  case "$CACHE_MODE" in
    cold|warm)
      ;;
    *)
      die "Unknown cache mode '$CACHE_MODE'. Use cold or warm"
      ;;
  esac
fi

if [[ "$RUNNER" == "host" && -n "$CACHE_MODE" ]]; then
  die "--cache-mode is only valid with --runner container"
fi

if [[ "$RUNNER" == "container" && -z "$CACHE_MODE" ]]; then
  CACHE_MODE="warm"
fi

if [[ "${GOGGLES_CI_IN_CONTAINER:-0}" == "1" && "$RUNNER" == "container" ]]; then
  die "Nested container CI runs are not supported"
fi

if [[ "$RUNNER" == "container" ]]; then
  CONTAINER_IMAGE="$(resolve_container_image)"

  set +e
  if [[ "$LANE" == "all" ]]; then
    run_timed_stage 0 "container/format lane" run_container_lane format
    status=$?
    if [[ $status -eq 0 ]]; then
      run_timed_stage 0 "container/build-test lane" run_container_lane build-test
      status=$?
    fi
    if [[ $status -eq 0 ]]; then
      run_timed_stage 0 "container/static-analysis lane" run_container_lane static-analysis
      status=$?
    fi
  else
    run_timed_stage 0 "container/$LANE lane" run_container_lane "$LANE"
    status=$?
  fi
  set -e

  maybe_write_stage_metrics
  print_stage_summary

  if [[ $status -ne 0 ]]; then
    exit "$status"
  fi

  CI_TOTAL_MS="$(( $(now_ms) - CI_START_MS ))"
  echo "All local CI lanes passed in $(format_elapsed_ms "$CI_TOTAL_MS")."
  exit 0
fi

set +e
case "$LANE" in
  all)
    run_timed_stage 0 "format lane" run_format_lane
    status=$?
    if [[ $status -eq 0 ]]; then
      run_timed_stage 0 "build-test lane" run_build_test_lane
      status=$?
    fi
    if [[ $status -eq 0 ]]; then
      run_timed_stage 0 "static-analysis lane" run_static_analysis_lane
      status=$?
    fi
    ;;
  format)
    run_timed_stage 0 "format lane" run_format_lane
    status=$?
    ;;
  build-test)
    run_timed_stage 0 "build-test lane" run_build_test_lane
    status=$?
    ;;
  static-analysis)
    run_timed_stage 0 "static-analysis lane" run_static_analysis_lane
    status=$?
    ;;
  static-analysis-semgrep)
    run_timed_stage 0 "static-analysis-semgrep lane" run_static_analysis_semgrep_lane
    status=$?
    ;;
  static-analysis-quality)
    run_timed_stage 0 "static-analysis-quality lane" run_static_analysis_quality_lane
    status=$?
    ;;
  static-analysis-quality-pr)
    run_timed_stage 0 "static-analysis-quality-pr lane" run_static_analysis_quality_pr_lane
    status=$?
    ;;
esac
set -e

maybe_write_stage_metrics

if [[ $status -ne 0 ]]; then
  if [[ "${GOGGLES_CI_SUPPRESS_SUMMARY:-0}" != "1" ]]; then
    print_stage_summary
  fi
  exit "$status"
fi

if [[ "${GOGGLES_CI_SUPPRESS_SUMMARY:-0}" != "1" ]]; then
  print_stage_summary
  CI_TOTAL_MS="$(( $(now_ms) - CI_START_MS ))"
  echo "All local CI lanes passed in $(format_elapsed_ms "$CI_TOTAL_MS")."
fi
