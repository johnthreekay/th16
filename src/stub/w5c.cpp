// Opaque stubs for wave 5 range C (0x440000-0x44c000): callees from other
// ranges. Compiled without /GL.
int w5c_sink(void *object, int value)
{
    return value;
}

int w5c_sink_f(float a, float b)
{
    return (int)(a * b);
}
