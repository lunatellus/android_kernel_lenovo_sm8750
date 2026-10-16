#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

function info()
{
  local l=10;
  local soc_sha_abbrev="${SOC_SHA:0:$l}";
  local ack_sha_abbrev="${ACK_SHA:0:$l}";
  local ack_sha_kp_abbrev="${ACK_SHA_KP:0:$l}";
  local indent='                             ';

  indent="${indent:0:$l}";

  cat <<INFO

Common files between SOC($soc_sha_abbrev) and KP ACK($ack_sha_kp_abbrev)
are ${#FILES[@]} and there are ${#CHANGES[@]}/${#CHANGES_ALL[@]} changes from
upstream ACK($ack_sha_abbrev) that are touching them.

ACK: $ack_sha_abbrev $ACK_URL
     $indent $ACK_BRANCH
ACK: $ack_sha_kp_abbrev $ACK_ROOT
SOC: $soc_sha_abbrev $SOC_ROOT

INFO
}

#
# This function prints well-formatted multi-line messages using carefully
# constructed printf format strings.
#
# shellcheck disable=SC2059
#
function init_errors()
{
  local error="$1"; shift 1;
  local error_messages=(
    # [0] error 1
"ERROR: KERNEL PLATFORM, soc-repo (SOC), common (ACK) root folders and SHAs are invalid or not set
       KERNEL_PLATFORM: '%s'
       ACK:             '%s'
       ACK SHA in KP:   '%s'
       ACK SHA:         '%s'
       SOC:             '%s'
       SOC SHA:         '%s'
"
    # [1] error 2
"
ERROR: Failed to load list of SOC files Can you check what returns:
ERROR: git -C '%s' ls-tree -r --name-only '%s' -- %s;
ERROR: git -C '%s' ls-tree -r --name-only '%s' -- %s;
"
    # [2] error 3
"
WARNING: No missing upstream changes touching the SOC files between last synced
         ACK SHA '%s' and '%s'
"
    # [3] error 4
"
ERROR: Failed to load list of ALL changes between last synced ACK SHA '%s' and '%s'
"
    # [4] error *
"
ERROR: Unknown init error '%s'
"
    # [5] error 5
"
ERROR: can't add ACK repo folder path in SOC-repo git alternates file
"
    # [6] error 6
"
ERROR: ACK SHA is not set!
"

  );

  case $error in
  1) printf -- "${error_messages[0]}" \
       "$KERNEL_PLATFORM_ROOT"        \
       "$ACK_ROOT"                    \
       "$ACK_SHA_KP"                  \
       "$ACK_SHA"                     \
       "$SOC_ROOT"                    \
       "$SOC_SHA";
  ;;
  2) printf -- "${error_messages[1]}"                        \
       "$SOC_ROOT" "$SOC_SHA"    "${DRIVERS[*]} ${PATHS[*]}" \
       "$ACK_ROOT" "$ACK_SHA_KP" "${DRIVERS[*]} ${PATHS[*]}";;
  3) printf -- "${error_messages[2]}" "$ACK_SHA_KP" "$ACK_SHA"; error=0;;
  4) printf -- "${error_messages[3]}" "$ACK_SHA_KP" "$ACK_SHA"; error=0;;
  5) printf -- "${error_messages[5]}";;
  6) printf -- "${error_messages[6]}" "$ACK_SHA";;
  *) printf -- "${error_messages[4]}" "$error";;
  esac

  return "$error";
}

function set-default-paths()
{
  # Relax user by set some intuitive defaults like current directory as
  # SOC_ROOT, but KP and ACK roots relatively according to the KP workspace
  # layout:
  if [ -z "${KERNEL_PLATFORM_ROOT}${ACK_ROOT}${SOC_ROOT}" ]; then

    SOC_ROOT="$(pwd)";
    ACK_ROOT="${SOC_ROOT}/../../$ACK_REPO_REL_ROOT";
    KERNEL_PLATFORM_ROOT="${SOC_ROOT}/../../";

  elif [ -z "${ACK_ROOT}${SOC_ROOT}" ]; then

    SOC_ROOT="${KERNEL_PLATFORM_ROOT}/$SOC_REPO_REL_ROOT";
    ACK_ROOT="${KERNEL_PLATFORM_ROOT}/$ACK_REPO_REL_ROOT";

  elif [ -z "$ACK_ROOT" ]; then

    ACK_ROOT="${SOC_ROOT}/../../$ACK_REPO_REL_ROOT";

  elif [ -z "$SOC_ROOT" ]; then

    SOC_ROOT="${ACK_ROOT}/../../$SOC_REPO_REL_ROOT";

  fi

  if [ -d "$KERNEL_PLATFORM_ROOT" ]; then
    KERNEL_PLATFORM_ROOT="$(readlink -e "$KERNEL_PLATFORM_ROOT")";

    # If ACK and SOC root folders are not set, set them according to the KP
    # workspace layout:
    [ -z "$ACK_ROOT" ] && ACK_ROOT="${KERNEL_PLATFORM_ROOT}/$ACK_REPO_REL_ROOT";
    [ -z "$SOC_ROOT" ] && SOC_ROOT="${KERNEL_PLATFORM_ROOT}/$SOC_REPO_REL_ROOT";
  fi

  if [ -d "$ACK_ROOT" ]; then
    ACK_ROOT="$(readlink -e "$ACK_ROOT")";
  else
    return 2;
  fi

  if [ -d "$SOC_ROOT" ]; then
    SOC_ROOT="$(readlink -e "$SOC_ROOT")";
  else
    return 3;
  fi
}

