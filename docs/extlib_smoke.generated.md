# Extension Library Catalog Smoke

Registered focused and package-declared JIT cases only; not C/AOT, examples, complete API coverage or release admission
Evidence valid for recorded inputs: True. A nonzero result is a reproduction requiring diagnosis, not automatically a package bug.

| Status | Count |
| --- | ---: |
| fail | 18 |
| pass | 103 |

| Case | Status | Exit | Milliseconds | Diagnostic |
| --- | --- | ---: | ---: | --- |
| test/cases/extlib_aho_corasick.xl | pass | 0 | 3577 |  aho_corasick: all tests passed  |
| test/cases/extlib_ansi.xl | pass | 0 | 213 |   |
| test/cases/extlib_argon2.xl | fail | 1 | 120 | D:\GIT\x-lang\test\cases\extlib_argon2.xl:93:35: [ir] typed XLIR operator operand has no canonical physical-to-execution path   |
| test/cases/extlib_barcode.xl | pass | 0 | 343 |   |
| test/cases/extlib_blake2.xl | pass | 0 | 1657 |   |
| test/cases/extlib_blake2_boundaries.xl | pass | 0 | 2685 |   |
| test/cases/extlib_blake2_progress.xl | pass | 0 | 1399 |   |
| test/cases/extlib_blake3.xl | pass | 0 | 915 |   |
| test/cases/extlib_blake3_boundaries.xl | pass | 0 | 1734 |   |
| test/cases/extlib_blake3_progress.xl | pass | 0 | 847 |   |
| test/cases/extlib_bloom.xl | pass | 0 | 2767 |  bloom: all tests passed  |
| test/cases/extlib_brotli.xl | pass | 0 | 2873 |   |
| test/cases/extlib_brotli_progress.xl | pass | 0 | 2902 |   |
| test/cases/extlib_bson.xl | pass | 0 | 309 |   |
| test/cases/extlib_bz2.xl | fail | 1 | 103 | D:\GIT\x-lang\test\cases\extlib_bz2.xl:68:39: [ir] typed XLIR operator operand has no canonical physical-to-execution path   |
| test/cases/extlib_bz2_progress.xl | pass | 0 | 1865 |   |
| test/cases/extlib_cbor.xl | pass | 0 | 286 |   |
| test/cases/extlib_chardet.xl | pass | 0 | 193 |   |
| test/cases/extlib_cli.xl | fail | 1 | 354 | tcc: tcc: error: unresolved reference to 'xl6_xrt_os_args'   |
| test/cases/extlib_color.xl | pass | 0 | 448 |   |
| test/cases/extlib_consistent_hash.xl | pass | 0 | 209 |   |
| test/cases/extlib_crc32.xl | pass | 0 | 450 |   |
| test/cases/extlib_crc32_progress.xl | pass | 0 | 494 |   |
| test/cases/extlib_crontab.xl | pass | 0 | 124 |   |
| test/cases/extlib_css.xl | pass | 0 | 146 |   |
| test/cases/extlib_csv.xl | pass | 0 | 994 |   |
| test/cases/extlib_cuckoo.xl | pass | 0 | 187 |   |
| test/cases/extlib_diff.xl | pass | 0 | 132 |   |
| test/cases/extlib_dinov3.xl | pass | 0 | 3280 |   |
| test/cases/extlib_dns.xl | fail | 1 | 141 | uncaught xlang error: ValueError: invalid numeric IP address uncaught xlang error: invalid numeric IP address   |
| test/cases/extlib_dotenv.xl | pass | 0 | 568 |   |
| test/cases/extlib_epub.xl | pass | 0 | 311 |   |
| test/cases/extlib_fix.xl | pass | 0 | 142 |   |
| test/cases/extlib_fuzzy.xl | pass | 0 | 131 |   |
| test/cases/extlib_geohash.xl | pass | 0 | 172 |   |
| test/cases/extlib_gettext.xl | pass | 0 | 301 |   |
| test/cases/extlib_graph.xl | pass | 0 | 756 |   |
| test/cases/extlib_gzip.xl | fail | 1 | 87 | D:\GIT\x-lang\test\cases\extlib_gzip.xl:49:77: [ir] typed XLIR operator operand has no canonical physical-to-execution path   |
| test/cases/extlib_hash_progress.xl | pass | 0 | 8692 |   |
| test/cases/extlib_hash_progress_cancel.xl | pass | 0 | 8249 |   |
| test/cases/extlib_hcl.xl | pass | 0 | 166 |   |
| test/cases/extlib_hyperloglog.xl | pass | 0 | 200 |   |
| test/cases/extlib_ical.xl | pass | 0 | 350 |   |
| test/cases/extlib_imagehash.xl | pass | 0 | 302 |  imagehash case: PASS  |
| test/cases/extlib_ini.xl | pass | 0 | 813 |   |
| test/cases/extlib_json_patch.xl | pass | 0 | 1406 |   |
| test/cases/extlib_json5.xl | pass | 0 | 185 |   |
| test/cases/extlib_jwt.xl | fail | 1 | 132 | tcc: In file included from __xlvfs__/csrc_0.c:91: D:/GIT/x-lang/release/extlibs/jwt/jwt_native.c:192: error: incompatible types for redefinition of 'xjwt_verify'   |
| test/cases/extlib_langdetect.xl | pass | 0 | 158 |   |
| test/cases/extlib_linalg.xl | pass | 0 | 159 |   |
| test/cases/extlib_lru.xl | pass | 0 | 269 |   |
| test/cases/extlib_lz4.xl | fail | 1 | 110 | D:\GIT\x-lang\test\cases\extlib_lz4.xl:92:41: [ir] typed XLIR operator operand has no canonical physical-to-execution path   |
| test/cases/extlib_lz4_progress.xl | pass | 0 | 1402 |   |
| test/cases/extlib_lzma.xl | fail | 1 | 117 | D:\GIT\x-lang\test\cases\extlib_lzma.xl:92:41: [ir] typed XLIR operator operand has no canonical physical-to-execution path   |
| test/cases/extlib_md4c.xl | pass | 0 | 228 |  md4c case ok  |
| test/cases/extlib_md5.xl | pass | 0 | 535 |   |
| test/cases/extlib_md5_progress.xl | pass | 0 | 515 |   |
| test/cases/extlib_memcached.xl | pass | 0 | 586 |   |
| test/cases/extlib_miniz.xl | pass | 0 | 961 |   |
| test/cases/extlib_modbus.xl | fail | 1 | 522 | tcc: In file included from __xlvfs__/csrc_0.c:98: D:/GIT/x-lang/release/extlibs/modbus/modbus_native.c:71: error: incompatible types for redefinition of 'xm_build_read'   |
| test/cases/extlib_money.xl | pass | 0 | 815 |  share 1 = $22.35 share 2 = $22.34 share 3 = $22.34 share 4 = $22.34  |
| test/cases/extlib_mqtt.xl | pass | 0 | 997 |   |
| test/cases/extlib_msgpack.xl | pass | 0 | 266 |   |
| test/cases/extlib_murmur3.xl | pass | 0 | 774 |   |
| test/cases/extlib_murmur3_progress.xl | pass | 0 | 640 |   |
| test/cases/extlib_netrc.xl | pass | 0 | 499 |   |
| test/cases/extlib_ntp.xl | pass | 0 | 131 |   |
| test/cases/extlib_oauth2.xl | fail | 1 | 164 | tcc: In file included from __xlvfs__/csrc_0.c:95: D:/GIT/x-lang/release/extlibs/oauth2/oauth2_native.c:285: error: incompatible types for redefinition of 'xoa_verify_challenge'   |
| test/cases/extlib_onnxruntime.xl | pass | 0 | 651 |   |
| test/cases/extlib_opencv.xl | pass | 0 | 489 |   |
| test/cases/extlib_pinyin.xl | pass | 0 | 117 |   |
| test/cases/extlib_pkcs.xl | pass | 0 | 260 |   |
| test/cases/extlib_plist.xl | pass | 0 | 1876 |   |
| test/cases/extlib_progress.xl | pass | 0 | 177 |   |
| test/cases/extlib_qr.xl | pass | 0 | 632 |   |
| test/cases/extlib_rapidocr.xl | pass | 0 | 21948 |   |
| test/cases/extlib_redis.xl | pass | 0 | 779 |   |
| test/cases/extlib_roaring.xl | pass | 0 | 159 |   |
| test/cases/extlib_rss.xl | pass | 0 | 1952 |   |
| test/cases/extlib_rtree.xl | pass | 0 | 155 |   |
| test/cases/extlib_semver.xl | pass | 0 | 324 |   |
| test/cases/extlib_sha3.xl | pass | 0 | 1813 |   |
| test/cases/extlib_sha3_progress.xl | pass | 0 | 915 |   |
| test/cases/extlib_shellwords.xl | pass | 0 | 249 |   |
| test/cases/extlib_siphash128_core.xl | pass | 0 | 372 |   |
| test/cases/extlib_sitemap.xl | pass | 0 | 730 |   |
| test/cases/extlib_skiplist.xl | pass | 0 | 304 |   |
| test/cases/extlib_slugify.xl | pass | 0 | 130 |   |
| test/cases/extlib_snappy.xl | pass | 0 | 1044 |   |
| test/cases/extlib_socks.xl | fail | 1 | 155 | tcc: In file included from __xlvfs__/csrc_0.c:95: D:/GIT/x-lang/release/extlibs/socks/socks_native.c:104: error: incompatible types for redefinition of 'xs_connect_domain'   |
| test/cases/extlib_sqlite.xl | fail | 1 | 397 | D:\GIT\x-lang\test\cases\extlib_sqlite.xl:550:28: [checked] XLIR C emitter runtime operation ABI is unresolved   |
| test/cases/extlib_srt.xl | pass | 0 | 348 |   |
| test/cases/extlib_statistics.xl | pass | 0 | 443 |   |
| test/cases/extlib_stb_image.xl | pass | 0 | 229 |   |
| test/cases/extlib_stb_image_resize2.xl | pass | 0 | 335 |   |
| test/cases/extlib_stb_image_write.xl | pass | 0 | 210 |   |
| test/cases/extlib_stb_truetype.xl | pass | 0 | 211 |   |
| test/cases/extlib_tabulate.xl | pass | 0 | 253 |   |
| test/cases/extlib_tar_p2.xl | pass | 0 | 6602 |  P2 PAX/GNU tests passed! |
| test/cases/extlib_terraform_state.xl | pass | 0 | 171 |   |
| test/cases/extlib_toml.xl | pass | 0 | 607 |   |
| test/cases/extlib_totp.xl | pass | 0 | 149 |   |
| test/cases/extlib_trie.xl | pass | 0 | 229 |   |
| test/cases/extlib_units.xl | pass | 0 | 174 |   |
| test/cases/extlib_vcard.xl | pass | 0 | 339 |   |
| test/cases/extlib_volc_core.xl | fail | 1 | 201 | D:\GIT\x-lang\release\extlibs\xhttp.multipart\src\xlang\xhttp_multipart.xl:352:23: [sem] unknown class/union field   |
| test/cases/extlib_xbot.xl | pass | 0 | 89 |   |
| test/cases/extlib_xbot_agent.xl | fail | 1 | 47 | D:\GIT\x-lang\release\extlibs\xbot.agent\src\agent.xl:405:29: [sem] 初始化表达式类型和声明类型不匹配: target int, value uint64   |
| test/cases/extlib_xfeat.xl | pass | 0 | 3569 |   |
| test/cases/extlib_xmail.xl | fail | 1 | 5499 | tcc: tcc: error: unresolved reference to 'xl6ExtlibTlsLeaseRelease'   |
| test/cases/extlib_xml_dom.xl | pass | 0 | 1161 |   |
| test/cases/extlib_xml_etree.xl | pass | 0 | 1079 |   |
| test/cases/extlib_xml_sax.xl | pass | 0 | 695 |   |
| test/cases/extlib_xregex.xl | pass | 0 | 1302 |   |
| test/cases/extlib_xssh.xl | pass | 0 | 952 |   |
| test/cases/extlib_xxhash.xl | pass | 0 | 690 |   |
| test/cases/extlib_xxhash_boundaries.xl | pass | 0 | 591 |   |
| test/cases/extlib_xxhash_progress.xl | pass | 0 | 598 |   |
| test/cases/extlib_yaml.xl | fail | 1 | 117 | tcc: In file included from __xlvfs__/csrc_0.c:105: D:/GIT/x-lang/release/extlibs/yaml/yaml_native.c:71: error: incompatible types for redefinition of 'xyaml_get_bool'   |
| test/cases/extlib_zip.xl | fail | 1 | 5098 | uncaught xlang error: IOError: unsafe ZIP entry path: ../escape.txt uncaught xlang error: unsafe ZIP entry path: ../escape.txt   |
| test/cases/extlib_zstd.xl | fail | 1 | 114 | D:\GIT\x-lang\test\cases\extlib_zstd.xl:92:41: [ir] typed XLIR operator operand has no canonical physical-to-execution path   |
