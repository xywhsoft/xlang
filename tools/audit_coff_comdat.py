"""Read-only COFF archive COMDAT section, auxiliary and key audit."""
from pathlib import Path
import argparse,hashlib,json,struct,collections

def parse(body,label):
    machine,count,stamp,symbol_at,symbol_count,optional,flags=struct.unpack_from('<HHLLLHH',body)
    if machine!=0x8664:return dict(member=label,unsupportedMachine=hex(machine))
    strings=body[symbol_at+18*symbol_count:]
    def string(at):
        end=strings.find(b'\0',at)
        if end<0:raise ValueError('Unterminated COFF string')
        return strings[at:end].decode('ascii')
    def name(raw,section=False):
        if section and raw.startswith(b'/'):return string(int(raw[1:].split(b'\0')[0]))
        if raw[:4]==b'\0'*4:return string(int.from_bytes(raw[4:],'little'))
        return raw.split(b'\0')[0].decode('ascii')
    sections={};symbols=[]
    for i in range(count):
        raw=body[20+optional+40*i:20+optional+40*(i+1)];v=struct.unpack_from('<LLLLLLHHL',raw,8)
        sections[i+1]=dict(index=i+1,name=name(raw[:8],True),flags=v[-1],size=v[2],raw=v[3],relocations=v[6],selection=0,association=0,keys=[],statics=[],aux=[])
    i=0
    while i<symbol_count:
        raw=body[symbol_at+18*i:symbol_at+18*(i+1)];value,section,kind,storage,aux=struct.unpack_from('<LhHBB',raw,8);n=name(raw[:8]);s=dict(index=i,name=n,value=value,section=section,type=kind,storage=storage,aux=aux)
        if section>0:
            if storage==2:sections[section]['keys'].append(s)
            if storage==3:sections[section]['statics'].append(s)
            if storage==3 and aux and kind==0:
                info=body[symbol_at+18*(i+1):symbol_at+18*(i+2)];sections[section]['selection']=info[14];sections[section]['association']=int.from_bytes(info[12:14],'little');sections[section]['aux'].append(dict(symbol=s,rawHex=info.hex()))
        symbols.append(s);i+=1+aux
    comdat=[s for s in sections.values()if s['flags']&0x1000]
    for s in comdat:
        key_section=sections.get(s['association'])if s['selection']==5 else s
        s['effectiveKey']=(key_section['keys'][0]['name']if key_section and key_section['keys']else None)
        s['rejectedByOriginalReader']=s['selection']not in(1,2,3,5)or not s['effectiveKey']
    return dict(member=label,sha256=hashlib.sha256(body).hexdigest(),sections=count,symbols=symbol_count,comdat=comdat)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();data=a.input.read_bytes();rows=[]
    if data[:8]==b'!<arch>\n':
        at=8;names=b''
        while at<len(data):
            h=data[at:at+60];n=h[:16].decode().strip();size=int(h[48:58]);body=data[at+60:at+60+size];at+=60+size+(size&1)
            if n=='//':names=body;continue
            if n=='/':continue
            if n.startswith('/')and n[1:].isdigit():n=names[int(n[1:]):].split(b'/\n')[0].decode()
            else:n=n.rstrip('/')
            rows.append(parse(body,n))
    else:rows.append(parse(data,a.input.name))
    bad=[dict(member=row['member'],**s)for row in rows for s in row.get('comdat',[])if s['rejectedByOriginalReader']]
    a.out.mkdir(parents=True,exist_ok=False);r=dict(input=str(a.input.resolve()),inputSha256=hashlib.sha256(data).hexdigest(),members=rows,rejected=bad,selections=dict(collections.Counter(str(s['selection'])for row in rows for s in row.get('comdat',[]))))
    (a.out/'results.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(dict(members=len(rows),rejected=len(bad),selections=r['selections'],first=bad[:3]),indent=2))
if __name__=='__main__':main()
