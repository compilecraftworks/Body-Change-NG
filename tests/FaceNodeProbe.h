#pragma once

// Exact product resolver with read-only engine boundaries replaced by fakes.
#include <stdexcept>
#include <string>
#include <vector>

namespace RE
{
    struct BSShaderMaterial {
        enum class Type { kLighting, other };
        enum class Feature { kFaceGen, kFaceGenRGBTint, other };
        Type type{ Type::kLighting };
        Feature feature{ Feature::kFaceGen };
        Type GetType() const { return type; }
        Feature GetFeature() const { return feature; }
    };
    struct Shader { BSShaderMaterial* material{}; };
    struct BSGeometry;
    struct NiAVObject {
        std::string name;
        std::vector<BSGeometry*> shapes;
        NiAVObject* GetObjectByName(const std::string& value);
    };
    struct BSGeometry : NiAVObject {
        Shader shader;
        Shader* lightingShaderProp_cast() { return &shader; }
    };
    inline NiAVObject* NiAVObject::GetObjectByName(const std::string& value) {
        for (auto* shape : shapes) if (shape && shape->name == value) return shape;
        return nullptr;
    }
    struct BGSHeadPart { enum class HeadPartType { kFace }; std::string formEditorID; };
    struct TESNPC {
        BGSHeadPart* head{};
        BGSHeadPart* GetCurrentHeadPartByType(BGSHeadPart::HeadPartType) { return head; }
    };
    struct Actor {
        bool loaded{ true };
        NiAVObject* face{}, *root{};
        TESNPC* base{};
        bool Is3DLoaded() const { return loaded; }
        NiAVObject* GetFaceNodeSkinned() { return face; }
        NiAVObject* Get3D(bool) { return root; }
        TESNPC* GetActorBase() { return base; }
    };
    namespace BSVisit {
        enum class BSVisitControl { kContinue };
        template<class Visitor> void TraverseScenegraphGeometries(NiAVObject* root, Visitor visitor) {
            for (auto* shape : root->shapes) visitor(shape);
        }
    }
}
namespace REL {
    struct Module { static Module& get() { static Module instance; return instance; } int version() { return 0; } };
}
namespace bcn::runtime {
    enum class GameBranch { supported, unsupported };
    inline bool supported{ true };
    inline GameBranch ResolveGameBranch(int) { return supported ? GameBranch::supported : GameBranch::unsupported; }
}
namespace bcn::face_skin {
#include "face_resolve_node.inc"

    inline void TestProductFaceResolver() {
        const auto check = [](bool ok) { if (!ok) throw std::runtime_error("product face resolver boundary failed"); };
        RE::BSShaderMaterial material;
        RE::BSGeometry head, body, mouth, second;
        head.name = "FollowerHead"; body.name = "FemaleHeadNord"; mouth.name = "Mouth"; second.name = "OtherHead";
        for (auto* shape : { &head, &body, &mouth, &second }) shape->shader.material = &material;
        RE::NiAVObject face, root;
        face.shapes = { &head, &mouth }; root.shapes = { &body, &head, &mouth };
        RE::BGSHeadPart part{ "FemaleHeadNord" };
        RE::TESNPC base{ &part };
        RE::Actor actor{ true, &face, &root, &base };
        check(ResolveNodeName(&actor) == "FollowerHead"); // body has exact name, but is outside FaceGen
        part.formEditorID = head.name;
        check(ResolveNodeName(&actor) == head.name); // normal player/NPC remains exact
        body.name = head.name;
        check(ResolveNodeName(&actor).empty()); // NiOverride would hit the body first
        body.name = "Body"; base.head = nullptr;
        check(ResolveNodeName(&actor) == head.name); // usable exported face without a HeadPart
        face.shapes.push_back(&second); root.shapes.push_back(&second);
        check(ResolveNodeName(&actor).empty()); // two fallback heads: no guess
        base.head = &part;
        check(ResolveNodeName(&actor) == head.name); // exact wins over fallback
        actor.loaded = false; check(ResolveNodeName(&actor).empty()); actor.loaded = true;
        runtime::supported = false; check(ResolveNodeName(&actor).empty()); runtime::supported = true;
        actor.face = nullptr; check(ResolveNodeName(&actor).empty());
        check(ResolveNodeName(nullptr).empty());
    }
}
