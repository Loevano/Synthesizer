#!/usr/bin/env bash
set -euo pipefail

DATE="$(date +%F)"
FILE="docs/devlog/${DATE}.md"

if [[ -f "${FILE}" ]]; then
  echo "Dev log already exists: ${FILE}"
  exit 0
fi

cat > "${FILE}" <<TEMPLATE
# ${DATE}

## What I changed
- 

## What I learned
- 

## Problems / Questions
- 

## Next step
- 
TEMPLATE

echo "Created ${FILE}"
