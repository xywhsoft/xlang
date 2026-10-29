static __thread int value;

int *tls_address(int number)
{
    value = number;
    return &value;
}
