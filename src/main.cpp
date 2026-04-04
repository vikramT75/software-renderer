#include "core/asset_manager.h"
#include "core/renderer.h"
#include "loaders/obj_loader.h"
#include "math/math_utils.h"
#include "platform/sdl_window.h"
#include "scene/entity.h"
#include "scene/material.h"
#include "scene/scene.h"
#include "shading/pbr.h"
#include "shading/texture.h"
#include <iostream>
#include <string>
#include <vector>

int main(int, char *[])
{
    const int WIDTH = 800;
    const int HEIGHT = 600;

    SDLWindow window("swr — software renderer", WIDTH, HEIGHT);
    Renderer renderer(WIDTH, HEIGHT);
    AssetManager assets; // Manages mesh/texture lifetimes and caching

    // =======================================================================
    // Scene Setup
    // =======================================================================
    Scene scene;

    // --- Camera ---
    scene.camera.position = {0.f, 4.f, 10.f};
    scene.camera.yaw = -MathUtils::HALF_PI;
    scene.camera.pitch = MathUtils::toRadians(-15.f);
    scene.camera.fovY = MathUtils::toRadians(60.f);
    scene.camera.aspect = static_cast<float>(WIDTH) / HEIGHT;
    scene.camera.zNear = 0.1f;
    scene.camera.zFar = 100.f;
    scene.camera.moveSpeed = 3.f;
    scene.camera.lookSpeed = 0.003f;

    // --- Lights ---
    Light keyLight;
    keyLight.type = LightType::Directional;
    keyLight.direction = Vec3{-1.f, -1.f, -1.f}.normalized();
    keyLight.color = {1.f, 1.f, 1.f};
    keyLight.intensity = 2.f;
    keyLight.castsShadow = true; // Use the decoupled shadow flag
    scene.lights.add(keyLight);

    Light fillLight;
    fillLight.type = LightType::Directional;
    fillLight.direction = Vec3{1.f, -0.5f, 0.f}.normalized();
    fillLight.color = {0.8f, 0.8f, 1.0f};
    fillLight.intensity = 0.5f;
    scene.lights.add(fillLight);

    Light flashlight;
    flashlight.type = LightType::Point;
    flashlight.color = {1.f, 0.95f, 0.85f};
    flashlight.intensity = 0.f;
    flashlight.attConstant = 1.f;
    flashlight.attLinear = 0.22f;
    flashlight.attQuadratic = 0.2f;
    scene.lights.add(flashlight);

    // --- Shadow Map ---
    scene.shadowMap.width = 1024;
    scene.shadowMap.height = 1024;
    scene.shadowMap.bias = 0.012f;
    scene.shadowMap.setup(keyLight.direction, {0.f, 0.f, 0.f}, 12.f, 0.1f, 40.f);

    // =======================================================================
    // Shaders & Materials
    // =======================================================================

    PBRShader centerPBR;
    centerPBR.albedoMap = assets.getTexture("assets/textures/gabagool.jpg");
    centerPBR.normalMap = assets.getTexture("assets/textures/cube_normal.png"); // Normal mapping
    centerPBR.metallic = 0.3f;
    centerPBR.roughness = 0.4f;

    PBRShader blockPBR;
    blockPBR.albedo = {0.8f, 0.2f, 0.2f};
    blockPBR.albedoMap = assets.getTexture("assets/textures/gospelv1.png");
    blockPBR.roughness = 0.8f;

    PBRShader groundPBR;
    groundPBR.albedo = {0.4f, 0.38f, 0.35f};
    groundPBR.roughness = 0.9f;

    PBRShader pillarPBR;
    pillarPBR.albedo = {0.3f, 0.3f, 0.3f};
    pillarPBR.roughness = 0.7f;

    PBRShader goldPBR;
    goldPBR.albedo = {1.f, 0.85f, 0.4f};
    goldPBR.metallic = 1.0f;
    goldPBR.roughness = 0.15f;

    PBRShader plasticPBR;
    plasticPBR.albedo = {0.1f, 0.5f, 0.9f};
    plasticPBR.roughness = 0.1f;

    PBRShader mattePBR;
    mattePBR.albedo = {0.8f, 0.2f, 0.2f};
    mattePBR.roughness = 0.9f;

    Material groundMat = {&groundPBR, CullMode::None, false};
    Material centerMat = {&centerPBR, CullMode::Back, true};
    Material pillarMat = {&pillarPBR, CullMode::Back, true};
    Material blockMat = {&blockPBR, CullMode::Back, true};
    Material matGold = {&goldPBR, CullMode::Back, true};
    Material matPlastic = {&plasticPBR, CullMode::Back, true};
    Material matMatte = {&mattePBR, CullMode::Back, true};

    // =======================================================================
    // Entity Hierarchy Construction
    // =======================================================================

    // Ground (Root)
    Entity *ground = scene.createEntity("ground");
    ground->mesh = assets.getMesh("assets/models/ground.obj");
    ground->material = &groundMat;
    ground->transform.scaleVec = {2.f, 1.f, 2.f};

    // Centerpiece (Root)
    Entity *center = scene.createEntity("center");
    center->mesh = assets.getMesh("assets/models/cube.obj");
    center->material = &centerMat;
    center->transform.position = {0.f, 1.5f, 0.f};

    // Pillars (Root)
    float pDist = 4.0f;
    Vec3 pPos[4] = {{pDist, 2.f, pDist}, {-pDist, 2.f, pDist}, {pDist, 2.f, -pDist}, {-pDist, 2.f, -pDist}};
    for (int i = 0; i < 4; ++i)
    {
        Entity *p = scene.createEntity("pillar_" + std::to_string(i));
        p->mesh = assets.getMesh("assets/models/cube.obj");
        p->material = &pillarMat;
        p->transform.position = pPos[i];
        p->transform.scaleVec = {0.8f, 4.0f, 0.8f};
    }

    // Orbiters (Children of Centerpiece)
    Material *oMats[3] = {&matGold, &matPlastic, &matMatte};
    std::vector<Entity *> orbiters;
    for (int i = 0; i < 3; ++i)
    {
        Entity *o = scene.createEntity("orbiter_" + std::to_string(i));
        o->mesh = assets.getMesh("assets/models/cube.obj");
        o->material = oMats[i];
        o->transform.scaleVec = {0.4f, 0.4f, 0.4f};

        center->addChild(o); // Establishing hierarchy
        orbiters.push_back(o);
    }

    // High Block (Root)
    Entity *block = scene.createEntity("block");
    block->mesh = assets.getMesh("assets/models/cube.obj");
    block->material = &blockMat;
    block->transform.position = {0.f, 5.0f, 0.f};

    // =======================================================================
    // Main Loop
    // =======================================================================
    float angle = 0.f;
    uint32_t lastTicks = SDL_GetTicks();

    while (window.pollEvents())
    {
        uint32_t currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.f;
        lastTicks = currentTicks;
        angle += 1.5f * dt;

        // --- Animations ---
        // Rotating the center rotates all children automatically
        center->transform.rotation.y = angle;
        center->transform.rotation.x = angle * 0.5f;

        // Local-space orbiter movement
        for (int i = 0; i < 3; ++i)
        {
            float offset = angle + (i * MathUtils::TWO_PI / 3.f);
            // These coordinates are now RELATIVE to the centerpiece
            orbiters[i]->transform.position = {std::cos(offset) * 2.5f, std::sin(angle * 2.f + i) * 0.5f,
                                               std::sin(offset) * 2.5f};
            orbiters[i]->transform.rotation.x += 1.0f * dt;
        }

        // --- Camera & Interaction ---
        scene.lights.lights[2].position = scene.camera.position;
        const uint8_t *keys = SDL_GetKeyboardState(nullptr);
        scene.lights.lights[2].intensity = keys[SDL_SCANCODE_F] ? 5.0f : 0.0f;

        const InputState &in = window.input();
        scene.camera.update(dt, in.w, in.s, in.a, in.d, in.e, in.q, in.shift, in.mouseDX, in.mouseDY);

        // --- Render ---
        // This call now triggers scene.updateHierarchy() internally
        renderer.render(scene);
        window.present(renderer.pixelData());
    }

    return 0;
}