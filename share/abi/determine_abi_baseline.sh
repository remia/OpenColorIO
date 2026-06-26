#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright Contributors to the OpenColorIO Project.
#
# Resolve the ABI baseline for the ABI compatibility check
# (.github/workflows/abi_compatibility.yml).
#
# Given the release branch a change targets (RB-MAJOR.MINOR), this prints the
# tag to use as the ABI baseline: the series anchor vMAJOR.MINOR.0 (the ABI
# first published under the MAJOR.MINOR SONAME), falling back to the lowest
# existing patch tag in the series if the anchor tag is missing.
#
# Usage:
#   determine_abi_baseline.sh <base-ref> [change-description]
#
#   <base-ref>           The targeted branch, e.g. "RB-2.6".
#   <change-description> Human-readable description of the proposed change, used
#                        only for reporting, e.g. "PR #123 -> RB-2.6".
#                        Defaults to <base-ref>.
#
# Results are written as key=value lines to the file named by $GITHUB_OUTPUT
# when set (GitHub Actions step outputs), otherwise to stdout for local use:
#   skip=true|false
#   baseline=<tag>        (only when skip=false)
#   current_desc=<text>   (only when skip=false)

set -euo pipefail

BASE_REF="${1:?usage: determine_abi_baseline.sh <base-ref> [change-description]}"
CURRENT_DESC="${2:-${BASE_REF}}"

# Emit a key=value step output (or print it when run outside GitHub Actions).
emit() {
    if [ -n "${GITHUB_OUTPUT:-}" ]; then
        echo "$1" >> "${GITHUB_OUTPUT}"
    else
        echo "$1"
    fi
}

# The CI container checks out the workspace as a different user than the one git
# expects; mark everything safe so git/worktree commands work. Harmless locally.
git config --global --add safe.directory '*'

echo "Target release branch: ${BASE_REF}"

# The release branch name encodes the series: RB-MAJOR.MINOR.
if [[ ! "${BASE_REF}" =~ ^RB-([0-9]+)\.([0-9]+)$ ]]; then
    echo "::notice::Base branch '${BASE_REF}' is not an RB-MAJOR.MINOR release branch; skipping ABI check."
    emit "skip=true"
    exit 0
fi
MAJOR="${BASH_REMATCH[1]}"
MINOR="${BASH_REMATCH[2]}"

# Anchor on X.Y.0, the ABI first published under the MAJOR.MINOR SONAME.
# Comparing the proposed change directly against the series anchor enforces
# compatibility with every consumer linked against any release in the series.
# Fall back to the lowest existing patch tag if X.Y.0 is missing for some reason.
ANCHOR="v${MAJOR}.${MINOR}.0"
if git rev-parse -q --verify "refs/tags/${ANCHOR}" >/dev/null; then
    BASELINE="${ANCHOR}"
else
    # `|| true`: an empty match (or head closing the pipe early under pipefail)
    # is an expected outcome handled by the emptiness check below, not an error.
    BASELINE=$(git tag --list "v${MAJOR}.${MINOR}.*" | grep -E "^v${MAJOR}\.${MINOR}\.[0-9]+$" | sort -V | head -1 || true)
fi
if [ -z "${BASELINE}" ]; then
    echo "::notice::The ${MAJOR}.${MINOR} series has no published release tag yet, so there is no ABI to preserve. Skipping."
    emit "skip=true"
    exit 0
fi

echo "Comparing '${CURRENT_DESC}' against baseline ${BASELINE}"
emit "skip=false"
emit "baseline=${BASELINE}"
emit "current_desc=${CURRENT_DESC}"
