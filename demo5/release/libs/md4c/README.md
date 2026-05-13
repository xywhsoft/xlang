# md4c

`md4c` renders Markdown text or files to HTML through the bundled MD4C C sources.

```xlang
import md4c

html = md4c.html("# Title\n\nHello **xlang**")
print(html)
```

## API

- `version() as string`
- `last_error() as string`
- `html(markdown as string) as string`
- `html_flags(markdown as string, parser_flags as int, renderer_flags as int) as string`
- `file_html(path as string, parser_flags as int, renderer_flags as int) as string`
- `commonmark_flags() as int`
- `github_flags() as int`
- `xhtml_flag() as int`
- `skip_utf8_bom_flag() as int`
- `verbatim_entities_flag() as int`

The package keeps MD4C's MIT license in `LICENSE.md`.
