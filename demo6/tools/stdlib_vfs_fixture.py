"""Independent stored XRTPACK v1 vector; never uses the language IO layer."""
from pathlib import Path
import struct,zlib

def create(directory):
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
    data=b'a\0b';name=b'entry.bin'
    index=struct.pack('<HBBIQQQII',len(name),0,0,0,80,len(data),len(data),zlib.crc32(data),0)+name
    size=80+len(data)+len(index)+32
    header=struct.pack('<8sHHIII6QII',b'XRTPACK\0',1,0,80,0,1,80,len(data),80+len(data),len(index),size,len(data),zlib.crc32(index),0)
    trailer=struct.pack('<8sQQ',b'XRTPEND\0',size,0)
    archive=header+data+index+trailer+struct.pack('<II',zlib.crc32(trailer+header),0)
    (directory/'entry.bin').write_bytes(data)
    (directory/'vfs-disk.bin').write_bytes(data)
    pack=directory/'vfs-pack.bin';pack.write_bytes(archive);return directory,pack
