"""The committed Colab notebook must exist, validate, and execute offline.

Executes the notebook's code cells in a fresh namespace with input() stubbed
to EOF (the nbconvert / no-terminal path) and asserts the demo chat runs.
"""

import builtins
import contextlib
import io
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
NB_PATH = REPO / "colab" / "claptrap_chatbot.ipynb"


def _src(cell):
    s = cell["source"]
    return "".join(s) if isinstance(s, list) else s


def _run(src, ns, buf):
    with contextlib.redirect_stdout(buf):
        exec(compile(src, "<cell>", "exec"), ns)


def test_notebook_is_valid_and_up_to_date():
    gen = subprocess.run(
        [sys.executable, str(REPO / "tools" / "make_colab_notebook.py")],
        capture_output=True, text=True,
    )
    assert gen.returncode == 0, gen.stderr

    nb = json.loads(NB_PATH.read_text())
    assert nb["nbformat"] == 4
    kinds = [c["cell_type"] for c in nb["cells"]]
    assert kinds == ["markdown", "code", "markdown", "code"]
    for cell in nb["cells"]:
        assert "id" in cell and cell["id"]


def test_notebook_executes_offline():
    nb = json.loads(NB_PATH.read_text())
    code_cells = [_src(c) for c in nb["cells"] if c["cell_type"] == "code"]
    assert len(code_cells) == 2

    ns = {}
    setup_out = io.StringIO()
    _run(code_cells[0], ns, setup_out)
    assert "CLAPTRAP brain loaded" in setup_out.getvalue()

    real_input = builtins.input

    def eof_input(*args, **kwargs):
        raise EOFError

    builtins.input = eof_input
    try:
        chat_out = io.StringIO()
        _run(code_cells[1], ns, chat_out)
    finally:
        builtins.input = real_input

    text = chat_out.getvalue()
    assert text.count("you> ") == 6          # demo lines echoed
    assert text.count("CLAPTRAP>") >= 6      # replies present
    assert "standby engaged" in text         # demo reached 'quit'
