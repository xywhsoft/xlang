# Expat

- Upstream: `https://github.com/libexpat/libexpat`
- Version: `2.8.2` (`R_2_8_2`)
- Archive: `expat-2.8.2.tar.gz`
- SHA-256: `ef7d1994f533c9e7343d6c19f31064fc8ebbcbcaa144be3812b4f43052a05f4c`
- License: MIT, see `vendor/expat/COPYING`

The package manifest compiles only Expat's library core. Tests, command-line
tools, build-system files and documentation are not part of the package. The
entropy dispatcher retains only the Windows rand_s, Linux getrandom, BSD/macOS
arc4random_buf and portable /dev/urandom implementations selected by the fixed
cross-platform configuration.
