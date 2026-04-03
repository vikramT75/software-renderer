#include "platform/sdl_window.h"
#include "core/renderer.h"
#include "scene/scene.h"
#include "scene/entity.h"
#include "scene/material.h"
#include "shading/pbr.h"
#include "shading/texture.h"
#include "loaders/obj_loader.h"
#include "math/math_utils.h"
#include <iostream>

int main(int argc, char *argv[])
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
    scene.camera.position = {0.f, 3.f, 6.f};
    scene.camera.yaw = -MathUtils::HALF_PI;
    scene.camera.pitch = MathUtils::toRadians(-20.f);
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
    pointLight.color = {1.f, 1.f, 1.f};
    pointLight.intensity = 0.f;
    pointLight.attConstant = 1.f;
    pointLight.attLinear = 0.5f;
    pointLight.attQuadratic = 0.8f;
    scene.lights.add(pointLight);

    // --- Shadow map ---
    scene.shadowMap.width = 1024;
    scene.shadowMap.height = 1024;
    scene.shadowMap.bias = 0.012f;
    scene.shadowMap.setup(keyLight.direction, {0.f, 0.f, 0.f}, 8.f, 0.1f, 30.f);

    // =======================================================================
    // Resources — owned by main(), referenced by entities via pointers
    // =======================================================================

    // Textures
    Texture cubeAlbedo;
    try
    {
        cubeAlbedo.load("assets/textures/cube.bmp");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Texture: " << e.what() << "\n";
    }

    // Shaders (hold material properties)
    PBRShader cubePBR;
    cubePBR.albedo = {1.f, 1.f, 1.f};
    cubePBR.metallic = 0.3f;
    cubePBR.roughness = 0.5f;
    cubePBR.ao = 1.f;
    cubePBR.albedoMap = cubeAlbedo.loaded ? &cubeAlbedo : nullptr;

    PBRShader groundPBR;
    groundPBR.albedo = {0.4f, 0.38f, 0.35f};
    groundPBR.metallic = 0.f;
    groundPBR.roughness = 0.85f;
    groundPBR.ao = 1.f;

    // Materials (wrap shaders + render state)
    Material cubeMat;
    cubeMat.shader = &cubePBR;
    cubeMat.cullMode = CullMode::Back;
    cubeMat.castsShadow = true;

    Material groundMat;
    groundMat.shader = &groundPBR;
    groundMat.cullMode = CullMode::None;
    groundMat.castsShadow = false;

    // Meshes
    Mesh cubeMesh, groundMesh;
    bool hasCube = false, hasGround = false;
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

    std::cout << "hasCube=" << hasCube << " verts=" << cubeMesh.vertices.size() << "\n";
    std::cout << "hasGround=" << hasGround << " verts=" << groundMesh.vertices.size() << "\n";

    // =======================================================================
    // Entities — bind mesh + material + transform
    // =======================================================================
    Entity cube;
    cube.name = "cube";
    cube.mesh = hasCube ? &cubeMesh : nullptr;
    cube.material = &cubeMat;
    cube.transform.position = {0.f, 1.f, 0.f};

    Entity ground;
    ground.name = "ground";
    ground.mesh = hasGround ? &groundMesh : nullptr;
    ground.material = &groundMat;
    // ground.transform is identity — default position

    scene.entities.push_back(cube);
    scene.entities.push_back(ground);

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

        // Animate cube
        angle += 1.5f * dt;
        scene.entities[0].transform.rotation.y = angle;
        scene.entities[0].transform.rotation.x = angle * 0.4f;

        // Orbit point light
        scene.lights.lights[2].position = {
            std::cos(angle * 1.3f) * 2.f,
            1.5f,
            std::sin(angle * 1.3f) * 2.f};

        // Update camera from input
        const InputState &in = window.input();
        scene.camera.update(dt,
                            in.w, in.s, in.a, in.d,
                            in.e, in.q, in.shift,
                            in.mouseDX, in.mouseDY);

        // One call renders the entire scene
        renderer.render(scene);
        window.present(renderer.pixelData());
    }

    return 0;
}