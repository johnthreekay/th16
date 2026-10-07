// Opaque helpers for wave 4's interpreter placeholders
// (src/placeholder/w4c.cpp). Compiled without /GL.
int w4c_placeholder_sink(void *object, int value)
{
    return (int)object + value;
}
