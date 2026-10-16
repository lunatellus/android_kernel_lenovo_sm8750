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
#
# Stores the current team name during iteration over teams. Referenced by other
# files to produce more informative messages.
: "$TEAM_NAME"

function get-soc-tip()
{
  local tip_regex="refs/heads/${SOC_BRANCH#refs/heads/}\$";

  git ls-remote -h "$SOC_URL" \
    | grep -P  "$tip_regex"   \
    | grep -Po '^[^\s]+';
}

function team-files()
{
  files "$SOC_ROOT" "$SOC_SHA" "${TEAM_PATHS[@]}";
}

#
# Provided paths collected in arrays DRIVERS and PAHTS is relaxed to be patterns
# and this function expands them to list of files which exists in the provided
# SHA
#
function soc-files()
{
  files "$SOC_ROOT" "$SOC_SHA" "${DRIVERS[@]}" "${PATHS[@]}";
}

function ack-files()
{
  files "$ACK_ROOT" "$ACK_SHA_KP" "${DRIVERS[@]}" "${PATHS[@]}";
}

#
# Load common files between SOC and ACK (state @ACK_SHA_KP) limited by:
# 1. Cmdline:
#      --driver options
#      Any paths provided as free arguments
# 2. BLOCKLIST array
#
function soc-monitored-files()
{
  $DEBUG && set -x;

  comm -12                 \
    <(soc-files | sort -u) \
    <(ack-files | sort -u) \
      | blocklist-filter;
}

function team-monitored-files()
{
  $DEBUG && set -x;

  local files='';

  files="$(
    printf -- "%s\n"              \
      "${SOC_MONITORED_FILES[@]}" \
        | sort -u;
  )";

  comm -12                  \
    <(echo "$files")        \
    <(team-files | sort -u) \
      | blocklist-filter;
}

function read-section()
{
  local label="$1"; shift 1;

  [ -z "$label" ] && return 1;

  perl -0 -s -ne '
    $status=0;

    /^\s*${label}:?\s*$
     (.+?)
     (^\s*$|\z)
    /ismx or exit 1;

    $list = $1;
    $list =~ s/^\s+|\s+$//mg;
    print $list;

    exit $status;
  ' -- -label="$label";
}

# This function relax user to provide either team name
#
#
function load-team-data()
{
  local team_name="$1"; shift 1;
  local team_info="$1"; shift 1;

  local team_regex="\\b${team_name}\\b";
  local -i match_count=0;
  local -i n=1;

  if [ -n "$team_name" ]; then
    team_info="$(

      grep -ril "$team_regex" "$TEAMS_DATA_ROOT" && exit 0;

      find "$TEAMS_DATA_ROOT" -maxdepth 1 -type f \
        | grep -Pi "$team_regex"                || exit 1;

    )" && {
      match_count="$(echo "$team_info" | wc -l)";
      if [[ $match_count -gt 1 ]]; then
        printf -- "\nERROR: Several teams matches provided team name '%s'\n" \
          "$team_name";
        return 1;
      fi
    }
  fi

  team_info="$(
    realpath --no-symlinks "$team_info" 2>> "$DEBUG_LOG";
  )" || return 1;

  TEAM_NAME="$(                 read-section team    < "$team_info")";
  readarray -t TEAM_POCS    < <(read-section pocs    < "$team_info");
  readarray -t TEAM_DRIVERS < <(read-section drivers < "$team_info");
  readarray -t TEAM_PATHS   < <(read-section paths   < "$team_info");

  n=$((
    ${#TEAM_POCS[@]}
    * ${#TEAM_DRIVERS[@]}
    * ${#TEAM_PATHS[@]}
  ));

  # Fail, if one or more arrays are empty.
  [[ $n -eq 0 ]] && return 1;

  return 0;
}

#
# This function dumps list of all files, which are part of SOC repo. It embeds
# the rules to prepare the list.
#
function repo_soc_files()
{
  git -C "$SOC_ROOT" show HEAD:"$SOC_FILE_LIST" \
    | grep -P '\.(c|h)'                         \
    | grep -Po '^\s+"\s*\K[^"]+';
}

#
# When doing git merge|cherry-pick, we need to remove those files with
# conflicts, which are NOT SOC related i.e. need to discard the garbage
# added on importing upstream changes into SOC repo.
#
# This function dumps the above described list of files.
#
function list-no-soc-files()
{
  # Remove:
  # 1) 'DD|DU'
  #    Those files with conflicts which were deleted in SOC repo
  #
  # 2) 'A '
  #    Remove newly added by the merge
  #
  # 3) 'UA'
  #    Exists on the merge side but the change which adds the file
  #    is discarded on some reason and now delta is added into that
  #    file

  git -C "$SOC_ROOT" status --porcelain \
    | sort                              \
    | grep -Po '(?<=^(DD|DU|A |UA) ).+$';
}

function discard-no-soc-files()
{
  local no_soc_files=();

  local -x GIT_DIR="${SOC_ROOT}/.git";
  local -x GIT_WORK_TREE="$SOC_ROOT";

  readarray -t no_soc_files < <(list-no-soc-files);

  if [[ ${#no_soc_files[@]} -gt 0 ]]; then
    $DEBUG && set -x;

    git rm -rf -- "${no_soc_files[@]}";

    $DEBUG && set +x;
  fi

  # TODO: Can we add here a wiki or some info, which describes why bindings
  #       directory is discarded i.e. converted to symlink?
  if [[ ! -L $DEVICETREE_BINDINGS_PATH ]]; then
    $DEBUG && set -x;

    git rm -rf        -- "${DEVICETREE_BINDINGS_PATH}/";
    git checkout HEAD -- "${DEVICETREE_BINDINGS_PATH}";

    $DEBUG && set +x;
  fi
}

function list-drivers()
{
  local root_dir="$1"; shift 1;

  (
    cd "$root_dir" || exit 1;

    $DEBUG && set -x;

    find . -type f -iname "$DRIVER_MARKER" \
      | trim-paths;
  )
}
