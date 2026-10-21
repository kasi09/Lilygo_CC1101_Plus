"""Build a local install ZIP; optionally prepare real repository metadata.

No network access or publication. The archive contains custom_components/ so
it can be extracted into /config. It never includes credentials or captures.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
COMPONENT = ROOT / "custom_components" / "lilygo_t_embed"


def build(output: Path) -> Path:
    manifest = json.loads((COMPONENT / "manifest.json").read_text(encoding="utf-8"))
    output.mkdir(parents=True, exist_ok=True)
    target = output / f"lilygo_t_embed-{manifest['version']}.zip"
    files = [
        p
        for p in COMPONENT.rglob("*")
        if p.is_file() and p.suffix in (".py", ".json", ".yaml", ".js", ".css") and "__pycache__" not in p.parts
    ]
    with ZipFile(target, "w", ZIP_DEFLATED) as archive:
        for source in sorted(files):
            archive.write(source, source.relative_to(ROOT).as_posix())
        archive.write(ROOT / "docs" / "HACS.md", "INSTALLATION.md")
        archive.write(ROOT / "docs" / "PROTOCOL.md", "PROTOCOL.md")
        archive.write(ROOT / "LICENSE", "LICENSE")
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix(".zip.sha256").write_text(f"{digest}  {target.name}\n", encoding="ascii")
    print(f"{target}\nSHA256: {digest}\nComponent files: {len(files)}")
    return target


def prepare_repository(repository: str, owner: str) -> None:
    if not re.fullmatch(r"https://github\.com/[A-Za-z0-9-]+/[A-Za-z0-9_.-]+", repository):
        raise ValueError("Expected https://github.com/OWNER/REPOSITORY without trailing slash")
    if not re.fullmatch(r"@[A-Za-z0-9-]+", owner):
        raise ValueError("Expected @GitHubUsername")
    path = COMPONENT / "manifest.json"
    manifest = json.loads(path.read_text(encoding="utf-8"))
    manifest.update(
        documentation=f"{repository}/blob/main/docs/HACS.md",
        issue_tracker=f"{repository}/issues",
        codeowners=[owner],
    )
    path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--repository", help="Prepare metadata locally for this future GitHub repository")
    parser.add_argument("--owner", help="Real GitHub code owner, e.g. @username")
    args = parser.parse_args()
    if bool(args.repository) != bool(args.owner):
        parser.error("--repository and --owner must be supplied together")
    if args.repository:
        prepare_repository(args.repository, args.owner)
    build(args.output)


if __name__ == "__main__":
    main()
