---
type: Runbook
title: GTAVXR install and session recovery
description: What a killed install leaves behind, the Suite records that gate Play, and when admin is needed.
tags: [suite, install, recovery, uac]
timestamp: '2026-11-18T03:30:00Z'
---

# Install facts

* Release: v1.0.2 `GTAVXR.exe`, SHA256 `B6E996BB…AFC013` (matches the release notes). There is no headless
  install command.
* Run it as admin for install. On this machine the folder picker did not appear, so it used the default Suite
  data folder `%LOCALAPPDATA%\GTAVXR\Suite` (about 256 MB at install). C: was 97% full; E:\GTAVXR\Suite would
  have been preferable.
* The install takes a long time with no feedback: it hashes and patches several GB of .rpf on E:. The window
  refuses to close during install, which looks like a hang. **Do not kill it.**

# Records that gate Play (EmbeddedInstall.RequireInstalledEmbedded)

| Record | Written | Missing → |
| --- | --- | --- |
| `game\apply.json` | during apply (journal) | — |
| `game\state.json` | apply finished | INSTALL_REQUIRED |
| `embedded\gtavxr-1-0-2.json` | install finished | "Install this version of GTAVXR before playing." |
| `sessions\<id>\session.json` phase LAUNCHED | game started | next Play shows "GTAVXR closed early…" and recovers |

Killing the first install left only apply.json. Re-running Install from the Suite completed it.

# Admin

Admin is needed only to install. Play should run in a normal Suite. However, if a session is played from an
elevated Suite, its leftover processes block a normal Suite with "Access is denied"; see
[elevated leftover processes](/fixes/suite-elevated-leftover-processes.md).

# Citations

[1] code/suite/src/Engine/EmbeddedInstall.cs:113-140, PlaySession.cs:340-347
