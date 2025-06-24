#ifdef CPP_SCOPE
    #pragma onces
    #include <math/type.hpp>

    namespace cannele
    {

    struct ImGuiDrawPushConsts
    {
        math::float2 scale{};
        math::float2 translate{};

        math::uint use_font{};
        math::uint texture_id{};
        math::uint sampler_id{};
    };

    }
#else

    #define PUSHCONSTANTS(TYPE, NAME) [[vk::push_constant]] TYPE NAME

    struct ImGuiDrawPushConsts
    {
        float2 scale;
        float2 translate;

        uint useFont;
        uint textureId;
        uint samplerId;
    };
    PUSHCONSTANTS(ImGuiDrawPushConsts, pushConstants);

#endif
