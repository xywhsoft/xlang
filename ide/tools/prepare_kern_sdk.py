"""Prepare SDK37 raw kerning sources in a fresh directory; no build/install."""
from pathlib import Path
import prepare_large_source_sdk as preparation

preparation.PATCH = Path(__file__).resolve().parents[1] / 'third_party/sdk_patches/large-source-experiment-v14'

if __name__ == '__main__':
    preparation.main()
