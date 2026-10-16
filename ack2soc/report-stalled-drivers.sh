#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

# Satisfy ShellCheck SC2034:
#
# List all variables that are false positives for this rule and explain
# why they must be exempt.
#
# These vars are unused here but are read by Git at runtime to locate the
# metadata directory and work tree. This avoids cd'ing into the work tree and
# keeps Git command lines simple by removing the need to pass options equivalent
# to these variables. It also supports detached Git-dir/work-tree setups.
: "$GIT_DIR" "$GIT_WORK_TREE"

function stalled()
{
  local file="$1";        shift 1;
  local sha="${1:-HEAD}"; shift 1;

  local -x GIT_DIR="${SOC_ROOT}/.git";
  local -x GIT_WORK_TREE="$SOC_ROOT";

  local one_day="$((24*60*60))"; # one day in seconds
  local file_date;
  local cur_date;
  local -i duration=-1;

  file_date="$(
    git log --pretty='%ct' -1 "$sha" -- "$file";
  )" || return 2;

  # Means that file is not part of the tree
  [ -z "$file_date" ] && return 1;

  cur_date="$(date +"%s")" || return 3;

  duration="$(
    echo "($cur_date - $file_date) / $one_day" | bc
  )" || return 4;

  echo "$duration";
  test "$duration" -gt "$NOT_UPDATED_WARNING_PERIOD";
}

function report-stalled()
{
  local files=("$@");

  local file;
  local duration=-1;

  for file in "${files[@]}"; do
    duration="$(stalled "$file")";
    status="$?";
    if [ $status -eq 0 ]; then
      printf -- "%4s days: %s\n" "$duration" "$file";
    elif [ $status -gt 1 ]; then
      echo "ERROR: Failed to get duration status for $file" >&2;
    fi
  done
}
