"""Build original native parser/editing syntax with independent parser references.

Does not build/publish a language server or admit current semantic/IR adapters.
"""
from build_lsp_ast import main


if __name__ == '__main__':
    main(syntax=True)
