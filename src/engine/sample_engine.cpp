#include "sample_engine.h"

#include "framework/nodes/environment_3d.h"
#include "framework/parsers/parse_gltf.h"
#include "framework/parsers/parse_scene.h"

#include "graphics/sample_renderer.h"
#include "graphics/renderer_storage.h"

#include "engine/scene.h"

#include "shaders/mesh_grid.wgsl.gen.h"

#include <spdlog/spdlog.h> //for manual debugging
#include <framework/nodes/camera.h> // for camera movement
#include "framework/camera/free_camera.h"
#include "framework/input.h"
#include <filesystem>  // Required for creating directories


int SampleEngine::initialize(Renderer* renderer, sEngineConfiguration configuration)
{
    int error = Engine::initialize(renderer, configuration);

    if (error) return error;

    /*main_scene = new Scene("main_scene");
    return error;*/ // Commented out because now the scene is created in post_initialize
    return 0u;
}

int SampleEngine::post_initialize()
{
    int error = Engine::post_initialize();

    if (error) return error;

    // Create scene
    main_scene = new Scene("main_scene");


    // Create skybox
    {
        MeshInstance3D* skybox = new Environment3D();
        main_scene->add_node(skybox);
    }


    // Access the renderer's camera
    if (Renderer::instance) {
        Camera* camera = Renderer::instance->get_camera();
        if (camera) {
            glm::vec3 eye = glm::vec3(-5.87f, 2.11f, 2.41f);
            // glm::vec3 center = glm::vec3(-0.87f, 0.11f, 0.41f); // CAN'T SET CENTER HERE BECAUSE IT WILL BE OVERRIDDEN BY LOOK_AT IN CAMERA FILE

            camera->set_eye(eye);
            camera->update_view_matrix();
        }
    }


    // Create grid
    {
        MeshInstance3D* grid = new MeshInstance3D();
        grid->set_name("Grid");
        grid->add_surface(RendererStorage::get_surface("quad"));
        grid->set_position(glm::vec3(0.0f));
        grid->rotate(glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        grid->scale(glm::vec3(10.f));
        grid->set_frustum_culling_enabled(false);

        // NOTE: first set the transparency and all types BEFORE loading the shader
        Material* grid_material = new Material();
        grid_material->set_transparency_type(ALPHA_BLEND);
        grid_material->set_cull_type(CULL_NONE);
        grid_material->set_type(MATERIAL_UNLIT);
        grid_material->set_shader(RendererStorage::get_shader_from_source(shaders::mesh_grid::source, shaders::mesh_grid::path, shaders::mesh_grid::libraries, grid_material));
        grid->set_surface_material_override(grid->get_surface(0), grid_material);

        main_scene->add_node(grid);
    }
    
    // Create nodes
    {
        std::vector<Node*> parsed_entities;
        GltfParser parser;
    
        parser.parse("data/meshes/bistro.glb", parsed_entities);

        main_scene->add_node(static_cast<MeshInstance3D*>(parsed_entities[0]));
    }

    return 0u;
}

void SampleEngine::clean()
{
    Engine::clean();
}

void SampleEngine::update(float delta_time)
{
    Engine::update(delta_time);

    FreeCamera* camera = dynamic_cast<FreeCamera*>(Renderer::instance->get_camera());
    if (camera) {
        // Save start position
        if (Input::is_key_pressed(GLFW_KEY_I)) {
            camera->start_eye = camera->get_eye();
            camera->start_center = camera->get_center();
            spdlog::info("Start position saved.");
        }

        // Save end position
        if (Input::is_key_pressed(GLFW_KEY_O)) {
            camera->end_eye = camera->get_eye();
            camera->end_center = camera->get_center();
            spdlog::info("End position saved.");
        }

        // Start animation when space is pressed
        if (Input::is_key_pressed(GLFW_KEY_SPACE) && camera->frame_counter == 0) {
            camera->total_frames = 50;  // Increase for smoother animation
            camera->frame_counter = 1;  // Start animation
            spdlog::info("Camera animation started.");
        }

        // Animate camera over multiple frames
        if (camera->frame_counter > 0 && camera->frame_counter <= camera->total_frames) {
            float t = static_cast<float>(camera->frame_counter) / static_cast<float>(camera->total_frames);
            glm::vec3 new_eye = glm::mix(camera->start_eye, camera->end_eye, t);
            glm::vec3 new_center = glm::mix(camera->start_center, camera->end_center, t);

            camera->set_eye(new_eye);
            camera->look_at(new_eye, new_center, glm::vec3(0.0f, 1.0f, 0.0f), false);
        }
    }

    main_scene->update(delta_time);
}


void SampleEngine::render()
{
    if (show_imgui) {
        render_default_gui();
    }

    main_scene->render();

    Renderer* renderer = Engine::get_renderer();
    if (!renderer) return;

    FreeCamera* camera = dynamic_cast<FreeCamera*>(renderer->get_camera());
    if (camera) {
        glm::vec3 pos = camera->get_eye();
        glm::vec3 center = camera->get_center();

        // Show Camera Info
        ImGui::Begin("Camera Info");
        ImGui::Text("Position: x = %.2f, y = %.2f, z = %.2f", pos.x, pos.y, pos.z);
        ImGui::Text("Center: x = %.2f, y = %.2f, z = %.2f", center.x, center.y, center.z);
        ImGui::End();
    }

    // Save frames if animation is running
    if (camera && camera->frame_counter > 0 && camera->frame_counter <= camera->total_frames) {
        // Ensure "frames/" folder exists
        std::filesystem::create_directory("frames");

        // Save frames inside the "frames/" directory
        std::string filename = "frames/frame_" + std::to_string(camera->frame_counter) + ".ppm";

        // Get screen texture
        WGPUSurfaceTexture screen_surface_texture;
        wgpuSurfaceGetCurrentTexture(renderer->get_webgpu_context()->surface, &screen_surface_texture);
        uint32_t width = renderer->get_webgpu_context()->screen_width;
        uint32_t height = renderer->get_webgpu_context()->screen_height;

        // Get command encoder
        WGPUCommandEncoder command_encoder = renderer->get_global_command_encoder();

        if (screen_surface_texture.texture) {
            renderer->store_texture_to_disk(command_encoder, screen_surface_texture.texture, { width, height, 1u }, filename.c_str());
            spdlog::info("Saved frame: {}", filename);
        }
        else {
            spdlog::error("Failed to save frame {}: Invalid screen texture!", camera->frame_counter);
        }

        camera->frame_counter++;  // Move to the next frame

        if (camera->frame_counter > camera->total_frames) {
            camera->frame_counter = 0;  // Reset after reaching last frame
            spdlog::info("Camera animation finished.");
        }

    }

    Engine::render();
}
