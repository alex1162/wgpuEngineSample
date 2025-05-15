#include "sample_engine.h"

#include "framework/nodes/environment_3d.h"
#include "framework/parsers/parse_gltf.h"
#include "framework/parsers/parse_scene.h"

#include "graphics/sample_renderer.h"
#include "graphics/renderer_storage.h"

#include "engine/scene.h"

#include "shaders/mesh_grid.wgsl.gen.h"
#include "shaders/mesh_grid_deferred.wgsl.gen.h"

#include <spdlog/spdlog.h> //for manual debugging
#include <framework/nodes/camera.h> // for camera movement
#include "framework/input.h"
#include <filesystem>  // Required for creating directories
#include <fstream>  // for std::ofstream

float halton(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0) {
        f /= static_cast<float>(base);
        r += f * (index % base);
        index /= base;
    }
    return r;
}

int SampleEngine::initialize(Renderer* renderer, sEngineConfiguration configuration)
{
    int error = Engine::initialize(renderer, configuration);

    screen_width = configuration.window_width;
    screen_height = configuration.window_height;

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

        Camera3D* camera = dynamic_cast<Camera3D*>(Renderer::instance->get_camera());
        if (!camera) {
            spdlog::error("Camera not found!");
            return 1;
        }

        if (camera) {
            glm::vec3 eye = glm::vec3(-5.87f, 2.11f, 2.41f);
            glm::vec3 center = glm::vec3(-4.88f, 1.97f, 2.68f); // CAN'T SET CENTER HERE BECAUSE IT WILL BE OVERRIDDEN BY LOOK_AT IN CAMERA FILE

            camera->set_eye(eye);
            camera->look_at(eye, center, glm::vec3(0.0f, 1.0f, 0.0f), true);
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
        grid_material->set_if_is_deferred(true);
        grid_material->set_shader(RendererStorage::get_shader_from_source(shaders::mesh_grid_deferred::source, shaders::mesh_grid_deferred::path, shaders::mesh_grid_deferred::libraries, grid_material));
        grid->set_surface_material_override(grid->get_surface(0), grid_material);

        main_scene->add_node(grid);
    }
    
    // Load model
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

    Camera3D* camera = dynamic_cast<Camera3D*>(Renderer::instance->get_camera());
    if (camera) {

        // GENERAL CAMERA INFORMATION
        // start position
        glm::vec3 start_eye = glm::vec3(-5.87f, 2.11f, 2.66f);
        glm::vec3 start_center = glm::vec3(-4.88f, 1.97f, 2.68f);

        /*camera->set_eye(start_eye);
        camera->look_at(start_eye, start_center, glm::vec3(0.0f, 1.0f, 0.0f), false);*/


        // end position
        glm::vec3 end_eye = glm::vec3(-5.49f, 2.05f, 2.87f);
        glm::vec3 end_center = glm::vec3(-4.50f, 1.92f, 2.89f);

        total_frames = 25;

        // Start 1st sequence 
        if (Input::is_key_pressed(GLFW_KEY_1) && frame_counter == 0) {
            frame_counter = 1;
            seq = 1;  // Running seq native res
            spdlog::info("First animation started.");

        }

        // Start sequence MSAA (press M)
        if (Input::is_key_pressed(GLFW_KEY_2) && frame_counter == 0) {
            frame_counter = 1;
            seq = 2;  // Running seq MSAA
            spdlog::info("Second animation started.");

        }

        // Animate camera over multiple frames
        if (frame_counter > 0 && frame_counter <= total_frames) {

            // linear interpolation(LERP) using interpolation factor t (0,1) for going from (A,B)
            float t = static_cast<float>(frame_counter) / static_cast<float>(total_frames);
            glm::vec3 new_eye = glm::mix(start_eye, end_eye, t);  
            glm::vec3 new_center = glm::mix(start_center, end_center, t);

            camera->set_eye(new_eye);
            camera->look_at(new_eye, new_center, glm::vec3(0.0f, 1.0f, 0.0f), true);

            static std::vector<std::string> output_names;
            output_names.push_back("results_txt/frame" + std::to_string(frame_counter) + "_GBuffer_");
            renderer->set_store_gbuffers(output_names.back().c_str());

            spdlog::info("Frame " + std::to_string(frame_counter) + " stored");
     
            // generate Halton jitter 
            float jitter_x = (halton(frame_counter, 2) - 0.5f);
            float jitter_y = (halton(frame_counter, 3) - 0.5f);

            // save jitter values to a file
            std::ofstream jitter_file("results_txt/frame" + std::to_string(frame_counter) + "_jitter.txt");
            jitter_file << jitter_x << " " << jitter_y << "\n";
            jitter_file.close();

            // scale jitter to screen resolution: Normalized Device Coordinates (-1, 1)
            float jitter_scale_x = 2.0f / static_cast<float>(screen_width);
            float jitter_scale_y = 2.0f / static_cast<float>(screen_height);

            // apply jitter to camera projection
            glm::mat4 proj = camera->get_projection();
            proj[2][0] += jitter_x * jitter_scale_x;
            proj[2][1] += jitter_y * jitter_scale_y;

            camera->set_jittered_projection(proj);

            frame_counter++;
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

    /*
    Camera* camera = dynamic_cast<Camera*>(renderer->get_camera());
    if (camera) {
        //glm::vec3 pos = camera->get_eye();
        //glm::vec3 center = camera->get_center();

        //ImGui::Begin("Camera Info");
        //ImGui::Text("Position: x = %.2f, y = %.2f, z = %.2f", pos.x, pos.y, pos.z);
        //ImGui::Text("Center: x = %.2f, y = %.2f, z = %.2f", center.x, center.y, center.z);
        //ImGui::End();
    }

    // Save frames for active animation sequence
    if (camera && frame_counter > 0 && frame_counter <= total_frames) {

        std::string folder = "frames";

        if (seq == 1) {
            folder = "frames/sequence1";
        }
        else if (seq == 2) {
            folder = "frames/sequence2";
        }

        std::filesystem::create_directories(folder);

        std::string filename = folder + "/frame_" + std::to_string(frame_counter) + ".ppm";

        WGPUSurfaceTexture screen_surface_texture;
        wgpuSurfaceGetCurrentTexture(renderer->get_webgpu_context()->surface, &screen_surface_texture);
        uint32_t width = renderer->get_webgpu_context()->screen_width;
        uint32_t height = renderer->get_webgpu_context()->screen_height;

        WGPUCommandEncoder command_encoder = renderer->get_global_command_encoder();

        if (screen_surface_texture.texture) {
            renderer->store_texture_to_disk(command_encoder, screen_surface_texture.texture, { width, height, 1u }, filename.c_str());
            spdlog::info("Saved frame: {}", filename);
        }
        else {
            spdlog::error("Failed to save frame {}: Invalid screen texture!", frame_counter);
        }

        frame_counter++;

        if (frame_counter > total_frames) {
            frame_counter = 0;  // Reset after animation finishes
            seq = 0; // No active sequence
            spdlog::info("Sequence finished.");
        }
    }*/

    Engine::render();
}

