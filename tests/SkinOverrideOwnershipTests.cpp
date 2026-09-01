#include "BodyChangerNG/SkinOverrideOwnership.h"

#include <cassert>

int main()
{
    using namespace bcn::skin_override::ownership;

    static_assert(IsOwnedTexturePath("textures\\BodyChangerNG\\Cache\\Skin\\female.dds"));
    static_assert(IsOwnedTexturePath("Textures/BodyChangerNG/Cache/Skin-Face/head.dds"));
    static_assert(!IsOwnedTexturePath("textures\\wetfunctionredux\\female_s.dds"));
    static_assert(!IsOwnedTexturePath("textures\\actors\\character\\female\\femalebody_1.dds"));

    assert(MayReplace(false, {}));
    assert(MayReplace(true, "textures\\bodychangerng\\cache\\skin\\body.dds"));
    assert(!MayReplace(true, "textures\\wetfunctionredux\\wet_s.dds"));
    assert(MayRemove(true, "textures/bodychangerng/cache/skin-face/head.dds"));
    assert(!MayRemove(true, "textures\\othermod\\head.dds"));
    assert(!MayRemove(false, "textures\\bodychangerng\\cache\\skin\\body.dds"));
}
