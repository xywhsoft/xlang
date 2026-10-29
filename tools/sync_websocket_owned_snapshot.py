#!/usr/bin/env python3
"""Use the complete current XRT snapshot; selective ABI backports are retired."""
from sync_xrt_snapshot import main
if __name__ == "__main__":
    raise SystemExit(main())
