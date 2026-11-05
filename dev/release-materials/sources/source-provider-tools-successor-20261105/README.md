# Source/provider tool successor

After extracting xlang-development-20261105-88bd73dc-r1.zip, copy the tools/ and docs/ directories from this overlay into the extracted root. This preserves the exact later Git commit 1f19a1dece0077a41bf25a3f5a6ee2897c961e0e without recompressing the immutable base archive. Verify each file using source-manifest.json. The Core, ABI, production images, source archive and historical qualification bytes are unchanged. This layer fixes authoritative SDK alias refresh, preserves the production CPU checker and clarifies the implementation-capable header SDK role.
