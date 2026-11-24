"""Generate the 72-member retained descriptor fixture for compiler acceptance."""

from pathlib import Path


MEMBER_COUNT = 72


def write_wide_named_call_fixture(path: Path):
    parameters = ','.join(f'c_int p{index}=0' for index in range(74))
    path.write_text(f'''func ordinalProbe({parameters}) as c_int
 return p63+p64+p72+p73
end
func main(...) as int
 assert ordinalProbe()==c_int(0)
 assert ordinalProbe(p73=c_int(4),p63=c_int(2),p72=c_int(3),p64=c_int(1))==c_int(10)
 println("WIDE_NAMED_ARGUMENTS_OK")
 return 0
end
''', encoding='utf8')


def write_wide_pack_fixture(folder: Path):
    members = [f'member{index}' for index in range(MEMBER_COUNT)]
    header = folder / 'wide_callback_pack.h'
    fields = [
        f'    int (*{member})(const WidePack*,' +
        ('const unsigned char*, int);' if index == 71 else 'int);')
        for index, member in enumerate(members)
    ]
    header.write_text(
        '#ifndef WIDE_CALLBACK_PACK_H\n#define WIDE_CALLBACK_PACK_H\n'
        '#include <__xl_runtime/xrt_decl.h>\n'
        'typedef struct WidePackOwner {\n'
        '    xrtownershipref reference;\n'
        '    bool (*hold)(const void*);\n'
        '    bool (*drop)(const void*);\n'
        '} WidePackOwner;\n'
        'typedef struct WidePack WidePack;\n'
        'struct WidePack {\n    void* userdata;\n'
        + '\n'.join(fields) +
        '\n};\n'
        'typedef struct WidePackAux { int seed; } WidePackAux;\n'
        'int wide_pack_call(const WidePack*,const WidePackOwner*,const WidePackAux*,int);\n'
        '#endif\n',
        encoding='utf8',
    )
    provider = folder / 'wide_callback_pack_native.c'
    provider.write_text(r'''#include "wide_callback_pack.h"
#include <assert.h>

int wide_pack_call(const WidePack* descriptor,const WidePackOwner* owner,const WidePackAux* auxiliary,int tail)
{
    assert(descriptor && auxiliary);
    int result=auxiliary->seed+tail;
    if(!owner){
        assert(!descriptor->userdata && !descriptor->member63 && !descriptor->member64 && !descriptor->member71);
        return result;
    }
    assert(descriptor->userdata && owner->reference.Data==descriptor->userdata);
    if(!owner->hold(descriptor->userdata))return -1;
    if(descriptor->member63)result+=descriptor->member63(descriptor,2);
    if(descriptor->member64)result+=descriptor->member64(descriptor,3);
    if(descriptor->member71){
        const unsigned char text[]={'a',0,'b'};
        result+=descriptor->member71(descriptor,text,3);
    }
    return owner->drop(descriptor->userdata) ? result:-1;
}
''', encoding='utf8')
    attributes = [
        '@header("wide_callback_pack.h")', '@no_raise', '@c_result("integer_min",0)',
        '@c_pack(0,"WidePack",0,72)', '@c_pack_callback_group(0,"userdata",1,"optional")',
        '@c_pack(2,"WidePackAux",72,1)', '@c_pack_field(2,"seed",72,"value")',
        '@c_callback_span(71,1,"const unsigned char*","int")',
    ]
    parameters = []
    for index, member in enumerate(members):
        attributes.append(f'@c_pack_field(0,"{member}",{index},"owned_callback")')
        attributes.append(f'@c_owned_callback({index},-1,"WidePackOwner",-1,"receiver")')
        callback_type = 'TailHandler' if index == 71 else 'Handler'
        parameters.append(f'{callback_type}? {member}=null')
    parameters.extend(['c_int seed=0', 'c_int tail=0'])
    declaration = 'extern "wide_pack_call" ' + ' '.join(attributes)
    declaration += ' func call(' + ','.join(parameters) + ') as c_int\n'
    fixture = folder / 'wide_callback_pack.xl'
    fixture.write_text('''extern "const WidePack*" class Receiver @header("wide_callback_pack.h") @lifetime("callback")
 native point handle
end
extern "const xbytesview*" class Slice @header("wide_callback_pack.h") @lifetime("callback")
 native point handle
 extern "Size" @c_field get size() as c_size
 extern "Data" @c_field @c_length("Size","size_t") @c_result("bytes_view") get copy() as bytes
end
typedef Handler=func(Receiver,c_int) as c_int
typedef TailHandler=func(Receiver,Slice) as c_int
''' + declaration + '''func scalar(Receiver receiver,c_int input) as c_int
 return input+c_int(10)
end
func tailBytes(Receiver receiver,Slice input) as c_int
 assert input.copy==b"a\\0b"
 return c_int(input.size)
end
func main(...) as int
 assert call()==c_int(0)
 assert call(seed=c_int(9),tail=c_int(5))==c_int(14)
 assert call(member71=tailBytes)==c_int(3)
 assert call(member63=scalar,member64=scalar,member71=tailBytes,seed=c_int(9),tail=c_int(5))==c_int(42)
 println("WIDE_PACK_CALLBACK_OK")
 return 0
end
''', encoding='utf8')
