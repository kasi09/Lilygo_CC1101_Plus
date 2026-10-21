"""Verify the distributable archive and future repository metadata preparation."""

import json
from zipfile import ZipFile

import pytest

from tools import package_hacs


def test_archive_contains_only_component_and_installation_docs(tmp_path):
    archive_path = package_hacs.build(tmp_path)
    with ZipFile(archive_path) as archive:
        assert archive.testzip() is None
        names = archive.namelist()
        assert "custom_components/lilygo_t_embed/manifest.json" in names
        assert "custom_components/lilygo_t_embed/translations/de.json" in names
        assert "custom_components/lilygo_t_embed/frontend/lilygo-panel.js" in names
        assert "custom_components/lilygo_t_embed/frontend.py" in names
        assert all(
            name.startswith("custom_components/lilygo_t_embed/")
            or name in ("INSTALLATION.md", "PROTOCOL.md", "LICENSE")
            for name in names
        )
        assert not any(
            part in " ".join(names) for part in ("config.local", ".pio", "captures", "__pycache__")
        )
    assert archive_path.with_suffix(".zip.sha256").is_file()


def test_real_repository_metadata_can_be_prepared_locally(tmp_path, monkeypatch):
    manifest = tmp_path / "manifest.json"
    manifest.write_text('{"domain":"lilygo_t_embed","version":"0.1.0"}', encoding="utf-8")
    monkeypatch.setattr(package_hacs, "COMPONENT", tmp_path)
    package_hacs.prepare_repository("https://github.com/example-owner/example-repo", "@example-owner")
    data = json.loads(manifest.read_text(encoding="utf-8"))
    assert data["issue_tracker"] == "https://github.com/example-owner/example-repo/issues"
    assert data["codeowners"] == ["@example-owner"]
    assert data["documentation"].endswith("/blob/main/docs/HACS.md")


@pytest.mark.parametrize(
    "url,owner",
    [
        ("https://other.example/repo", "@owner"),
        ("https://github.com/owner/repo;bad", "@owner"),
        ("https://github.com/owner/repo", "not-an-at-username"),
    ],
)
def test_invalid_publication_metadata_is_rejected(url, owner):
    with pytest.raises(ValueError):
        package_hacs.prepare_repository(url, owner)
