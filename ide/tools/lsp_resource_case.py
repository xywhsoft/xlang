"""Valid language input that reaches the native index's byte budget.

Unique numeric prefixes avoid a common long comparison prefix;the file stays
below the existing8MiB input limit. No synthetic compiler result is supplied.
"""
def index_byte_limit_source():
    text = ''.join('func huge_%04d_%s() as int\n return 1\nend\n' % (i, 'q'*4000)
                   for i in range(1600)) + 'func main(...) as int\n return 0\nend\n'
    assert len(text.encode()) < 8*1024*1024
    return text
