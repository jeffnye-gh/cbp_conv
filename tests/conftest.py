import pytest

@pytest.fixture(scope="session")
def cli_env(monkeypatch, tmp_path_factory):
    """Shared env for functional tests that may need temp dirs or env vars."""
    tmp_dir = tmp_path_factory.mktemp("cbp_conv")
    monkeypatch.setenv("CBP_CONV_TMPDIR", str(tmp_dir))
    return {"tmp": tmp_dir}

