#include "BodyChangeNG/RaceMenuFormDeletePolicy.h"
#include <cstdlib>
#include <iostream>

int main()
{
    using bcn::racemenu_form_delete::IsDirectFormHandle;
    const auto require=[](bool value){ if (!value) std::abort(); };
    using bcn::racemenu_form_delete::VerifyDirectFormSamples;
    require(VerifyDirectFormSamples(0xFFFF00000000ULL,0x14,0xFFFF00000014ULL,7,0xFFFF00000007ULL));
    require(!VerifyDirectFormSamples(0,0x14,0x14,7,7));
    require(!VerifyDirectFormSamples(0xFFFF00000000ULL,0x14,0x2001500000014ULL,7,0xFFFF00000007ULL));
    require(!VerifyDirectFormSamples(0xFFFF00000000ULL,0,0xFFFF00000000ULL,7,0xFFFF00000007ULL));
    require(!VerifyDirectFormSamples(0xFFFF00000000ULL,0x14,0xFFFF00000014ULL,7,0xFFFF00000008ULL));
    require(!VerifyDirectFormSamples(0xFFFF00000000ULL,0x14,0xFFFF00000014ULL,0x14,0xFFFF00000014ULL));
    require(!IsDirectFormHandle(0));
    require(!IsDirectFormHandle(0x0000FFFF00000000ULL)); // EmptyHandle
    require(!IsDirectFormHandle(0xFFFFFFFFFFFFFFFFULL));
    for (const auto id : {0x14U,0x813BAU,0xFF002E75U,0xFFFFFFFFU}) {
        require(IsDirectFormHandle(0x0000FFFF00000000ULL|id));
        // Every alias/effect ordinal and high-word boundary. Same low32 is
        // deliberately shared with a direct Form to catch narrowing regressions.
        for (std::uint64_t ordinal{};ordinal<0x10000;++ordinal) {
            const auto alias=(ordinal<<32)|id;
            require(IsDirectFormHandle(alias)==(ordinal==0xFFFF));
            for (auto tag : {1ULL,2ULL,3ULL,0x8000ULL,0xFFFFULL})
                require(!IsDirectFormHandle((tag<<48)|alias));
        }
    }
    std::cout<<"FormDeletePolicyTests passed (SE 1.5.97 verified encoding; no game lifecycle claim)\n";
}
