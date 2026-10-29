"""Exact public persistence ABI fixture checks against real typed/lowered XLIR."""
def check(typed,ir,text):
    types={row['name']:row['id'] for row in typed['types']}
    profile,scalar,integer=types['PersistenceProfile'],types['string'],types['int']
    layout=[r for r in typed['classLayouts'] if r['type']==profile]
    assert len(layout)==1 and layout[0]['baseType'] and layout[0]['persistenceFormatMask']==7
    fields=[r for r in typed['classFields'] if r['ownerType']==profile]
    assert len(fields)==5
    for name,value,count in (('jsonPersistenceFlags',0,1),('jsonPersistenceFlags',5,1),
        ('jsonRenamed',1,2),('xlonPersistenceFlags',0,1),('xlonPersistenceFlags',1,4),('xlonRenamed',1,1)):
        assert sum(r[name]==value for r in fields)==count,(name,value)
    expected={
        'json-stringify':[(profile,1),(profile,2),(scalar,1),(integer,1)],
        'xlon-stringify':[(scalar,1),(profile,1)],
        'json-parse':[(profile,1)],'xlon-parse':[(profile,1)],
        'json-write':[(profile,3),(scalar,2)],'xlon-write':[(profile,2)],
        'json-read':[(profile,1)],'xlon-read':[(profile,1)]}
    boxed=[]
    instructions=ir['instructions']; definitions={r['result']:r for r in instructions if r['result']}
    def stored(value):
        load=definitions[value]; assert load['kindName']=='load'
        stores=[r for r in instructions if r['kindName']=='store' and r['targetSymbol']==load['targetSymbol'] and r['type']==load['type']]
        assert len(stores)==1
        releases=[r for r in instructions if r['kindName']=='release' and r['targetSymbol']==load['targetSymbol'] and r['type']==load['type']]
        assert releases and any(r['id']>load['id'] for r in releases)
        producer=definitions[stores[0]['operands'][0]]
        assert producer['id']<stores[0]['id']<load['id']
        return producer,load,stores[0],releases
    for suffix,payloads in expected.items():
        kind='persistence-'+suffix
        calls=[r for r in typed['intrinsicCalls'] if r['kindName']==kind]
        assert sorted((r['payloadType'],r['operandCount']) for r in calls)==sorted(payloads),kind
        write=suffix.endswith('write'); parse=suffix.endswith(('parse','read'))
        for r in calls:
            assert r['admissionName']=='verified' and r['errorAbiName']=='may-raise'
            assert r['resultAbiName']==('value' if write else 'owned')
            assert r['resultType']==(1 if write else profile if parse else scalar)
        ops=[r for r in instructions if r['runtimeOpName']==kind]
        assert len(ops)==len(payloads)
        assert sorted(r['payloadType'] for r in ops)==sorted(p for p,_ in payloads)
        for op in ops:
            assert op['kindName']=='runtime-call' and op['operandCount']==(3 if write else 2)
            if parse:
                assert op['type']==profile
                continue
            projection,load,_,releases=stored(op['operands'][0])
            expected_project='persistence-'+suffix.split('-')[0]+'-project'
            assert projection['runtimeOpName']==expected_project and projection['payloadType']==op['payloadType']
            assert load['id']<op['id'] and any(r['id']>op['id'] for r in releases)
            if op['payloadType'] in (scalar,integer):
                box,source_load,_,source_releases=stored(projection['operands'][0])
                assert box['runtimeOpName']==('xvalue-string-copy' if op['payloadType']==scalar else 'xvalue-i64')
                assert source_load['id']<projection['id']<op['id']
                assert any(r['id']>op['id'] for r in source_releases)
                boxed.append(op['id'])
    assert len(boxed)==4
    assert ir['persistenceProjectPlans'] and ir['persistenceRestoreRoots']
    assert all(not r['pending'] for r in ir['persistenceRestoreRoots'])
    for required in ('xl6PersistenceProjectV1(','xl6PersistenceRestoreParsedTakeV1(',
        'xl6JsonWriteFilePrettyV1(','xl6XlonWriteFilePrettyV1(','xrtJsonStringify(',
        'xrtXlonStringify(','xlir_persist_plan_2','xlir_persist_plan_4','"display_name"','"secret_name"'):
        assert required in text,required
    for forbidden in ('xlir_persist_json_assign_','xlir_persist_xlon_assign_',
        'xlir_persist_json_value_','xlir_persist_xlon_value_','_stringify_t','_write_t'):
        assert forbidden not in text,forbidden
    return dict(TypedCalls=sum(map(len,expected.values())),BoxedProjectionTransactions=len(boxed),
        NativeRestoreRoots=len(ir['persistenceRestoreRoots']),SchemaFields=len(fields))
