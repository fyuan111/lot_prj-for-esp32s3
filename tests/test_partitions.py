from pathlib import Path


def _parse_size_to_bytes(size_text: str) -> int:
    s = size_text.strip().upper()
    if s.endswith('M'):
        return int(float(s[:-1]) * 1024 * 1024)
    if s.endswith('K'):
        return int(float(s[:-1]) * 1024)
    if s.startswith('0X'):
        return int(s, 16)
    return int(s)


def _read_partitions(path: Path):
    rows = []
    for line in path.read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = [p.strip() for p in line.split(',')]
        if len(parts) < 5:
            continue
        rows.append({
            'name': parts[0],
            'type': parts[1],
            'subtype': parts[2],
            'offset': parts[3],
            'size': parts[4],
        })
    return rows


def test_partition_required_entries_exist():
    root = Path(__file__).resolve().parents[1]
    rows = _read_partitions(root / 'partitions.csv')
    names = {r['name'] for r in rows}

    required = {'otadata', 'factory', 'ota_0', 'ota_1'}
    missing = required - names
    assert not missing, f'missing required partitions: {sorted(missing)}'


def test_ota_slots_size_match_and_are_reasonable():
    root = Path(__file__).resolve().parents[1]
    rows = _read_partitions(root / 'partitions.csv')
    table = {r['name']: r for r in rows}

    ota0_size = _parse_size_to_bytes(table['ota_0']['size'])
    ota1_size = _parse_size_to_bytes(table['ota_1']['size'])

    assert ota0_size == ota1_size, 'ota_0 and ota_1 size mismatch'
    assert ota0_size >= 1024 * 1024, 'OTA slot too small (< 1MB)'
