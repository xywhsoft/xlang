import configparser
import pathlib


ROOT = pathlib.Path("tmp")


def parser():
    result = configparser.ConfigParser(interpolation=None, strict=True)
    result.optionxform = str
    return result


def check_xlang_ini():
    config = parser()
    with (ROOT / "xlang_interop.ini").open("r", encoding="utf-8") as stream:
        config.read_file(stream)
    assert config["server"]["host"] == "localhost"
    assert config.getint("server", "port") == 8080
    assert config["用户"]["name"] == "测试"


def write_python_ini():
    config = parser()
    config["Main"] = {"Name": "xlang", "Enabled": "true", "Count": "42"}
    config["路径"] = {"Value": "中文/目录"}
    with (ROOT / "python_interop.ini").open("w", encoding="utf-8", newline="\n") as stream:
        config.write(stream)


if __name__ == "__main__":
    ROOT.mkdir(parents=True, exist_ok=True)
    check_xlang_ini()
    write_python_ini()
    print("ini_python: PASS")
