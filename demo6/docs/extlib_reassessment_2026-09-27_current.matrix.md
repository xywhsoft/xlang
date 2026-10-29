# Extension Library Current Inventory

Recorded: 2026-09-27T21:31:58.0274293+08:00. Manifest facts and selected Windows JIT observations only; untested is not failed, passed is not release admission.
Inputs unchanged during this run: True.

xregex documentation was completed after the test snapshot; its site-source cell below is updated. Runtime observations retain the original fixed-build batch. See [delivery follow-up](扩展库xregex交付收口_2026-09-27.md) for the newer focused tests.

| Package | C / XL sources | Registered | Package tests | Case / example | README / site source | Release JIT selected |
| --- | ---: | --- | ---: | --- | --- | --- |
| _bzip2 | 7 / 0 |  | 0 | False / False | False / False | not selected |
| _expat | 4 / 0 |  | 0 | False / False | False / False | not selected |
| _libzip | 111 / 0 |  | 0 | False / False | False / False | not selected |
| _tomlc17 | 1 / 0 |  | 0 | False / False | False / False | not selected |
| _xge_native | 96 / 0 |  | 0 | False / False | False / False | not selected |
| _zlib | 11 / 0 |  | 0 | False / False | False / False | not selected |
| acme | 1 / 1 |  | 0 | True / True | False / False | not selected |
| aho_corasick | 2 / 1 | featured | 1 | True / True | True / True | aho_corasick=pass |
| ansi | 1 / 1 | phase1 | 0 | True / True | False / False | ansi=pass |
| argon2 | 6 / 1 | featured | 0 | True / True | True / True | argon2=pass |
| barcode | 1 / 1 | phase1 | 0 | True / True | False / False | barcode=fail |
| blake2 | 3 / 1 | featured | 0 | True / True | True / True | blake2=pass |
| blake3 | 3 / 1 | featured | 0 | True / True | True / True | blake3=pass |
| bloom | 1 / 1 | featured | 0 | True / True | True / True | bloom=pass |
| brotli | 33 / 1 | featured | 0 | True / True | True / True | brotli=pass |
| bson | 1 / 1 | featured | 0 | True / True | False / False | not selected |
| bz2 | 3 / 1 | featured | 0 | True / True | True / True | bz2=pass |
| canbus | 1 / 1 |  | 0 | True / True | False / False | not selected |
| cbor | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| chardet | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| cli | 1 / 1 | phase1 | 0 | True / True | False / False | cli=fail |
| clickhouse | 1 / 1 |  | 0 | True / True | False / False | not selected |
| clip | 1 / 1 |  | 0 | True / True | False / False | not selected |
| clipboard | 1 / 1 | platform | 0 | False / True | False / False | not selected |
| coap | 1 / 1 |  | 0 | True / True | False / False | not selected |
| color | 1 / 1 | phase1 | 0 | True / True | False / False | color=fail |
| com | 1 / 1 | platform | 0 | True / True | False / False | not selected |
| consistent_hash | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| crc32 | 2 / 1 | featured | 0 | True / True | True / True | crc32=pass |
| crontab | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| css | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| csv | 4 / 1 | phase1 | 0 | True / True | False / False | csv=fail |
| cuckoo | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| daemon | 1 / 1 | platform | 0 | True / True | False / False | not selected |
| dataframe | 1 / 1 |  | 0 | True / True | False / False | not selected |
| diff | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| dinov3 | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| dns | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| docker | 1 / 1 |  | 0 | True / True | False / False | not selected |
| docx | 1 / 1 |  | 0 | True / True | False / False | not selected |
| dotenv | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| duckdb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| embeddings | 1 / 1 |  | 0 | True / True | False / False | not selected |
| enet | 1 / 1 |  | 0 | True / True | False / False | not selected |
| epub | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| etcd | 1 / 1 |  | 0 | True / True | False / False | not selected |
| faiss | 1 / 1 |  | 0 | True / True | False / False | faiss=pass |
| ffmpeg | 1 / 1 |  | 0 | True / True | False / False | not selected |
| fix | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| flatbuffers | 1 / 1 |  | 0 | True / True | False / False | not selected |
| freetype | 1 / 1 |  | 0 | True / True | False / False | not selected |
| ftp | 1 / 1 |  | 0 | True / True | False / False | not selected |
| fuzzy | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| geohash | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| gettext | 1 / 1 | phase1 | 0 | True / True | False / False | gettext=pass |
| ggml | 1 / 1 |  | 0 | True / True | False / False | not selected |
| git | 1 / 1 |  | 0 | True / True | False / False | not selected |
| glad | 1 / 1 |  | 0 | True / True | False / False | not selected |
| gpio | 1 / 1 |  | 0 | True / True | False / False | not selected |
| graph | 1 / 1 | phase1 | 0 | True / True | False / False | graph=fail |
| grpc | 1 / 1 |  | 0 | True / True | False / False | not selected |
| gzip | 2 / 1 | phase1 | 0 | True / True | True / True | gzip=pass |
| harfbuzz | 1 / 1 |  | 0 | True / True | False / False | not selected |
| hcl | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| hnswlib | 1 / 1 |  | 0 | True / True | False / False | not selected |
| html5 | 1 / 1 |  | 0 | True / True | False / False | not selected |
| hyperloglog | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| ical | 1 / 1 | phase1 | 0 | True / True | False / False | ical=pass |
| icu | 1 / 1 |  | 0 | True / True | False / False | not selected |
| imagehash | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| imgui | 1 / 1 |  | 0 | True / True | False / False | not selected |
| influxdb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| ini | 4 / 1 | phase1 | 0 | True / True | False / False | not selected |
| jieba | 1 / 1 |  | 0 | True / True | False / False | not selected |
| json.patch | 1 / 1 | phase1 | 0 | True / True | False / False | json.patch=fail |
| json5 | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| jwt | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| kubernetes | 1 / 1 |  | 0 | True / True | False / False | not selected |
| langdetect | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| ldap | 1 / 1 |  | 0 | True / True | False / False | not selected |
| leveldb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| libsndfile | 1 / 1 |  | 0 | True / True | False / False | not selected |
| libsvm | 1 / 1 |  | 0 | True / True | False / False | not selected |
| libtorch | 1 / 1 |  | 0 | True / True | False / False | not selected |
| libusb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| linalg | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| llama | 1 / 1 |  | 0 | True / True | False / False | not selected |
| lmdb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| lru | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| lz4 | 3 / 1 | featured | 0 | True / True | False / False | not selected |
| lzma | 7 / 1 | featured | 0 | True / True | False / False | not selected |
| md4c | 4 / 1 | phase1 | 0 | True / True | False / False | not selected |
| md5 | 2 / 1 | featured | 0 | True / True | True / True | md5=pass |
| mdns | 1 / 1 |  | 0 | True / True | False / False | not selected |
| memcached | 1 / 1 | phase1 | 0 | True / True | False / False | memcached=pass |
| minio | 1 / 1 |  | 0 | True / True | False / False | not selected |
| miniz | 2 / 1 | featured | 0 | True / True | False / False | not selected |
| mnn | 1 / 1 |  | 0 | True / True | False / False | not selected |
| modbus | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| money | 1 / 1 | phase1 | 0 | True / True | False / False | money=pass |
| mongodb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| mqtt | 5 / 2 | phase1 | 1 | True / True | True / True | mqtt=pass |
| msgpack | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| murmur3 | 3 / 1 | featured | 0 | True / True | True / True | murmur3=pass |
| mysql | 1 / 1 |  | 0 | True / True | True / False | mysql=pass |
| nanomsg | 1 / 1 |  | 0 | True / True | False / False | not selected |
| nats | 1 / 1 |  | 0 | True / True | False / False | nats=pass |
| ncnn | 1 / 1 |  | 0 | True / True | False / False | not selected |
| neo4j | 1 / 1 |  | 0 | True / True | False / False | not selected |
| netrc | 4 / 1 | phase1 | 0 | True / True | False / False | not selected |
| nsq | 1 / 1 |  | 0 | True / True | False / False | not selected |
| ntp | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| oauth2 | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| onnxruntime | 1 / 1 | phase1 | 0 | True / True | False / False | onnxruntime=fail |
| op | 1 / 1 |  | 0 | True / True | False / False | not selected |
| opencv | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| opentelemetry | 1 / 1 |  | 0 | True / True | False / False | not selected |
| openvino | 1 / 1 |  | 0 | True / True | False / False | not selected |
| opus | 1 / 1 |  | 0 | True / True | False / False | not selected |
| passkey | 1 / 1 |  | 0 | True / True | False / False | not selected |
| pcap | 1 / 1 |  | 0 | True / True | False / False | not selected |
| pdf | 1 / 1 |  | 0 | True / True | False / False | not selected |
| pdfium | 1 / 1 |  | 0 | True / True | False / False | not selected |
| pinyin | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| pkcs | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| pkg.math | 0 / 1 |  | 0 | True / True | False / False | not selected |
| plist | 7 / 1 | phase1 | 0 | True / True | False / False | not selected |
| portaudio | 1 / 1 |  | 0 | True / True | False / False | not selected |
| postgresql | 1 / 1 |  | 0 | True / True | False / False | not selected |
| pptx | 1 / 1 |  | 0 | True / True | False / False | not selected |
| procps | 1 / 1 |  | 0 | True / True | False / False | not selected |
| progress | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| prometheus | 1 / 1 |  | 0 | True / True | False / False | not selected |
| protobuf | 1 / 1 |  | 0 | True / True | False / False | not selected |
| qr | 1 / 1 | phase1 | 0 | True / True | False / False | qr=pass |
| quic | 1 / 1 |  | 0 | True / True | False / False | not selected |
| rabbitmq | 1 / 1 |  | 0 | True / True | False / False | not selected |
| rapidocr | 1 / 1 | phase1 | 0 | True / True | True / False | rapidocr=fail |
| ratatui | 1 / 1 |  | 0 | True / True | False / False | not selected |
| redis | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| roaring | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| rocksdb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| rss | 1 / 1 | phase1 | 0 | True / True | False / False | rss=pass |
| rtmp | 1 / 1 |  | 0 | True / True | False / False | not selected |
| rtree | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| sciter | 1 / 1 |  | 0 | True / True | False / False | not selected |
| semver | 1 / 1 | phase1 | 0 | True / True | False / False | semver=fail |
| sentencepiece | 1 / 1 |  | 0 | True / True | False / False | not selected |
| serial | 1 / 1 | platform | 0 | True / True | True / False | not selected |
| sha3 | 3 / 1 | featured | 0 | True / True | True / True | sha3=pass |
| sharedmem | 1 / 1 | platform | 0 | True / True | False / False | not selected |
| shellwords | 1 / 1 | phase1 | 0 | True / True | False / False | shellwords=pass |
| singleinstance | 1 / 1 | platform | 0 | True / True | False / False | not selected |
| siphash | 3 / 1 | featured | 0 | True / True | True / True | siphash=pass |
| sitemap | 1 / 1 | phase1 | 0 | True / True | False / False | sitemap=fail |
| skiplist | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| slugify | 1 / 1 | phase1 | 0 | True / True | False / False | slugify=fail |
| snappy | 3 / 1 | featured | 0 | True / True | False / False | not selected |
| snmp | 1 / 1 |  | 0 | True / True | False / False | not selected |
| socks | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| sox | 1 / 1 |  | 0 | True / True | False / False | not selected |
| sqlite | 2 / 1 | phase1 | 2 | True / True | True / True | sqlite=pass |
| srt | 1 / 1 | phase1 | 0 | True / True | False / False | srt=fail |
| stablediffusion | 1 / 1 |  | 0 | True / True | False / False | not selected |
| statistics | 1 / 1 | phase1 | 0 | True / True | False / False | statistics=fail |
| stb_image | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| stb_image_resize2 | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| stb_image_write | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| stb_truetype | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| sysinfo | 1 / 1 | platform | 0 | True / True | False / False | not selected |
| tabulate | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| taglib | 1 / 1 |  | 0 | True / True | False / False | not selected |
| tar | 5 / 1 | phase1 | 0 | True / True | False / False | not selected |
| tdengine | 1 / 1 |  | 0 | True / True | False / False | not selected |
| tensorrt | 1 / 1 |  | 0 | True / True | False / False | not selected |
| terraform_state | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| tokenizers | 1 / 1 |  | 0 | True / True | False / False | not selected |
| toml | 4 / 1 | phase1 | 0 | True / True | False / False | not selected |
| totp | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| treesitter | 1 / 1 |  | 0 | True / True | False / False | not selected |
| trie | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| ui.auto | 0 / 1 | platform | 0 | True / True | False / False | not selected |
| uia | 1 / 1 | platform | 0 | False / True | False / False | not selected |
| units | 1 / 1 | phase1 | 0 | True / True | False / False | units=fail |
| upnp | 1 / 1 |  | 0 | True / True | False / False | not selected |
| vault | 1 / 1 |  | 0 | True / True | False / False | not selected |
| vcard | 1 / 1 | phase1 | 0 | True / True | False / False | vcard=pass |
| vectordb | 1 / 1 |  | 0 | True / True | False / False | not selected |
| volc.core | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| volc.phone | 1 / 1 |  | 0 | False / True | False / False | not selected |
| volc.sts | 0 / 1 |  | 0 | False / True | False / False | not selected |
| watch | 1 / 1 | platform | 0 | True / True | False / False | not selected |
| webrtc | 1 / 1 |  | 0 | True / True | False / False | not selected |
| webview | 1 / 1 |  | 0 | True / True | False / False | not selected |
| whisper | 1 / 1 |  | 0 | True / True | False / False | not selected |
| xbot | 0 / 1 | phase1 | 1 | True / True | False / False | xbot=pass |
| xbot.agent | 0 / 1 | phase1 | 0 | True / True | False / False | not selected |
| xfeat | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| xge | 0 / 12 |  | 1 | False / False | False / False | not selected |
| xge_shape_ex | 0 / 3 |  | 1 | False / False | False / False | not selected |
| xhttp | 8 / 1 |  | 2 | False / False | True / False | xhttp=fail |
| xhttp.body | 4 / 1 |  | 1 | False / False | True / False | not selected |
| xhttp.body_async | 2 / 1 |  | 1 | False / False | True / False | not selected |
| xhttp.body_codec | 4 / 1 |  | 1 | False / False | True / False | not selected |
| xhttp.body_decode | 2 / 1 |  | 1 | False / False | True / False | not selected |
| xhttp.body_file | 2 / 1 |  | 1 | False / False | True / False | xhttp.body_file=fail |
| xhttp.body_stream | 2 / 1 |  | 1 | False / False | True / False | not selected |
| xhttp.client | 21 / 1 |  | 3 | False / False | True / False | xhttp.client=fail |
| xhttp.form_data | 6 / 1 |  | 1 | False / False | True / False | not selected |
| xhttp.multipart | 11 / 1 |  | 1 | False / False | True / False | not selected |
| xhttp.protocol | 9 / 1 |  | 3 | False / False | True / False | not selected |
| xlsx | 1 / 1 |  | 0 | True / True | False / False | not selected |
| xmail | 1 / 1 | phase1 | 4 | True / True | True / False | xmail=fail |
| xml.dom | 4 / 1 | phase1 | 0 | True / True | False / False | not selected |
| xml.etree | 4 / 1 | phase1 | 0 | True / True | False / False | not selected |
| xml.sax | 3 / 1 | phase1 | 0 | True / True | False / False | not selected |
| xregex | 8 / 1 | phase1 | 2 | True / True | True / True | xregex=pass, xregex-lifetime=pass |
| xssh | 1 / 1 | phase1 | 1 | True / True | True / False | xssh=fail |
| xui | 0 / 14 |  | 1 | False / False | False / False | not selected |
| xws | 12 / 1 |  | 3 | False / False | True / False | not selected |
| xxhash | 3 / 1 | featured | 0 | True / True | True / True | xxhash=pass |
| yaml | 1 / 1 | phase1 | 0 | True / True | False / False | not selected |
| zeromq | 1 / 1 |  | 0 | True / True | False / False | not selected |
| zip | 4 / 1 | phase1 | 0 | True / True | False / False | not selected |
| zstd | 3 / 1 | featured | 0 | True / True | False / False | not selected |
