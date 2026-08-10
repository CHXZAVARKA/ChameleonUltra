# CHXZAVARKA firmware fork

The `main` branch is the custom firmware integration branch. It contains the
latest accepted upstream firmware plus focused custom commits, such as atomic
whole-slot swapping. Future LED and rainbow work should be added as separate
commits or feature branches and then merged into `main`.

## Automatic upstream updates

The `Merge upstream firmware` workflow runs every Monday and can also be
started manually from GitHub Actions. It fetches
`RfidResearchGroup/ChameleonUltra:main`, creates a normal merge commit when
needed, runs the host slot-transaction tests, and pushes only when the merge and
tests succeed.

The workflow never force-pushes `main`. If upstream conflicts with a custom
feature, the job stops and leaves the fork unchanged. Resolve that conflict in
a local branch, run the firmware checks, and push the resulting merge normally.

## Local remotes

- `upstream`: `RfidResearchGroup/ChameleonUltra`, fetch only.
- `fork`: `CHXZAVARKA/ChameleonUltra`, fetch and push.

Use focused commits for each custom feature. This keeps future upstream merges
and hardware debugging understandable.
