"""Exact streaming freeze hashes, boundary normalization and truncation refusal."""
import hashlib,io,json
from freeze_stdlib_source import blob_hashes

class Blocks(io.BytesIO):
    def __init__(self,data,limit):super().__init__(data);self.limit=limit;self.maximum=0
    def read(self,size=-1):
        assert 0<=size<=65536,size
        self.maximum=max(self.maximum,size);return super().read(min(size,self.limit))

def main():
    vectors=[b'',b'\r',b'\n',b'\r\n',b'\r\r\n',b'a\0b\r\nz\r',b'x'*65535+b'\r\ny\r',b'\r\n'*65537]
    count=0
    for data in vectors:
        for limit in (1,2,3,65535,65536):
            for declared in (None,len(data)):
                stream=Blocks(data,limit);raw,normalized=blob_hashes(stream,declared)
                assert raw==hashlib.sha256(data).hexdigest()
                assert normalized==hashlib.sha256(data.replace(b'\r\n',b'\n')).digest()
                assert stream.tell()==len(data);count+=1
    for data in vectors:
        try:blob_hashes(Blocks(data,65536),len(data)+1)
        except AssertionError:count+=1
        else:raise AssertionError('Truncated declared blob was accepted')
    print(json.dumps(dict(Complete=True,Vectors=count,MaximumRead=65536)),flush=True)
if __name__=='__main__':main()
