---
type: Fix
title: Suite "Access is denied" from elevated leftover processes
description: A normal Suite failed Play because GameRunning() threw on zero-thread leftovers of an admin-run game; fixed by a snapshot thread-count check.
tags: [suite, processes, uac, bug, upstream]
timestamp: '2026-11-18T03:30:00Z'
---

# Symptom

The Play button reports **"Access is denied"** in a Suite opened normally after an earlier session was played
from an elevated (admin) Suite.

# Cause

* The patched game can leave processes that exited but stay listed. `tasklist` shows
  `GTAVXR_SourceBaseline.exe` at 32 K with status Unknown, and `Get-Process` shows **Threads=0**.
* They cannot be killed: an elevated `taskkill /F` also returns Access is denied, because the process is
  already exiting.
* A system-wide `handle64 -a` dump found no ordinary user-mode holder. Only a reboot clears them.
* Process Explorer kills of the Suite leave the same kind of leftover (GTAVXR.exe at 24 K).
* `PlaySession.GameRunning()` (`code/suite/src/Engine/PlaySession.cs:164`) calls `Process.HasExited` on every
  process named like the game. Opening an **elevated** process from a normal one throws Win32Exception, and
  Play aborts.
* The upstream source already notes residual single-thread processes (`CloseResidual`, PlaySession.cs:620).

# Fix (public source, commit 9c3eff4)

```csharp
static bool Alive(Process p){
    try{return !p.HasExited;}
    catch(System.ComponentModel.Win32Exception){
        try{return p.Threads.Count>0;}catch(Exception){return true;}
    }
}
```

The snapshot thread list needs no process handle. A live elevated game still has threads, so it still counts
as running.

# Deployment

The playable Suite is built by LVL from this source plus private components, so the fix takes effect once it is
included upstream. Workarounds until then: run the Suite as administrator, or reboot to clear the leftovers.
It is worth reporting upstream to LVL-VR.
