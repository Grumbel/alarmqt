# TODO – AlarmQt

## Tip / base

- **Work-line base:** `071e920` (Initial checkin)
- **Current tip:** HEAD of bundle `alarmqt-006.1-cli-table-ui-071e920.bundle`
- **Next bundle NNN:** 007

## Done in this line

- Initial QWidget app + continuity docs + REUSE/LICENSE
- Re-notify until ack; prune finished alarms
- Qt 6.11 QShortcut fix
- Anime SVG icon; desktop integration; short AppStream id `alarmqt`
- CLI add via running instance (SingleInstance read-on-accept race fix)
- Table UI (Remaining / When / Label) with row background highlights

## Open / nice-to-have

- [ ] Sound on trigger
- [ ] Daily / weekly repeat
- [ ] `--list` reply protocol from primary
- [ ] Unit tests for the time parser
- [ ] Sortable table columns

## Handoff

```bash
git pull /path/to/alarmqt-006.1-cli-table-ui-071e920.bundle HEAD
```

Requires base `071e920`.
