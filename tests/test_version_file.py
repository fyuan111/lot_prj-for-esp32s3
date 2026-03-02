import re
from pathlib import Path

SEMVER_RE = re.compile(r'^\d+\.\d+\.\d+$')


def test_project_version_in_sdkconfig_is_semver():
    root = Path(__file__).resolve().parents[1]
    sdkconfig = root / 'sdkconfig'
    assert sdkconfig.exists(), 'sdkconfig missing'

    version = None
    for line in sdkconfig.read_text(encoding='utf-8').splitlines():
        if line.startswith('CONFIG_LOT_PROJECT_VERSION='):
            version = line.split('=', 1)[1].strip().strip('"')
            break

    assert version is not None, 'CONFIG_LOT_PROJECT_VERSION not found in sdkconfig'
    assert version, 'CONFIG_LOT_PROJECT_VERSION is empty'
    assert SEMVER_RE.match(version), f'invalid version format: {version}'


def test_build_version_txt_matches_sdkconfig_if_exists():
    root = Path(__file__).resolve().parents[1]
    build_ver = root / 'build' / 'version.txt'
    if not build_ver.exists():
        return

    version_txt = build_ver.read_text(encoding='utf-8').strip()
    assert version_txt, 'build/version.txt empty'
    assert SEMVER_RE.match(version_txt), f'invalid build/version.txt format: {version_txt}'
