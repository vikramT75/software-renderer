#include "platform/sdl_window.h"
#include "core/renderer.h"
#include "core/shadow_map.h"
#include "scene/camera.h"
#include "scene/transform.h"
#include "scene/light.h"
#include "math/math_utils.h"
#include "loaders/obj_loader.h"
#include "shading/pbr.h"
#include "shading/texture.h"
#include <iostream>

int main(int argc, char *argv[])
{
    const int WIDTH = 800;
    const int HEIGHT = 600;

    SDLWindow window("swr — software renderer", WIDTH, HEIGHT);
    Renderer renderer(WIDTH, HEIGHT);

    Camera camera;
    camera.position = {0.f, 3.f, 6.f};
    camera.yaw = -MathUtils::HALF_PI;
    camera.pitch = MathUtils::toRadians(-20.f);
    camera.fovY = MathUtils::toRadians(60.f);
    camera.aspect = static_cast<float>(WIDTH) / HEIGHT;
    camera.zNear = 0.1f;
    camera.zFar = 100.f;
    camera.moveSpeed = 3.f;
    camera.lookSpeed = 0.003f;

    // -----------------------------------------------------------------------
    // Lights
    // -----------------------------------------------------------------------
    LightList lightList;

    Light keyLight;
    keyLight.type = LightType::Directional;
    keyLight.direction = Vec3{-1.f, -1.f, -1.f}.normalized();
    keyLight.color = {1.f, 1.f, 1.f};
    keyLight.intensity = 2.f;
    lightList.add(keyLight);

    Light fillLight;
    fillLight.type = LightType::Directional;
    fillLight.direction = Vec3{1.f, -0.5f, 0.f}.normalized();
    fillLight.color = {1.f, 1.f, 1.f};
    fillLight.intensity = 0.5f;
    lightList.add(fillLight);

    Light pointLight;
    pointLight.type = LightType::Point;
    pointLight.color = {1.f, 1.f, 1.f};
    pointLight.intensity = 0.f;
    pointLight.attConstant = 1.f;
    pointLight.attLinear = 0.5f;
    pointLight.attQuadratic = 0.8f;
    lightList.add(pointLight);

    // -----------------------------------------------------------------------
    // Shadow map — key light only
    // -----------------------------------------------------------------------
    ShadowMap shadowMap;
    shadowMap.width = 1024;
    shadowMap.height = 1024;
    shadowMap.bias = 0.012f;
    shadowMap.setup(keyLight.direction, {0.f, 0.f, 0.f}, 8.f, 0.1f, 30.f);

    // -----------------------------------------------------------------------
    // Textures
    // -----------------------------------------------------------------------
    Texture cubeAlbedo;
    try
    {
        cubeAlbedo.load("assets/textures/cube.bmp");
    }
    catch (const std::exception &e)
    {
        std::cerr << "Texture: " << e.what() << "\n";
    }

    // -----------------------------------------------------------------------
    // Shaders
    // -----------------------------------------------------------------------
    PBRShader cubePBR;
    cubePBR.lights = &lightList;
    cubePBR.shadowMap = &shadowMap;
    cubePBR.albedo = {1.f, 1.f, 1.f};
    cubePBR.metallic = 0.3f;
    cubePBR.roughness = 0.5f;
    cubePBR.ao = 1.f;
    cubePBR.albedoMap = cubeAlbedo.loaded ? &cubeAlbedo : nullptr;

    PBRShader groundPBR;
    groundPBR.lights = &lightList;
    groundPBR.shadowMap = &shadowMap;
    groundPBR.albedo = {0.4f, 0.38f, 0.35f};
    groundPBR.metallic = 0.f;
    groundPBR.roughness = 0.85f;
    groundPBR.ao = 1.f;

    // -----------------------------------------------------------------------
    // Meshes
    // -----------------------------------------------------------------------
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

    // Cube sits on ground — original cube.obj verts in [0,1] so y=0 puts bottom on ground
    Transform cubeTransform;
    cubeTransform.position = {0.f, 1.f, 0.f};

    Transform groundTransform; // identity

    float angle = 0.f;
    uint32_t last = SDL_GetTicks();

    while (window.pollEvents())
    {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.f;
        last = now;

        angle += 1.5f * dt;
        cubeTransform.rotation.y = angle;
        cubeTransform.rotation.x = angle * 0.4f;

        // Orbit point light above ground
        lightList.lights[2].position = {
            std::cos(angle * 1.3f) * 2.f,
            1.5f,
            std::sin(angle * 1.3f) * 2.f};

        const InputState &in = window.input();
        camera.update(dt,
                      in.w, in.s, in.a, in.d,
                      in.e, in.q, in.shift,
                      in.mouseDX, in.mouseDY);

        cubePBR.cameraPos = camera.position;
        groundPBR.cameraPos = camera.position;

        Mat4 cubeModel = cubeTransform.matrix();
        Mat4 groundModel = groundTransform.matrix();
        Mat4 view = camera.view();
        Mat4 proj = camera.projection();

        // -------------------------------------------------------------------
        // Pass 1 — shadow pass
        // -------------------------------------------------------------------
        renderer.setModel(cubeModel);
        renderer.beginShadowPass(shadowMap);
        if (hasCube)
            renderer.drawTriangles(cubeMesh.vertices, cubeMesh.indices, 0);
        renderer.endShadowPass();

        // -------------------------------------------------------------------
        // Pass 2 — main pass
        // -------------------------------------------------------------------
        renderer.beginFrame(0xFF1a1a2e);

        // Cube
        renderer.cullMode = CullMode::Back;
        renderer.activeShader = &cubePBR;
        renderer.setMVP(cubeModel, view, proj);
        if (hasCube)
            renderer.drawTriangles(cubeMesh.vertices, cubeMesh.indices, 0);

        // Ground — no culling so winding doesn't matter
        renderer.cullMode = CullMode::None;
        renderer.activeShader = &groundPBR;
        renderer.setMVP(groundModel, view, proj);
        if (hasGround)
            renderer.drawTriangles(groundMesh.vertices, groundMesh.indices, 0);
        renderer.cullMode = CullMode::Back;

        renderer.endFrame();
        window.present(renderer.pixelData());
    }

    return 0;
}