#!/usr/bin/env bash
set -euo pipefail

# OpenSpec CI Validation Script
# Checks proposals for: validation errors, incomplete tasks, unarchived completed proposals
# Environment variables:
#   REPORT_FILE - output file for PR comment (default: /tmp/openspec-report.md)
#   PR_NUMBER   - PR number for posting comment (optional)
#   GH_TOKEN    - GitHub token for gh CLI (optional)

REPORT_FILE="${REPORT_FILE:-/tmp/openspec-report.md}"
HAS_ERRORS=false

# Start report
cat > "$REPORT_FILE" << 'EOF'
## 🔍 OpenSpec Validation Report

EOF

# 1. Validate all proposals
echo "Checking proposal validation..."
if ! validation_output=$(openspec validate --all --strict 2>&1); then
    HAS_ERRORS=true
    cat >> "$REPORT_FILE" << EOF
### ❌ Proposal Validation Failed

The following proposals have validation errors:

\`\`\`
$validation_output
\`\`\`

**How to fix:**
1. Run \`openspec validate <change-id> --strict\` locally
2. Common issues: Missing \`#### Scenario:\` headers, requirements without scenarios
3. See \`openspec/AGENTS.md\` for spec format reference

---

EOF
fi

# 2. Check incomplete tasks
echo "Checking task completion..."
incomplete=$(openspec list 2>/dev/null | grep -E '[0-9]+/[0-9]+ tasks' | awk '{split($2,a,"/"); if(a[1]<a[2]) print $1, a[1], a[2]}' || true)

if [[ -n "$incomplete" ]]; then
    HAS_ERRORS=true
    cat >> "$REPORT_FILE" << 'EOF'
### ❌ Incomplete Tasks Found

The following proposals have incomplete tasks:

| Proposal | Completed | Total | Remaining |
|----------|-----------|-------|-----------|
EOF
    while read -r change_id completed total; do
        remaining=$((total - completed))
        echo "| \`$change_id\` | $completed | $total | $remaining |" >> "$REPORT_FILE"
    done <<< "$incomplete"

    cat >> "$REPORT_FILE" << 'EOF'

**How to fix:**
1. Open `openspec/changes/<proposal-id>/tasks.md`
2. Complete all tasks marked with `- [ ]`
3. Mark completed tasks with `- [x]`

---

EOF
fi

# 3. Check unarchived completed proposals
echo "Checking for unarchived proposals..."
completed_proposals=$(openspec list 2>/dev/null | grep "Complete" | awk '{print $1}' || true)

if [[ -n "$completed_proposals" ]]; then
    HAS_ERRORS=true
    cat >> "$REPORT_FILE" << 'EOF'
### ❌ Unarchived Completed Proposals

The following proposals have all tasks complete but are not archived:

EOF
    while read -r change_id; do
        echo "- \`$change_id\`" >> "$REPORT_FILE"
    done <<< "$completed_proposals"

    cat >> "$REPORT_FILE" << 'EOF'

**How to fix:**
1. Run `openspec archive <change-id> --yes` to archive the proposal
2. This will move the proposal to `openspec/changes/archive/`
3. Include the archived proposal in this PR

---

EOF
fi

# Add quick reference
cat >> "$REPORT_FILE" << 'EOF'
<details>
<summary>📚 Quick Reference</summary>

```bash
openspec validate <change-id> --strict  # Validate a proposal
openspec list                           # List all proposals
openspec archive <change-id> --yes      # Archive a proposal
```

</details>
EOF

# Post PR comment if in CI and PR_NUMBER is set
if [[ -n "${PR_NUMBER:-}" ]] && [[ "$HAS_ERRORS" == "true" ]]; then
    echo "Posting comment to PR #$PR_NUMBER..."
    gh pr comment "$PR_NUMBER" --body-file "$REPORT_FILE" || echo "::warning::Failed to post PR comment"
fi

# Show report contents
echo ""
echo "=== Report Contents ==="
cat "$REPORT_FILE"
echo "======================="

# Exit with error if checks failed
if [[ "$HAS_ERRORS" == "true" ]]; then
    echo ""
    echo "::error::OpenSpec validation failed"
    exit 1
fi

echo ""
echo "✅ All OpenSpec checks passed!"
