#include "BodyChangeNG/SkinOverrideOwnership.h"

#include <cassert>

int main()
{
    using namespace bcn::skin_override::ownership;

    static_assert(IsOwnedTexturePath("textures\\BodyChangeNG\\Cache\\Skin\\female.dds"));
    static_assert(IsOwnedTexturePath("Textures/BodyChangeNG/Cache/Skin-Face/head.dds"));
    static_assert(IsOwnedTexturePath("textures\\BodyChangerNG\\Cache\\Skin\\female.dds"));
    static_assert(IsOwnedTexturePath("Textures/BodyChangerNG/Cache/Skin-Face/head.dds"));
    static_assert(!IsOwnedTexturePath("textures\\wetfunctionredux\\female_s.dds"));
    static_assert(!IsOwnedTexturePath("textures\\actors\\character\\female\\femalebody_1.dds"));
    static_assert(IsRacialSkinVarianceTexturePath(
        "actors\\character\\RSV\\bretonfemale\\femalehead.dds"));
    static_assert(IsRacialSkinVarianceTexturePath(
        "Textures/Actors/Character/RSV/oldnord/malehead_msn.dds"));
    static_assert(!IsRacialSkinVarianceTexturePath(
        "textures\\actors\\character\\female\\femalehead.dds"));
    static_assert(!IsRacialSkinVarianceTexturePath(
        "textures\\bodychangeng\\cache\\skin-face\\head.dds"));

    assert(MayReplace(false, {}));
    assert(MayReplace(true, "textures\\bodychangeng\\cache\\skin\\body.dds"));
    assert(!MayReplace(true, "textures\\wetfunctionredux\\wet_s.dds"));
    assert(MayRemove(true, "textures/bodychangeng/cache/skin-face/head.dds"));
    assert(!MayRemove(true, "textures\\othermod\\head.dds"));
    assert(!MayRemove(false, "textures\\bodychangeng\\cache\\skin\\body.dds"));
}
