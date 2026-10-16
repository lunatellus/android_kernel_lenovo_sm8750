#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

INCLUDE_PATH="$(
  realpath -m \
    "$(dirname "$0")/$(basename "${0%.sh}")"
)" || exit 1;

INCLUDE_CMD='include.sh';

INCLUDES=(
   "${INCLUDE_PATH}/env.sh"
   "${INCLUDE_PATH}/cmdline.sh"
   "${INCLUDE_PATH}/misc.sh"
   "${INCLUDE_PATH}/utils.sh"
   "${INCLUDE_PATH}/soc-utils.sh"
   "${INCLUDE_PATH}/report-missing-changes.sh"
   "${INCLUDE_PATH}/report-stalled-drivers.sh"
);

# Load all functionality
#
# IGNORE: https://www.shellcheck.net/wiki/SC1090
# shellcheck disable=SC1090
source "${INCLUDE_PATH}/${INCLUDE_CMD}" "${INCLUDES[@]}" || exit 1;

# =============================================================================
#                                  M A I N
# =============================================================================

trap cleanup EXIT;

# Nothing to do!
[ $# -le 0 ] && usage && exit 0;

parse_cmdline 'process_options_ack2soc' "$@" || {
 printf -- '\nERROR(%s): Fail to parse command line\n\n' "$?";
 exit 1;
}

if $LIST_DRIVERS; then
  echo 'This is the complete list of SOC drivers:' >&2;

  set-default-paths || {
    init_errors 1 && exit 0;
    printf -- '\nERROR: Fail to initialize data\n\n';
    exit 3;
  }

  list-drivers "$SOC_ROOT" || {
    printf -- '\nERROR: Fail to list drivers\n\n';
    exit 2;
  }
  exit 0;
fi

init || {
  init_errors $? && exit 0;
  printf -- '\nERROR: Fail to initialize data\n\n';
  exit 3;
}

main || {
  printf -- '\nERROR(%s): Fail to collect missing changes\n\n' "$?";
  exit 5;
}