function set-defaults()
{
  set-default-paths || return $?;

  DEBUG="$(true-false "$DEBUG")";

  # shellcheck disable=SC2034
  DEBUG_LOG="$(_mktemp ack2soc-debug)";

  INTERACTIVE="$(true-false "$INTERACTIVE")";
  PROGRESS="$(true-false "$PROGRESS")";

  if [ -z "$SOC_SHA" ]; then
    SOC_SHA="$(
      git -C "$SOC_ROOT" log -1 --pretty='%H';
    )" || return 2;
  fi

  if [ -z "$ACK_SHA_KP" ]; then
    ACK_SHA_KP="$(
      git -C "$ACK_ROOT" log -1 --pretty='%H';
    )" || return 3;
  fi

  if [ -z "$ACK_SHA" ]; then
    # TODO: git ls-remote .... ?
    :
    # return 1;
  fi
}

function init()
{
  local changes_all=() changes_count=0;
  local -i i=0;

  [ -z "$ACK_SHA" ] && return 6;

  set-defaults || return 1;

  # Make sure ACK_SHA is available in SOC repo
  git show "$ACK_SHA" >/dev/null 2>&1 || {
    git-add-alternate "$SOC_ROOT" "$ACK_ROOT" || return 5;
  }

  # Build list of regexes representing blocked paths
  blocklist-regexes || return 8;

  # Load the list of common files between SOC_SHA and ACK_SHA_KP reduced with
  # files from the BLOCKLIST
  readarray -t SOC_MONITORED_FILES < <(
    soc-monitored-files
  );
  test "${#SOC_MONITORED_FILES[@]}" -le 0 && return 2;

  # Get list of ALL changes i.e. above list, but do not limit it to changes
  # only in FILES
  readarray -t changes_all < <(
    FILES=();
    git-log "$ACK_ROOT" "$ACK_SHA_KP" "$ACK_SHA" --reverse;
  );
  changes_count="${#changes_all[*]}";
  test "$changes_count" -le 0 && return 4;

  while [[ $i -lt $changes_count ]]; do
    CHANGES_ALL+=([${changes_all[$i]}]=0);
    i+=1;
  done

  return 0;
}

function list-changes-per-teams()
{
  local teams=("${TEAMS[@]}");
  local team='';
  local key='';

  #local -xa FILES=();

  local -i n=1;
  local load_team_data_cmd=(load-team-data);

  if $VERBOSE; then
    printf -- "Reading teams data ...\n";
    printf -- "  folder : '%s'\n" "$TEAMS_DATA_ROOT";
    printf -- "  pattern: '%s'\n" "$TEAMS_PATTERN";
  fi

  if [ "${TEAMS[0]}" = "all" ]; then
    readarray -t teams < <(
      find "$TEAMS_DATA_ROOT" -type f \
        | grep -Pi "^\\Q${TEAMS_DATA_ROOT}/\\E\K$TEAMS_PATTERN";
    );
    # This is an empty argument, due to double expansion performed by eval
    load_team_data_cmd+=('');
    n+=1;
  fi

  for team in "${teams[@]}"; do
    load_team_data_cmd[n]="$team";
    "${load_team_data_cmd[@]}" || {
      printf -- "ERROR: Failed to load data for team '%s'\n" "$team";
      continue;
    }
    printf -- "\nProcessing team %s (POCs %s) ...\n" \
      "$TEAM_NAME"                                   \
      "$(IFS=,; echo "${TEAM_POCS[*]}")";

    CHANGES=();
    for key in "${!CHANGES_ALL[@]}"; do
      CHANGES_ALL["$key"]=0;
    done

    readarray -t FILES < <(
      team-monitored-files;
    );

    if [ "${#FILES[@]}" -eq 0 ]; then
      cat <<WARN

WARNING: No files to process!!!

  This is based on the intersection of files:
    * Existing in common folder at $ACK_SHA_KP
    * Owned by the team $TEAM_NAME
    * Expanded by the paths you provided as free arguments
    * Expanded by and drivers you set via --driver options

WARN
      continue;
    fi

    actions || {
      echo  "FAILED: Not able to collect changes for team '$TEAM_NAME'" >&2;
    }
  done
}

function main()
{

  if [ ${#TEAMS[@]} -gt 0 ]; then
    list-changes-per-teams || return 1;
    return 0;
  fi
  FILES=("${SOC_MONITORED_FILES[@]}");
  actions || return 1;

  return 0;
}

function actions()
{
  # Get list of ALL changes, between ACK_SHA_KP (integrated already ACK SHA in
  # KP) and ACK_SHA, which are touching _only_ the FILES (common between SOC_SHA
  # and ACK_SHA_KP).

  list-changes "$ACK_ROOT" "$ACK_SHA_KP" "$ACK_SHA" || {
    echo "ERROR: list-changes: $?";
  }
  test "${#CHANGES[@]}" -le 0 && return 1;

  info || {
    printf -- '\nERROR(%s): Fail to collect info\n\n' "$?";
    return 2;
  }

  if $REPORT_STALLED; then
    echo "List of stalled files (not updated since $NOT_UPDATED_WARNING_PERIOD days)";
    report-stalled "${FILES[@]}" || return 3;
  fi

  report || {
    printf -- '\nERROR(%s): Fail to get list of missing chages\n\n' "$?";
    return 4;
  }
}
