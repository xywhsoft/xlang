# 扩展库当前逐包矩阵（2026-09-27）

目录/登记/文档只说明存在性；最后一列只记录最终隔离候选上的 28 个选定入口，未执行不等于失败，也不等于未实现。完整证据见同名 final.inventory.json 和 extlib_file_oom_2026-09-27.final-extensions.results.json。

| 包 | 属性 | C/XL 源文件 | 测试登记 | README / 维护页 | 本批选定 JIT |
| --- | --- | ---: | --- | --- | --- |
| _bzip2 | internal | 7/0 | 未登记 | 无 / 无 | 未执行 |
| _expat | internal | 4/0 | 未登记 | 无 / 无 | 未执行 |
| _libzip | internal | 111/0 | 未登记 | 无 / 无 | 未执行 |
| _tomlc17 | internal | 1/0 | 未登记 | 无 / 无 | 未执行 |
| _xge_native | internal | 96/0 | 未登记 | 无 / 无 | 未执行 |
| _zlib | internal | 11/0 | 未登记 | 无 / 无 | 未执行 |
| acme | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| aho_corasick | public | 2/1 | featured | 有 / 有 | test/cases/extlib_aho_corasick.xl: pass |
| ansi | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| argon2 | public | 6/1 | featured | 有 / 有 | test/cases/extlib_argon2.xl: pass |
| barcode | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| blake2 | public | 3/1 | featured | 有 / 有 | test/cases/extlib_blake2.xl: pass |
| blake3 | public | 3/1 | featured | 有 / 有 | test/cases/extlib_blake3.xl: pass |
| bloom | public | 1/1 | featured | 有 / 有 | test/cases/extlib_bloom.xl: pass |
| brotli | public | 33/1 | featured | 有 / 有 | test/cases/extlib_brotli.xl: pass |
| bson | public | 1/1 | featured | 无 / 无 | 未执行 |
| bz2 | public | 3/1 | featured | 有 / 有 | test/cases/extlib_bz2.xl: pass<br>test/cases/extlib_bz2_progress.xl: pass |
| canbus | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| cbor | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| chardet | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| cli | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| clickhouse | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| clip | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| clipboard | public | 1/1 | platform | 无 / 无 | 未执行 |
| coap | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| color | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| com | public | 1/1 | platform | 无 / 无 | 未执行 |
| consistent_hash | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| crc32 | public | 2/1 | featured | 有 / 有 | test/cases/extlib_crc32.xl: pass |
| crontab | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| css | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| csv | public | 4/1 | phase1 | 无 / 无 | test/cases/extlib_csv.xl: fail |
| cuckoo | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| daemon | public | 1/1 | platform | 无 / 无 | 未执行 |
| dataframe | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| diff | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| dinov3 | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| dns | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| docker | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| docx | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| dotenv | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| duckdb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| embeddings | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| enet | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| epub | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| etcd | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| faiss | public | 1/1 | 未登记 | 无 / 无 | test/cases/extlib_faiss.xl: pass |
| ffmpeg | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| fix | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| flatbuffers | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| freetype | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| ftp | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| fuzzy | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| geohash | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| gettext | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| ggml | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| git | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| glad | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| gpio | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| graph | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| grpc | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| gzip | public | 2/1 | phase1 | 有 / 有 | test/cases/extlib_gzip.xl: pass |
| harfbuzz | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| hcl | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| hnswlib | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| html5 | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| hyperloglog | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| ical | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| icu | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| imagehash | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| imgui | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| influxdb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| ini | public | 4/1 | phase1 | 无 / 无 | 未执行 |
| jieba | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| json.patch | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| json5 | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| jwt | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| kubernetes | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| langdetect | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| ldap | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| leveldb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| libsndfile | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| libsvm | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| libtorch | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| libusb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| linalg | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| llama | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| lmdb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| lru | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| lz4 | public | 3/1 | featured | 无 / 无 | 未执行 |
| lzma | public | 7/1 | featured | 无 / 无 | 未执行 |
| md4c | public | 4/1 | phase1 | 无 / 无 | 未执行 |
| md5 | public | 2/1 | featured | 有 / 有 | test/cases/extlib_md5.xl: pass |
| mdns | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| memcached | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| minio | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| miniz | public | 2/1 | featured | 无 / 无 | 未执行 |
| mnn | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| modbus | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| money | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| mongodb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| mqtt | public | 5/2 | phase1 | 有 / 有 | test/cases/extlib_mqtt.xl: pass |
| msgpack | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| murmur3 | public | 3/1 | featured | 有 / 有 | test/cases/extlib_murmur3.xl: pass |
| mysql | public | 1/1 | 未登记 | 有 / 无 | test/cases/extlib_mysql.xl: pass |
| nanomsg | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| nats | public | 1/1 | 未登记 | 无 / 无 | test/cases/extlib_nats.xl: pass |
| ncnn | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| neo4j | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| netrc | public | 4/1 | phase1 | 无 / 无 | 未执行 |
| nsq | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| ntp | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| oauth2 | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| onnxruntime | public | 1/1 | phase1 | 无 / 无 | test/cases/extlib_onnxruntime.xl: fail |
| op | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| opencv | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| opentelemetry | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| openvino | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| opus | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| passkey | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| pcap | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| pdf | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| pdfium | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| pinyin | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| pkcs | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| pkg.math | internal | 0/1 | 未登记 | 无 / 无 | 未执行 |
| plist | public | 7/1 | phase1 | 无 / 无 | 未执行 |
| portaudio | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| postgresql | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| pptx | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| procps | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| progress | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| prometheus | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| protobuf | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| qr | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| quic | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| rabbitmq | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| rapidocr | public | 1/1 | phase1 | 有 / 无 | test/cases/extlib_rapidocr.xl: fail |
| ratatui | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| redis | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| roaring | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| rocksdb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| rss | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| rtmp | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| rtree | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| sciter | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| semver | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| sentencepiece | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| serial | public | 1/1 | platform | 有 / 无 | 未执行 |
| sha3 | public | 3/1 | featured | 有 / 有 | test/cases/extlib_sha3.xl: pass |
| sharedmem | public | 1/1 | platform | 无 / 无 | 未执行 |
| shellwords | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| singleinstance | public | 1/1 | platform | 无 / 无 | 未执行 |
| siphash | public | 3/1 | featured | 有 / 有 | test/cases/extlib_siphash.xl: pass |
| sitemap | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| skiplist | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| slugify | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| snappy | public | 3/1 | featured | 无 / 无 | 未执行 |
| snmp | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| socks | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| sox | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| sqlite | public | 2/1 | phase1 | 有 / 有 | test/cases/extlib_sqlite.xl: pass |
| srt | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| stablediffusion | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| statistics | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| stb_image | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| stb_image_resize2 | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| stb_image_write | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| stb_truetype | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| sysinfo | public | 1/1 | platform | 无 / 无 | 未执行 |
| tabulate | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| taglib | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| tar | public | 5/1 | phase1 | 无 / 无 | 未执行 |
| tdengine | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| tensorrt | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| terraform_state | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| tokenizers | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| toml | public | 4/1 | phase1 | 无 / 无 | 未执行 |
| totp | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| treesitter | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| trie | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| ui.auto | public | 0/1 | platform | 无 / 无 | 未执行 |
| uia | public | 1/1 | platform | 无 / 无 | 未执行 |
| units | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| upnp | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| vault | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| vcard | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| vectordb | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| volc.core | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| volc.phone | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| volc.sts | public | 0/1 | 未登记 | 无 / 无 | 未执行 |
| watch | public | 1/1 | platform | 无 / 无 | 未执行 |
| webrtc | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| webview | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| whisper | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| xbot | public | 0/1 | phase1 | 无 / 无 | release/extlibs/xbot/test/xbot_test.xl: pass |
| xbot.agent | public | 0/1 | phase1 | 无 / 无 | 未执行 |
| xfeat | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| xge | public | 0/12 | 未登记 | 无 / 无 | 未执行 |
| xge_shape_ex | public | 0/3 | 未登记 | 无 / 无 | 未执行 |
| xhttp | public | 8/1 | 未登记 | 有 / 无 | release/extlibs/xhttp/test/xhttp_url_test.xl: fail |
| xhttp.body | public | 4/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.body_async | public | 2/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.body_codec | public | 4/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.body_decode | public | 2/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.body_file | public | 2/1 | 未登记 | 有 / 无 | release/extlibs/xhttp.body_file/test/xhttp_body_file_test.xl: fail |
| xhttp.body_stream | public | 2/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.client | public | 21/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.form_data | public | 6/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.multipart | public | 11/1 | 未登记 | 有 / 无 | 未执行 |
| xhttp.protocol | public | 9/1 | 未登记 | 有 / 无 | 未执行 |
| xlsx | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| xmail | public | 1/1 | phase1 | 有 / 无 | release/extlibs/xmail/test/xmail_test.xl: fail |
| xml.dom | public | 4/1 | phase1 | 无 / 无 | 未执行 |
| xml.etree | public | 4/1 | phase1 | 无 / 无 | 未执行 |
| xml.sax | public | 3/1 | phase1 | 无 / 无 | 未执行 |
| xregex | public | 8/1 | phase1 | 有 / 无 | release/extlibs/xregex/test/xregex_test.xl: fail |
| xssh | public | 1/1 | phase1 | 有 / 无 | 未执行 |
| xui | public | 0/14 | 未登记 | 无 / 无 | 未执行 |
| xws | public | 12/1 | 未登记 | 有 / 无 | 未执行 |
| xxhash | public | 3/1 | featured | 有 / 有 | test/cases/extlib_xxhash.xl: pass |
| yaml | public | 1/1 | phase1 | 无 / 无 | 未执行 |
| zeromq | public | 1/1 | 未登记 | 无 / 无 | 未执行 |
| zip | public | 4/1 | phase1 | 无 / 无 | 未执行 |
| zstd | public | 3/1 | featured | 无 / 无 | 未执行 |
