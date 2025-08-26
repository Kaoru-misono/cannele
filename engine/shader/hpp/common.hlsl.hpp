#ifndef COMMON_HLSL_HPP
#define COMMON_HLSL_HPP

#ifdef CPP_SCOPE
    #include <math/type.hpp>

    #define NAMESPACE_CANNELE_BEGIN namespace cannele {
    #define NAMESPACE_CANNELE_END }
    #define CHECK_STRUCT_ALIGNMENT(s) static_assert( sizeof(s) % 16 == 0, "sizeof(" #s ") is not multiple of 16" )
#endif

#ifdef SLANG_SCOPE
    #define NAMESPACE_CANNELE_BEGIN
    #define NAMESPACE_CANNELE_END
    #define CHECK_STRUCT_ALIGNMENT(s)

    #define PUSHCONSTANTS(TYPE, NAME) [[vk::push_constant]] TYPE NAME

#endif

NAMESPACE_CANNELE_BEGIN

    struct StorageDouble4
    {
        uint2 x;
        uint2 y;
        uint2 z;
        uint2 w;

#ifdef HLSL_SCOPE
        void fill(double3 v)
        {
            asuint(v.x, x.x, x.y);
            asuint(v.y, y.x, y.y);
            asuint(v.z, z.x, z.y);
        }

        double3 toDouble3()
        {
            double3 d;
            d.x = asdouble(x.x, x.y);
            d.y = asdouble(y.x, y.y);
            d.z = asdouble(z.x, z.y);

            return d;
        }

        float3 castFloat3()
        {
            return float3(toDouble3());
        }
#endif
    };

NAMESPACE_CANNELE_END

#endif // COMMON_HLSL_HPP
