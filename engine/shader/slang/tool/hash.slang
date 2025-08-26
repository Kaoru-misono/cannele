// Simple hash uint.
// from niagara stream. see https://www.youtube.com/watch?v=BR2my8OE1Sc
uint simple_hash(uint a)
{
   a = (a + 0x7ed55d16) + (a << 12);
   a = (a ^ 0xc761c23c) ^ (a >> 19);
   a = (a + 0x165667b1) + (a << 5);
   a = (a + 0xd3a2646c) ^ (a << 9);
   a = (a + 0xfd7046c5) + (a << 3);
   a = (a ^ 0xb55a4f09) ^ (a >> 16);
   return a;
}

// Simple hash color from uint value.
// from niagara stream. see https://www.youtube.com/watch?v=BR2my8OE1Sc
float3 simple_hash_color(uint i)
{
    uint h = simple_hash(i);
    return float3(float(h & 255), float((h >> 8) & 255), float((h >> 16) & 255)) / 255.0;
}
