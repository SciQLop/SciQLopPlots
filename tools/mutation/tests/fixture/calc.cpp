int clamp_index(int i, int n)
{
    if (i < 0)
        return 0;
    if (i >= n)
        return n - 1;
    return i;
}
