#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

declare -gx  KERNEL_PLATFORM_ROOT;

declare -gx  ACK_URL;
declare -gx  ACK_BRANCH_BASENAME;
declare -gx  ACK_BRANCH;
declare -gx  LTS_BRANCH;

# This is the SHA, which will be scanned for new changes. On LTS import this is
# the SHA being imported i.e. LTS manifest change reset common folder to this
# SHA.
declare -gx  ACK_SHA;

# This is the SHA on which is reset $ACK_ROOT (common) folder i.e. this was
# already imported into kernel platform (KP) via some previous LTS.
declare -gx  ACK_SHA_KP;
declare -gx  ACK_REPO_REL_ROOT;
declare -gx  ACK_ROOT;

declare -gx  SOC_URL;
declare -gx  SOC_BRANCH;
declare -gx  SOC_SHA;
declare -gx  SOC_REPO_REL_ROOT;
declare -gx  SOC_ROOT;

declare -gx  LIST_TEAMS;
declare -gx  TEAMS_POCS_PAGE;
declare -gx  TEAMS_DATA_ROOT;
declare -gx  TEAMS_PATTERN;
declare -gxa TEAMS;

declare -gx  TEAM_NAME='';
declare -gxa TEAM_POCS=();
declare -gxa TEAM_DRIVERS=();
declare -gxa TEAM_PATHS=();

declare -gx  LIST_DRIVERS;
declare -gxa DRIVERS;

declare -gxa PATHS;
declare -gxa FILES;
declare -gxa SOC_MONITORED_FILES;
declare -gxa BLOCKLIST;
declare -gxa BLOCKLIST_REGEXES;

declare -gx  DRIVER_MARKER;
declare -gx  DEVICETREE_BINDINGS_PATH;

declare -gx  REPORT_STALLED=false;
declare -gxi NOT_UPDATED_WARNING_PERIOD;
declare -gx  REPORT_ALL=false;

declare -gxa CHANGES=();
declare -gxA CHANGES_ALL=();

declare -gx  INTERACTIVE;
declare -gx  PROGRESS;
declare -gx  DEBUG;
declare -gx  VERBOSE;

declare -gx  TIMESTAMP;
declare -gx  DEBUG_LOG;
declare -gxa DELETE_QUEUE;

# Page with various teams details, like POCs, list of files monitored by the
# team, etc.
TEAMS_POCS_PAGE='https://confluence.qualcomm.com/confluence/display/LK/LTS#LTS-POCs';
TEAMS_DATA_ROOT='data/teams';
TEAMS_DATA_ROOT="${INCLUDE_PATH}/${TEAMS_DATA_ROOT}";
TEAMS_PATTERN='.*(linux|display|audio|ssg|wlan).*';
TEAMS=();

# List of keywords which are matching to list of files and directories
DRIVERS=();

# List of files/dirs in SOC repo to scan for missing changes. If a path is
# directory, then all files and subfolders will be scanned recursively.
PATHS=();

# These are all common files between SOC_SHA and ACK_SHA_KP reduced with
# files from the BLOCKLIST
SOC_MONITORED_FILES=();

# Files to process. This is prepared from the above arrays by expanding their
# values to existing files in SOC repo and applying filtering of excluded files.
# These are the files for which is required to list/pick the changes from
# upstream (ACK_SHA). FILES is equal or subset of SOC_MONITORED_FILES. It is a
# subset, when we would like to limit the scan range to a particular file(s),
# dir(s) OR driver(s) files.
FILES=();

ACK_URL="${ACK_URL:-ssh://review-android.quicinc.com:29418/kernel/common}";
ACK_BRANCH_BASENAME="${ACK_BRANCH_BASENAME:-android-mainline}";
ACK_BRANCH="${ACK_BRANCH:-refs/heads/aosp/$ACK_BRANCH_BASENAME}";
ACK_REPO_REL_ROOT="${ACK_REPO_REL_ROOT:-kernel_platform/common}";

SOC_URL="${SOC_URL:-ssh://review-android.quicinc.com:29418/kernel/qcom}";
SOC_BRANCH="${SOC_BRANCH:-refs/heads/kernel.lnx.0.0}";
SOC_REPO_REL_ROOT="${SOC_REPO_REL_ROOT:-kernel_platform/soc-repo}";

# Existence of this file marks the containing folder as a driver folder
# TODO: Is this still valid?
DRIVER_MARKER='modules.bzl';
DEVICETREE_BINDINGS_PATH="${DEVICETREE_BINDINGS_PATH:-Documentation/devicetree/bindings}";

NOT_UPDATED_WARNING_PERIOD=60 # days

# Any delta done by an ACK change in below path prefixes, will be NOT taken
# into SOC repo.
BLOCKLIST=(
  "$DEVICETREE_BINDINGS_PATH"
  'scripts'
  'arch/arm64/boot/dts'
  'BUILD.bazel'
  'arch/arm64/configs/consolidate.fragment'
  'arch/arm64/configs/generic_vm_defconfig'
);
BLOCKLIST_REGEXES=();

TIMESTAMP="$(date +"%Y-%m-%d_%H.%M.%S")";

DEBUG="${DEBUG:-false}";
INTERACTIVE="${INTERACTIVE:-false}";
PROGRESS="${PROGRESS:-false}";
VERBOSE="${VERBOSE:-false}";

LIST_DRIVERS='false';
LIST_TEAMS='false';

DELETE_QUEUE=();
