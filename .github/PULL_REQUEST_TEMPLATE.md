## Summary

- TBD

## Scope

- Branch type: `feature` / `fix` / `docs` / `ui` / `test` / `chore` / `infra` / `core` / `dsp` / `routing` / `architecture` / `hotfix`
- Target branch: `dev` unless this is a tested `dev -> main` promotion
- Protected core paths changed: yes / no

## Verification

- [ ] `./scripts/build.sh`
- [ ] `node --check src/ui_web/app.js` if UI JavaScript changed
- [ ] Manual app check if behavior changed

## Risk

- TBD
