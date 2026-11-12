# Beta 2.4.1 OTA installation decision

**Decision:** keep muOS Archive Manager as the installation authority.

The muOS 2606.0 application launcher uses `/run/muos/storage/application`, a
bind-mounted abstraction for either SD1 or SD2. App-owned attempts to rename
that mount or replace running executables have not been validated. Direct
calls into internal muOS archive scripts are not a documented stable public
API for package installation. Therefore this beta does not implement silent
replacement, an undocumented installer invocation, or a claimed automatic
rollback.

**Available workflow:** optional single check/day; press A to prepare the
selected update; download and integrity-check; back up user settings; press A
to exit; install through Archive Manager. This is deliberately two distinct
user decisions (download and installation). It avoids forcing updates.

**Future fully automatic option (not implemented):** only after identifying a
supported install service/CLI in the target firmware should a separate helper
perform a post-exit installation, with a verified signed release, preflight,
transactional backup and an independently testable rollback process. If that
firmware-level primitive is unavailable, retain the Archive Manager handoff.
