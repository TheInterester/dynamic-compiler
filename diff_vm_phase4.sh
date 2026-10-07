#!/usr/bin/env bash
set -euo pipefail

# Run this from inside fa25-team8 (new implementation)

# ==== Paths =========================================================
NEW_REPO="$(pwd)"                                  # fa25-team8 (new)
BASE_REPO="$(cd ../fa25-team8-phase4 && pwd)"      # fa25-team8-phase4 (old)

# Binaries produced by ./build.sh (CMake)
NEW_BIN="$NEW_REPO/build/cmake/release/mitscript"
BASE_BIN="$BASE_REPO/build/cmake/release/mitscript"

TEST_DIR="$NEW_REPO/tests/phase4/public"

# ===================================================================

if [[ ! -x "$BASE_BIN" ]]; then
  echo "Baseline binary not found or not executable: $BASE_BIN" >&2
  exit 1
fi

if [[ ! -x "$NEW_BIN" ]]; then
  echo "New VM binary not found or not executable: $NEW_BIN" >&2
  exit 1
fi

if [[ ! -d "$TEST_DIR" ]]; then
  echo "Test directory not found: $TEST_DIR" >&2
  exit 1
fi

echo "Baseline: $BASE_BIN"
echo "New     : $NEW_BIN"
echo "Tests   : $TEST_DIR"
echo

shopt -s nullglob

for test in "$TEST_DIR"/*.mit "$TEST_DIR"/*.mitbc; do
  [[ -e "$test" ]] || continue   # skip if no matches
  
  name="$(basename "$test")"
  ext="${test##*.}"
  
  echo "== $name =="
  
  base_out=$(mktemp)
  new_out=$(mktemp)
  
  if [[ "$ext" == "mit" ]]; then
    # ---------------------------------------------------
    # .mit: compile in each repo, then run its VM
    # ---------------------------------------------------
    base_bc=$(mktemp --suffix=".mitbc")
    new_bc=$(mktemp --suffix=".mitbc")
    
    # Compile with baseline
    if "$BASE_BIN" compile "$test" -o "$base_bc" > /dev/null 2>&1; then
      base_compile_status=0
    else
      base_compile_status=$?
    fi
    
    # Compile with new
    if "$NEW_BIN" compile "$test" -o "$new_bc" > /dev/null 2>&1; then
      new_compile_status=0
    else
      new_compile_status=$?
    fi
    
    if [[ "$base_compile_status" != 0 || "$new_compile_status" != 0 ]]; then
      echo "  COMPILE FAILED:"
      echo "    baseline: status=$base_compile_status"
      echo "    new     : status=$new_compile_status"
      rm -f "$base_bc" "$new_bc" "$base_out" "$new_out"
      exit 1
    fi
    
    # Run VMs on their compiled bytecode
    if "$BASE_BIN" vm "$base_bc" >"$base_out" 2>&1; then
      base_status=0
    else
      base_status=$?
    fi
    
    if "$NEW_BIN" vm "$new_bc" >"$new_out" 2>&1; then
      new_status=0
    else
      new_status=$?
    fi
    
    rm -f "$base_bc" "$new_bc"
  else
    # ---------------------------------------------------
    # .mitbc: directly run vm on the same file
    # ---------------------------------------------------
    if "$BASE_BIN" vm "$test" >"$base_out" 2>&1; then
      base_status=0
    else
      base_status=$?
    fi
    
    if "$NEW_BIN" vm "$test" >"$new_out" 2>&1; then
      new_status=0
    else
      new_status=$?
    fi
  fi
  
  # Compare exit codes
  if [[ "$base_status" != "$new_status" ]]; then
    echo "  STATUS MISMATCH:"
    echo "    baseline: $base_status"
    echo "    new     : $new_status"
    echo "  --- baseline output ---"
    cat "$base_out"
    echo "  --- new output ---"
    cat "$new_out"
    rm -f "$base_out" "$new_out"
    exit 1
  fi
  
  # Compare outputs ONLY if both succeeded (status 0)
  # If both errored (non-zero status), skip output comparison
  if [[ "$base_status" == 0 ]]; then
    if ! diff -u "$base_out" "$new_out" >/dev/null; then
      echo "  OUTPUT MISMATCH:"
      echo "  --- baseline ---"
      cat "$base_out"
      echo "  --- new ---"
      cat "$new_out"
      rm -f "$base_out" "$new_out"
      exit 1
    fi
  else
    echo "  Both errored (status=$base_status) - skipping output comparison"
  fi
  
  rm -f "$base_out" "$new_out"
done

echo
echo "All differential tests passed!"
