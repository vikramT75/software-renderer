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

int main(int, char *[])
{
    const int WIDTH = 800;
    const int HEIGHT = 600;

    SDLWindow window("swr — software renderer", WIDTH, HEIGHT);
    Renderer renderer(WIDTH, HEIGHT);

    // =======================================================================
    // Scene setup — all data lives here
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
    scene.lights.add(keyLight);

    Light fillLight;
    fillLight.type = LightType::Directional;
    fillLight.direction = Vec3{1.f, -0.5f, 0.f}.normalized();
    fillLight.color = {1.f, 1.f, 1.f};
    fillLight.intensity = 0.5f;
    scene.lights.add(fillLight);

    Light pointLight;
    pointLight.type = LightType::Point;
    pointLight.color = {1.f, 0.95f, 0.85f};
    pointLight.intensity = 0.f; // off by default — flashlight
    pointLight.attConstant = 1.f;
    pointLight.attLinear = 0.22f;
    pointLight.attQuadratic = 0.2f;
    scene.lights.add(pointLight);

    // --- Shadow map (bigger extent to cover the full scene) ---
    scene.shadowMap.width = 1024;
    scene.shadowMap.height = 1024;
    scene.shadowMap.bias = 0.012f;
    scene.shadowMap.setup(keyLight.direction, {0.f, 0.f, 0.f}, 12.f, 0.1f, 40.f);

    // =======================================================================
    // Resources — owned by main(), referenced by entities via pointers
    // =======================================================================

    // Textures
    Texture centerAlbedo;
    Texture centerNormal; // Added for Normal Mapping
    Texture blockAlbedo;

    try
    {
        centerAlbedo.load("assets/textures/gabagool.jpg");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Texture: " << e.what() << "\n";
    }

    try
    {
        centerNormal.load("assets/textures/cube_normal.png");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Texture: " << e.what() << "\n";
    }

    try
    {
        blockAlbedo.load("assets/textures/gospelv1.png");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Texture: " << e.what() << "\n";
    }

    // --- Shaders (each instance = a unique material definition) ---

    // Centerpiece — textured
    PBRShader centerPBR;
    centerPBR.albedo = {1.f, 1.f, 1.f};
    centerPBR.metallic = 0.3f;
    centerPBR.roughness = 0.5f;
    centerPBR.ao = 1.f;
    centerPBR.albedoMap = centerAlbedo.loaded ? &centerAlbedo : nullptr;
    centerPBR.normalMap = centerNormal.loaded ? &centerNormal : nullptr; // Assign normal map

    PBRShader blockPBR;
    blockPBR.albedo = {0.8f, 0.2f, 0.2f};
    blockPBR.metallic = 0.0f;
    blockPBR.roughness = 0.9f;
    blockPBR.ao = 1.f;
    blockPBR.albedoMap = blockAlbedo.loaded ? &blockAlbedo : nullptr;

    // Ground
    PBRShader groundPBR;
    groundPBR.albedo = {0.4f, 0.38f, 0.35f};
    groundPBR.metallic = 0.f;
    groundPBR.roughness = 0.85f;
    groundPBR.ao = 1.f;

    // Pillars — dark stone
    PBRShader pillarPBR;
    pillarPBR.albedo = {0.35f, 0.3f, 0.3f};
    pillarPBR.metallic = 0.0f;
    pillarPBR.roughness = 0.7f;
    pillarPBR.ao = 1.f;

    // PBR showcase orbiters
    PBRShader goldPBR;
    goldPBR.albedo = {1.f, 0.85f, 0.4f};
    goldPBR.metallic = 1.0f;
    goldPBR.roughness = 0.15f;
    goldPBR.ao = 1.f;

    PBRShader plasticPBR;
    plasticPBR.albedo = {0.1f, 0.5f, 0.9f};
    plasticPBR.metallic = 0.0f;
    plasticPBR.roughness = 0.1f;
    plasticPBR.ao = 1.f;

    PBRShader mattePBR;
    mattePBR.albedo = {0.8f, 0.2f, 0.2f};
    mattePBR.metallic = 0.0f;
    mattePBR.roughness = 0.9f;
    mattePBR.ao = 1.f;

    // --- Materials ---
    Material centerMat = {&centerPBR, CullMode::Back, true};
    Material blockMat = {&blockPBR, CullMode::Back, true};
    Material groundMat = {&groundPBR, CullMode::None, false};
    Material pillarMat = {&pillarPBR, CullMode::Back, true};
    Material matGold = {&goldPBR, CullMode::Back, true};
    Material matPlastic = {&plasticPBR, CullMode::Back, true};
    Material matMatte = {&mattePBR, CullMode::Back, true};

    // --- Meshes ---
    Mesh cubeMesh, groundMesh, blockMesh;
    bool hasCube = false, hasGround = false, hasBlock = false;
    try
    {
        cubeMesh = loadOBJ("assets/models/cube.obj");
        hasCube = true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Cube OBJ: " << e.what() << "\n";
    }
    try
    {
        groundMesh = loadOBJ("assets/models/ground.obj");
        hasGround = true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Ground OBJ: " << e.what() << "\n";
    }
    try
    {
        blockMesh = loadOBJ("assets/models/cube.obj");
        hasBlock = true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Ground OBJ: " << e.what() << "\n";
    }

    std::cout << "hasCube=" << hasCube << " verts=" << cubeMesh.vertices.size() << "\n";
    std::cout << "hasGround=" << hasGround << " verts=" << groundMesh.vertices.size() << "\n";

    if (!hasCube || !hasGround)
    {
        std::cerr << "Cannot run without meshes.\n";
        return 1;
    }

    // =======================================================================
    // Entities — build the scene
    // =======================================================================

    // 0. Ground
    {
        Entity e;
        e.name = "ground";
        e.mesh = &groundMesh;
        e.material = &groundMat;
        e.transform.scaleVec = {2.f, 1.f, 2.f};
        scene.entities.push_back(e);
    }

    // 1. Centerpiece (rotating textured cube)
    {
        Entity e;
        e.name = "center";
        e.mesh = &cubeMesh;
        e.material = &centerMat;
        e.transform.position = {0.f, 1.5f, 0.f};
        scene.entities.push_back(e);
    }

    // 2-5. Four pillars
    {
        float pDist = 4.0f;
        Vec3 pillarPos[4] = {{pDist, 2.f, pDist}, {-pDist, 2.f, pDist}, {pDist, 2.f, -pDist}, {-pDist, 2.f, -pDist}};
        for (int i = 0; i < 4; ++i)
        {
            Entity e;
            e.name = "pillar_" + std::to_string(i);
            e.mesh = &cubeMesh;
            e.material = &pillarMat;
            e.transform.position = pillarPos[i];
            e.transform.scaleVec = {0.8f, 4.0f, 0.8f};
            scene.entities.push_back(e);
        }
    }

    // 6-8. Three PBR orbiter cubes
    {
        Material *orbitMats[3] = {&matGold, &matPlastic, &matMatte};
        for (int i = 0; i < 3; ++i)
        {
            Entity e;
            e.name = "orbiter_" + std::to_string(i);
            e.mesh = &cubeMesh;
            e.material = orbitMats[i];
            e.transform.scaleVec = {0.4f, 0.4f, 0.4f};
            scene.entities.push_back(e);
        }
    }

    // 9. Block
    {
        Entity e;
        e.name = "block";
        e.mesh = &blockMesh;
        e.material = &blockMat;
        e.transform.position = {0.f, 5.0f, 0.f};
        scene.entities.push_back(e);
    }

    std::cout << "Scene: " << scene.entities.size() << " entities\n";

    // =======================================================================
    // Game loop
    // =======================================================================
    float angle = 0.f;
    uint32_t last = SDL_GetTicks();

    while (window.pollEvents())
    {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.f;
        last = now;
        angle += 1.5f * dt;

        // --- Animate centerpiece ---
        scene.entities[1].transform.rotation.y = angle;
        scene.entities[1].transform.rotation.x = angle * 0.5f;

        // --- Animate the 3 PBR orbiters ---
        for (int i = 0; i < 3; ++i)
        {
            int idx = 6 + i;
            float offsetAngle = angle + (i * MathUtils::TWO_PI / 3.f);

            scene.entities[idx].transform.position = {
                std::cos(offsetAngle) * 2.5f, 1.5f + std::sin(angle * 2.f + i) * 0.5f, std::sin(offsetAngle) * 2.5f};

            scene.entities[idx].transform.rotation.x += 1.0f * dt;
            scene.entities[idx].transform.rotation.y += 1.5f * dt;
        }

        // --- Flashlight: attach point light to camera, toggle with F ---
        scene.lights.lights[2].position = scene.camera.position;
        const uint8_t *keys = SDL_GetKeyboardState(nullptr);
        scene.lights.lights[2].intensity = keys[SDL_SCANCODE_F] ? 5.0f : 0.0f;

        // --- Update camera from input ---
        const InputState &in = window.input();
        scene.camera.update(dt, in.w, in.s, in.a, in.d, in.e, in.q, in.shift, in.mouseDX, in.mouseDY);

        // --- Render ---
        renderer.render(scene);
        window.present(renderer.pixelData());
    }

    return 0;
}