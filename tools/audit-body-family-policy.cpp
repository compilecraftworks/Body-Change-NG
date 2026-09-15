// Diagnostic probe of CURRENT production rules, not a desired-behavior test.
// This does not emulate a loaded Actor or resolve its winning NIF/metadata.
#include "BodyChangeNG/BodyFamily.h"
#include <iostream>

int main()
{
    using namespace bcn::body_family;
    const auto report = [&](const char* label, Mask metadata, Mask loaded, Mask installed, SkinTextureLayout layout) {
        const auto family = ResolveActorFamily(loaded, metadata, installed, layout, Sex::female);
        std::cout << label << ": family=" << family << ", CBBE-visible="
                  << Matches(Bit(Family::cbbe), family) << '\n';
    };
    const auto standard = SkinTextureLayout::standard;
    const auto cbbe = Bit(Family::cbbe), unp = Bit(Family::unp), ube = Bit(Family::ube);
    std::cout << "Eila generic metadata=" << DetectText(
        "Immersive Wenches.esp IW_SkinNaked_Wench Skyrim.esm NakedTorso "
        "Actors\\Character\\Character Assets\\FemaleBody_1.nif", Sex::female) << '\n';
    report("CBBE + UBE installed / standard layout", 0, 0, cbbe | ube, standard);
    report("No metadata or installed family", 0, 0, 0, SkinTextureLayout::unknown);
    report("Only UBE detected / positive standard layout", 0, 0, ube, standard);
    report("UNP metadata / actual CBBE mesh signal", unp, cbbe, cbbe | unp, standard);
    report("UBE metadata / actual standard CBBE signal", ube, cbbe, cbbe | ube, standard);
}
