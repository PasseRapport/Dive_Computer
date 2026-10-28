Ticket04 / final combined acceptance: durable freediving preferences.
PC/native simulator evidence only; no Nordic target or physical watch persistence.

Reproduce after python3 launch.py --build-only:
 python3 tests/check_apnea_persistence.py <build>/zephyr/zephyr.exe /tmp/apnea-restarts
 python3 tests/check_apnea_persistence_render.py /tmp/apnea-restarts
 python3 launch.py --apnea-settings-check --gallery /tmp/apnea-settings
 python3 tests/check_apnea_settings.py /tmp/apnea-settings

TDD red: harness-only native build completes public UI save journey, then the
separate-process orchestrator fails Applied freediving preferences were not saved.
red-before-adapter.txt preserves that failure. Adapter implementation makes the
six combinations green. Each save and load runs a genuinely separate OS process.
The saved UI journey applies both fields, records apneas, cancels pending edits,
and tries confirmation in each editor after immersion begins. Restart proves
only applied preferences survive. Read-only directory failure preserves the prior
durable file while the current process retains its chosen in-memory preference.
Six invalid version/range/trailing-data cases use Fresh/Temperature without
rewriting their original bytes. Seeded GF preference/log/checkpoint bytes remain
identical after every normal restart case; no temporary write files remain.

Combined acceptance in each loaded process:
 Fresh Main -> fresh Statistics -> active Main -> Statistics -> Main -> Water
 Type -> Leave -> new session. Visible depth12.3m and record1:00 agree across
 Main optional record fields and Statistics before surfacing, then re-entry
 clears records/recovery while retaining both preferences. Pixel checks confirm
 the restored Fresh/Marine selected row, Main/Statistics depth render equality,
 and unchanged selected bottom field when returning to Main.

Storage isolation:
 Launcher Settings check started with an inherited normal-state directory seeded
 with Marine/Session Apnea, GF and checkpoint files. It began at Fresh/Temperature
 using temporary storage, passed, and left all inherited files byte-identical.
 Direct native Settings check omitted DIVE_WATCH_STATE_DIR while XDG_STATE_HOME
 pointed at a seeded normal preference. It began at defaults, completed settings
 saves in memory, and left the normal file/directory unchanged. Explicit temporary
 DIVE_WATCH_STATE_DIR remains supported for restart orchestrators.

Focused regressions:
 Existing native Settings journey and x1/x2 pixel checks PASS.
 CTest disk_checkpoint_process_continuity_and_faults,
 durable_preferences_logs_separate_processes, recovery_pair_lifecycle_log_integration
 PASS (3/3) in /tmp/apnee-bench-regression built from unchanged integrated storage
 sources. Native adapter compiles -Wall -Wextra -Werror.
 Earlier Main/Statistics/scuba checks were already verified on prerequisite
 commits; this ticket's combined public journey additionally checks their record
 agreement and resets. No broad duplicate suite was claimed.
 Known unrelated baseline pixel failures remain as documented in prior tickets:
 scuba orange DECO caption expectation and watch-face retention image comparison.
 No corresponding assertion has been weakened.

Storage is apnea-preferences.txt schema1 only: water0/1, field0/1/2. Atomic flushed
 replacement and directory flushing retain prior committed content on failure.
 Session state, exposure/GF, dive logs and checkpoints are never serialized by this
 adapter. Startup loads before the first freediving presentation is constructed.
