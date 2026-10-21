#!/usr/bin/env python3
"""Measure a running browser and its descendants using Linux /proc."""

import argparse
import json
from pathlib import Path
from datetime import datetime, timezone


def snapshot(root_pid):
    parents = {}
    for entry in Path('/proc').iterdir():
        if not entry.name.isdigit():
            continue
        try:
            # comm may contain spaces and parentheses.
            fields = (entry / 'stat').read_text().rsplit(')', 1)[1].split()
            parents[int(entry.name)] = int(fields[1])
        except (OSError, ValueError, IndexError):
            continue
    if root_pid not in parents:
        raise ValueError(f'PID {root_pid} unavailable')
    selected = {root_pid}
    while True:
        children = {pid for pid, parent in parents.items() if parent in selected}
        expanded = selected | children
        if expanded == selected:
            break
        selected = expanded
    rows = []
    for pid in sorted(selected):
        row = {'pid': pid, 'rss_kib': None, 'pss_kib': None}
        try:
            row['name'] = (Path('/proc') / str(pid) / 'comm').read_text().strip()
            for line in (Path('/proc') / str(pid) / 'smaps_rollup').read_text().splitlines():
                key, _, value = line.partition(':')
                if key in ('Rss', 'Pss'):
                    row[key.lower() + '_kib'] = int(value.split()[0])
        except (OSError, ValueError) as error:
            row['error'] = str(error)
        rows.append(row)
    complete = all(row['pss_kib'] is not None for row in rows)
    return {
        'timestamp': datetime.now(timezone.utc).isoformat(),
        'root_pid': root_pid,
        'complete': complete,
        'rss_kib': sum(row['rss_kib'] or 0 for row in rows),
        'pss_kib': sum(row['pss_kib'] or 0 for row in rows),
        'processes': rows,
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pid', type=int, help='PID of the browser main process')
    args = parser.parse_args()
    try:
        print(json.dumps(snapshot(args.pid), indent=2))
    except ValueError as error:
        parser.exit(1, f'{error}\n')
