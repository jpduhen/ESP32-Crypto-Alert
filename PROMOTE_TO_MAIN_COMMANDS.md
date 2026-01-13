# Promote Branch to Main - Step by Step Commands

## Work Branch
`<WORK_BRANCH>` = `codex/verify-calculations-for-time-values`

---

## Step-by-Step Commands

### Step 1: Confirm current branch and HEAD
```bash
git branch --show-current
git log --oneline -1 HEAD
```
**Explanation:** Verify you're on the correct branch and see its latest commit.

---

### Step 2: Fetch latest from origin
```bash
git fetch origin
```
**Explanation:** Get the latest state from GitHub to ensure we have current remote references.

---

### Step 3: Create and push backup branch from current remote main
```bash
git branch -f legacy-main origin/main
git push origin legacy-main:legacy-main
```
**Explanation:** Create local `legacy-main` pointing to current `origin/main`, then push it to remote as backup.

---

### Step 4: Checkout work branch and rename to main
```bash
git checkout codex/verify-calculations-for-time-values
git branch -m main
```
**Explanation:** Switch to work branch, then rename it locally to `main` (preserves all commits from work branch only).

---

### Step 5: Force push new main to origin (with lease)
```bash
git push --force-with-lease origin main
```
**Explanation:** Update `origin/main` to point to new main, using `--force-with-lease` for safety (fails if remote changed).

---

### Step 6: Set upstream tracking
```bash
git branch --set-upstream-to=origin/main main
```
**Explanation:** Configure local `main` to track `origin/main` for future pushes/pulls.

---

## Verification Commands

### Check local branches
```bash
git log --oneline --decorate -n 5 main
git log --oneline --decorate -n 5 legacy-main
```

### Check remote branches
```bash
git show -s --oneline origin/main
git show -s --oneline origin/legacy-main
```

### Compare commit hashes
```bash
git rev-parse main
git rev-parse origin/main
git rev-parse legacy-main
git rev-parse origin/legacy-main
```

---

## Rollback Instructions

If you made a mistake and need to restore `origin/main` from `legacy-main`:

```bash
# Option 1: Restore main from legacy-main (destructive)
git checkout main
git reset --hard origin/legacy-main
git push --force-with-lease origin main

# Option 2: Create new branch from legacy-main and switch (safer)
git checkout -b restored-main origin/legacy-main
git push origin restored-main:main --force-with-lease
git checkout main
git branch -D main  # Delete local main
git checkout -b main origin/main  # Recreate from restored remote
```

**Verification after rollback:**
```bash
git show -s --oneline origin/main
git show -s --oneline origin/legacy-main
# Should show same commit hash
```

---

## Quick Script (All-in-One)

Save as `promote-to-main.sh` and run:
```bash
chmod +x promote-to-main.sh
./promote-to-main.sh
```

Or use the provided `PROMOTE_TO_MAIN.sh` script.

---

## Important Notes

- **No merging:** This approach does NOT merge anything from old main into new main
- **Force-with-lease:** Safer than `--force` - will fail if someone else pushed to main
- **Backup first:** `legacy-main` preserves the old main branch
- **Collaboration:** Inform team members before force-pushing to main
- **Verification:** Always verify commits match expectations before and after
