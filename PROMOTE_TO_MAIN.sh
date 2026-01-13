#!/bin/bash
# Promote codex/verify-calculations-for-time-values to main
# Preserve current main as legacy-main

set -e  # Exit on error

WORK_BRANCH="codex/verify-calculations-for-time-values"
LEGACY_BRANCH="legacy-main"

echo "=== Step 1: Confirm current branch and HEAD ==="
git branch --show-current
git log --oneline -1 HEAD

echo ""
echo "=== Step 2: Fetch latest from origin ==="
git fetch origin

echo ""
echo "=== Step 3: Create and push backup branch from current remote main ==="
git branch -f ${LEGACY_BRANCH} origin/main
git push origin ${LEGACY_BRANCH}:${LEGACY_BRANCH}

echo ""
echo "=== Step 4: Checkout work branch and rename to main ==="
git checkout ${WORK_BRANCH}
git branch -m main

echo ""
echo "=== Step 5: Force push new main to origin (with lease) ==="
git push --force-with-lease origin main

echo ""
echo "=== Step 6: Set upstream tracking ==="
git branch --set-upstream-to=origin/main main

echo ""
echo "=== Verification ==="
echo "--- Local main (new) ---"
git log --oneline --decorate -n 5 main
echo ""
echo "--- Local legacy-main (old main) ---"
git log --oneline --decorate -n 5 ${LEGACY_BRANCH}
echo ""
echo "--- Remote origin/main (new) ---"
git show -s --oneline origin/main
echo ""
echo "--- Remote origin/legacy-main (old main) ---"
git show -s --oneline origin/${LEGACY_BRANCH}

echo ""
echo "=== SUCCESS ==="
echo "New main is now: ${WORK_BRANCH}"
echo "Old main preserved as: ${LEGACY_BRANCH}"
