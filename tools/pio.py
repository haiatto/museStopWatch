"""Run the workspace PlatformIO with Windows long-path archive support."""
import os
from pathlib import Path
import shutil
import sys
import time

ROOT = Path(__file__).resolve().parent.parent
os.environ["PYTHONUTF8"] = "1"
os.environ["PYTHONIOENCODING"] = "utf-8"
for stream in (sys.stdout, sys.stderr):
    if hasattr(stream, "reconfigure"):
        stream.reconfigure(encoding="utf-8", errors="replace")
os.environ["PLATFORMIO_CORE_DIR"] = str(ROOT / ".tools" / "platformio")

if os.name == "nt":
    from platformio.package.unpack import BaseArchiver, ZIPArchiver, FileUnpacker
    from platformio import fs

    def long_path(path):
        path = os.path.abspath(os.fspath(path))
        if path.startswith("\\\\?\\"):
            return path
        if path.startswith("\\\\"):
            return "\\\\?\\UNC\\" + path[2:]
        return "\\\\?\\" + path

    original_resolve = BaseArchiver.resolve_path
    BaseArchiver.resolve_path = staticmethod(
        lambda path: long_path(original_resolve(path))
    )

    def after_extract(self, item, dest_dir):
        path = long_path(os.path.join(dest_dir, item.filename))
        attrs = item.external_attr >> 16
        if attrs:
            os.chmod(path, attrs)
        timestamp = time.mktime(tuple(item.date_time) + (0, 0, 0))
        os.utime(path, (timestamp, timestamp))

    ZIPArchiver.after_extract = after_extract
    ZIPArchiver.get_item_filename = lambda self, item: os.path.normpath(item.filename)
    original_unpack = FileUnpacker.unpack

    def unpack(self, dest_dir=None, *args, **kwargs):
        destination = dest_dir or os.getcwd()
        if isinstance(self._archiver, ZIPArchiver):
            destination = long_path(destination)
        return original_unpack(self, destination, *args, **kwargs)

    FileUnpacker.unpack = unpack
    original_copytree = shutil.copytree

    def copytree(src, dst, *args, **kwargs):
        return original_copytree(long_path(src), long_path(dst), *args, **kwargs)

    shutil.copytree = copytree
    original_rmtree = fs.rmtree

    def rmtree(path):
        resolved = Path(path).resolve()
        tool_root = (ROOT / '.tools').resolve()
        if resolved.is_relative_to(tool_root):
            return original_rmtree(long_path(path))
        return original_rmtree(path)

    fs.rmtree = rmtree

from platformio.__main__ import main

if __name__ == "__main__":
    sys.exit(main())
