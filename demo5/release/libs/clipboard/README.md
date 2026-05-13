# clipboard

`clipboard` is a Windows clipboard extension package for xlang.

## Core API

- `formats()` / `types()` returns an array of clipboard format names.
- `formats_json()` / `types_json()` returns format ids and names as JSON.
- `has_text()`, `has_image()`, `has_files()` checks common data classes.
- `get_text()` / `set_text(text)` reads and writes Unicode clipboard text.
- `get_files()` / `get_file()` reads CF_HDROP file paths.
- `set_files(paths)` / `set_file(path)` writes file-drop clipboard data.
- `save_image(path)` saves the clipboard image as a BMP file.
- `set_image(path)` writes a BMP file to the clipboard as CF_DIB.
- `image_info_json()` returns width, height, bit depth, compression, and byte count.
- `clear()`, `sequence_number()`, `count_formats()`, `format_name(id)`, `register_format(name)`, and `is_format_available(id)` expose utility operations.

## Notes

Image read/write is implemented with Win32 DIB/BMP data. `save_image()` writes BMP data even if the target extension is different. `set_image()` currently accepts BMP files.

```xl
import clipboard

func main() as int
	clipboard.set_text("hello clipboard")
	if clipboard.get_text() != "hello clipboard"
		return 1
	end

	auto formats = clipboard.types_json()
	clipboard.set_file("D:\\temp\\example.txt")
	auto first = clipboard.get_file()
	return 0
end
```
