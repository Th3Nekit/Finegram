#!/usr/bin/env python3
"""Materialize public pinned gitlinks without adding Git history to this tree."""
import hashlib, io, json, pathlib, shutil, subprocess, tarfile, tempfile
root = pathlib.Path(__file__).resolve().parent
manifest = json.loads((root / 'SOURCE-MANIFEST.json').read_text(encoding='utf-8'))
for dep in manifest['dependencies']:
    if dep['bundled']:
        continue
    target = root / dep['path']
    target.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='finegram-fetch-') as temp:
        repo = pathlib.Path(temp) / 'repo'
        subprocess.run(['git', 'init', '-q', str(repo)], check=True)
        subprocess.run(['git', '-C', str(repo), 'fetch', '--depth=1', dep['url'], dep['commit']], check=True)
        head = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'FETCH_HEAD']).decode().strip()
        if head != dep['commit']:
            raise RuntimeError('Dependency commit mismatch')
        data = subprocess.check_output(['git', '-C', str(repo), 'archive', head])
        with tarfile.open(fileobj=io.BytesIO(data)) as archive:
            for member in archive:
                name = pathlib.PurePosixPath(member.name)
                if name.is_absolute() or '..' in name.parts or member.issym() or member.islnk():
                    raise RuntimeError('Unsafe dependency member')
                if not member.isfile() or member.name in dep['deleted_overlay_paths']:
                    continue
                dest = target.joinpath(*name.parts)
                if dest.exists():
                    continue  # Snapshot files and reviewed local overlays take precedence.
                dest.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as src, dest.open('xb') as out:
                    shutil.copyfileobj(src, out)
                dest.chmod(member.mode)
for item in manifest['files']:
    with (root / item['path']).open('rb') as stream:
        if hashlib.file_digest(stream, 'sha256').hexdigest() != item['sha256']:
            raise RuntimeError('Snapshot or overlay hash mismatch: ' + item['path'])
print('Pinned dependencies materialized; snapshot hashes verified.')
