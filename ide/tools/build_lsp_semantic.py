"""Build original semantic checks plus guarded typed-index/type adapters.

The original checker is a reference foundation. Full service memo/IR adapters
must pass their independent upgrade audits before a current server is built.
"""
from build_lsp_ast import main


if __name__ == '__main__':
    main(semantic=True)
