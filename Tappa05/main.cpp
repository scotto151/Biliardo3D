#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"
#include "../include/mesh.hh"
#include "../include/matrices.hh"
#include "../include/multishaders.hh"
#include "../include/trackball.hh"
#include <SFML/Window.hpp>
#include <iostream>
#include <optional>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>


namespace game {
    constexpr float ball_radius = 0.0256f;
    constexpr float table_length = 1.0f;
    constexpr float table_width = 0.5f;
    constexpr float cloth_height = 0.02f;
    constexpr float rail_width = 0.06f;
    constexpr float rail_height = 0.056f;
    constexpr float leg_height = 0.28f;
    constexpr float leg_width = 0.070f;
    constexpr float table_height = 0.08f;
    constexpr float table_friction = 0.6f;
    constexpr float min_speed = 0.01f;
    constexpr float x_boundary = table_length * 0.5f - ball_radius;
    constexpr float z_boundary = table_width * 0.5f - ball_radius;
}


/////////
//Setup//
/////////

class Setup
{
public:
    static const int window_width = 800;
    static const int window_height = 800;

    sf::Window* window;

    Setup ()
    {
        sf::ContextSettings settings;
        settings.depthBits = 32;
        settings.stencilBits = 8;
        settings.antiAliasingLevel = 4;
        settings.attributeFlags = sf::ContextSettings::Attribute::Core;
        settings.majorVersion = 4;
        settings.minorVersion = 1;

        window = new sf::Window (
                                 sf::VideoMode({window_width, window_height}),
                                 "Biliardo3D",
                                 sf::Style::Default,
                                 sf::State::Windowed,
                                 settings
                                 );
        window->setVerticalSyncEnabled (true);

        if (!window->setActive (true)) {
            std::cerr << "Failure: error during SFML OpenGL Activation." << std::endl;
            exit (1);
        }
        sf::ContextSettings gotten = window->getSettings ();

        std::cout << "depth bits: " << gotten.depthBits << std::endl;
        std::cout << "stencil bits: " << gotten.stencilBits << std::endl;
        std::cout << "antialiasing level: " << gotten.antiAliasingLevel << std::endl;
        std::cout << "SFML GL version: " << gotten.majorVersion << "." << gotten.minorVersion << std::endl;

        int version = gladLoadGL (sf::Context::getFunction);
        if (!version) {
            std::cerr << "Failure: error during glad loading." << std::endl;
            exit (1);
        }
        std::cout << "GLAD GL version: " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << std::endl;
    }

    ~Setup ()
    {
        delete window;
    }
};

class Lights
{
    public:
        glm::vec3 light_direct_pos = {0.0f ,0.5f ,0.0f};
        glm::vec3 light_direct_val = {1.0f ,1.0f ,1.0f};
        glm::vec3 light_ambient_val = {0.35f ,0.35f ,0.35f};
    private:
        fcg::Shaders* shaders = nullptr;
    public:
        Lights (fcg::Shaders& shaders) : shaders(&shaders) { }
        
        void send_parameters()
        {
            shaders->set("light.direct_pos",light_direct_pos);
            shaders->set("light.direct_val", light_direct_val);
            shaders->set("light.ambient_val",light_ambient_val);
        }
};

struct Material
{
    glm::vec3 diffuse;
    glm::vec3 ambient;
    glm::vec3 specular;
    float shininess;
};

class Camera
{
public:
    glm::mat4 v;
    glm::mat4 inv_v;
    glm::mat4 vp;

private:
    /** Intrinsic camera parameters **/
    const float normal_fd = 50.0 / 18.0; 
    const float tele_fd =  400.0 / 18.0;
    const float wide_fd = 24 / 18.0;
    float fd; // focal distance
    float ar; // aspect ratio

    /** Extrinsic camera parameters **/
    // xyz, camera position (fixed in world coordinates)
    glm::vec3 camera_pos = {0.0, 0.0, 0.0};
    // The Trackball contains the object rotation relative to the fixed camera position
    fcg::Trackball trackball;
    // object distance, relative to the fixed camera position
    float od;
    fcg::Shaders* shaders = nullptr;

public:
    Camera (fcg::Shaders& shaders) :
    shaders(&shaders)
    {
        
        lens_normal ();
        set_window_size (Setup::window_width, Setup::window_height);        
        view_projection ();
    }

    void set_window_size (int w, int h)
    {
        trackball.set_window_size (w, h);
        ar = ((float) w) / (float) h;
        trackball.set_view (od, 1.0f / (fd * ar));
        view_projection ();
    }

    void start_rotate (float x, float y)
    {
        trackball.start (x, y);
    }

    void stop_rotate ()
    {
        trackball.stop ();
    }

    bool rotate (float x, float y)
    {
        bool moved = trackball.move (x, y);
        if (moved)
            view_projection ();
        return moved;
    }

    void zoom (float dy)
    {
        float ratio = fd / 100.0;
        fd += dy * ratio;
        if (fd < 0.1)
            fd = 0.1;
        trackball.set_view (od, 1.0f / (fd * ar));
        view_projection ();
    }

    void distance (float dy)
    {
        float ratio = od / 100.0;
        od -= dy * ratio; // note: we go in the opposite direction of zoooming
        if (od < 0.5)
            od = 0.5;
        trackball.set_view (od, 1.0f / (fd * ar));
        view_projection ();
    }

    void lens_tele ()
    {
        fd = tele_fd;
        od = tele_fd;
        trackball.set_view (od, 1.0f / (fd * ar));
        view_projection ();
    }

    void lens_normal ()
    {
        fd = normal_fd;
        od = normal_fd * 0.8;
        trackball.set_view (od, 1.0f / (fd * ar));
        view_projection ();
    }

    void lens_wide ()
    {
        fd = wide_fd;
        od = wide_fd;
        trackball.set_view (od, 1.0f / (fd * ar));
        view_projection ();
    }

    void view_projection ()
    {

        float ncp = od - 4.0; // distance near clip plane
        if (ncp < 0.0001)
            ncp = 0.0001;
        float fcp = od + 4.0; // distance far clip plane

        // rotation matrix from trackball
        glm::mat4 r = trackball.rotation_matrix ();

        // prepare translation matrix
        glm::mat4 tz = fcg::translation (0.0, 0.0, -od);

        // prepare projection matrix
        float a = (fcp + ncp) / (ncp - fcp);       // coefficient 3rd col
        float b = 2.0 * fcp * ncp / (ncp - fcp);   // coefficient 4th col

        glm::mat4 pr = glm::mat4(
                                 fd,  0.0,     0.0,  0.0,    // 1st column
                                 0.0, fd * ar, 0.0,  0.0,    // 2nd column
                                 0.0, 0.0,       a, -1.0,    // 3rd column
                                 0.0, 0.0,       b,  0.0     // 4th column
                                 );

        // Compute VP matrix and update it
        v = tz * r;
        vp = pr * v;
        inv_v = glm::inverse (v);

        glm::vec4 cp4 = {0.0, 0.0, 0.0, 1.0};
        cp4 = inv_v * cp4;
        camera_pos = {cp4.x,cp4.y,cp4.z};
        send_position();
    }

    void send_position()
    {
        shaders->set("camera_pos", camera_pos);
    }

    void set_initial_view(float x_deg, float y_deg)
    {
        glm::quat x = glm::angleAxis(glm::radians(x_deg), glm::vec3(1.0f,0.0f,0.0f));
        glm::quat y = glm::angleAxis(glm::radians(y_deg), glm::vec3(0.0f,1.0f,0.0f));
        trackball.set_rotation(x * y);
        view_projection();
    }
};


class GPUMesh
{
public:
    glm::vec3 min_bounds;
    glm::vec3 max_bounds;
    glm::vec3 center;
    glm::vec3 extent;
    float span;
    glm::mat4 to_unit_extent; // normalization model matrix
    glm::vec3 unit_center;
    glm::vec3 unit_extent;
    float unit_span;

private:
    std::vector<float> points = {};
    std::vector<unsigned int> indices = {};

    GLuint vbo;
    GLuint ebo;
    GLuint vao;
    bool initialized = false;

public:
    GPUMesh (std::string filename){ load (filename); }

    ~GPUMesh () { clean (); }

    void load (std::string filename)
    {
        fcg::Mesh mesh (filename);
        mesh.pack4gpu (points, indices);
        send_arrays_2a3f ();

        min_bounds = mesh.min_bounds;
        max_bounds = mesh.max_bounds;
        center = (min_bounds + max_bounds) * 0.5f;
        span = glm::distance (max_bounds, min_bounds);
        extent = max_bounds - min_bounds;

        std::cout <<"MESH: "<< filename << "\n";
        std::cout <<"(original) center, extent, span:" << "\n";
        std::cout << center.x <<" "<< center.y <<" "<< center.z << "\n";
        std::cout << extent.x <<" "<< extent.y <<" "<< extent.z << "\n";
        std::cout << span << "\n";

        to_unit_extent =
            fcg::scaling (1.0 / glm::compMax (extent)) *
            fcg::translation (-center);

        unit_center = {0.0, 0.0, 0.0};
        unit_span = glm::distance (extent, {0.0, 0.0, 0.0});
        unit_extent = extent / glm::compMax (extent);

        std::cout <<"(unit normalized) center, extent, span:" << "\n";
        std::cout << unit_center.x <<" "<< unit_center.y <<" "<< unit_center.z << "\n";
        std::cout << unit_extent.x <<" "<< unit_extent.y <<" "<< unit_extent.z << "\n";
        std::cout << unit_span << "\n\n";

        initialized = true;
    }

    void clean ()
    {
        if (initialized) {
            glDeleteVertexArrays (1, &vao);
            glDeleteBuffers (1, &vbo);
        }
    }

    void draw ()
    {
        glBindVertexArray (vao);
        glDrawElements(GL_TRIANGLES, indices.size (), GL_UNSIGNED_INT, 0);
    }

protected:
    // send to the gpu the mesh arrays:
    // - the mesh vertices, 2 attributes, 3 floats each
    // - the mesh indices
    void send_arrays_2a3f ()
    {
        // we want just one buffer, and we retrieve the name OpenGL assigns to it.
        glGenBuffers (1, &vbo);
        // bind it as the current VBO
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        // transfer data from CPU RAM to GPU RAM.
        glBufferData (GL_ARRAY_BUFFER,
                      points.size () * sizeof (float),
                      points.data (),
                      GL_STATIC_DRAW);

        // we want just one buffer container, and we retrieve the name OpenGL assigns to it.
        glGenVertexArrays (1, &vao);
        // bind it as the current vao.
        glBindVertexArray (vao);

        // Attribute 0: position (x, y, z)
        glVertexAttribPointer (0,
                               3,
                               GL_FLOAT,
                               GL_FALSE,
                               6 * sizeof(float),
                               (void*)0);
        glEnableVertexAttribArray (0);

        // Attribute 1: 3 generic floats (u, v, w)
        glVertexAttribPointer (1,
                               3,
                               GL_FLOAT,
                               GL_FALSE,
                               6 * sizeof(float),
                               (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray (1);

        glGenBuffers(1, &ebo); 
        // MUST be bound after the VAO's binding!
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size () * sizeof (unsigned int),
                     indices.data (),
                     GL_STATIC_DRAW);
    }
};

struct Ball{
    glm::vec3 pos;
    glm::vec3 vel;
    int number;
};

class Physics{
    public:
        std::vector<Ball> active_balls = {};
        
        Physics() 
        {
            setup_cue();
            setup_rack();
        }

        void update(float dt)
        {
            for(auto& b : active_balls){
                b.pos += b.vel * dt;
                b.vel *= 1.0f - game::table_friction * dt;
                if(std::abs(b.vel.x)<game::min_speed) b.vel.x = 0.0f;
                if(std::abs(b.vel.z)<game::min_speed) b.vel.z = 0.0f; 
            }

            detect_rail_collisions();
        }

        void detect_rail_collisions()
        {
            for(auto& b : active_balls)
            {
                if(b.pos.x > game::x_boundary){
                    b.vel.x *= -1.0f;
                    b.pos.x = game::x_boundary;
                }
                if(b.pos.x < -game::x_boundary){
                    b.vel.x *= -1.0f;
                    b.pos.x = -game::x_boundary;
                }
                if(b.pos.z > game::z_boundary){
                    b.vel.z *= -1.0f;
                    b.pos.z = game::z_boundary;
                }
                if(b.pos.z < -game::z_boundary){
                    b.vel.z *= -1.0f;
                    b.pos.z = -game::z_boundary;
                }
            }
        }

        void setup_cue()
        {
            active_balls.push_back(Ball{{- game::table_length * 0.25f, game::ball_radius, 0.0f},{0,0,0},0});
        }

        void setup_rack()
        {
            int k = 0;
            float x_init = game::table_length * 0.25f;
            for(int row = 0; row <5; row++){
                float x = x_init + (game::ball_radius * row) * std::sqrt(3.0f);
                for(int col = 0; col <= row; col++){
                    k++;
                    float z = (col - row * 0.5f) * 2.0f * game::ball_radius;
                    active_balls.push_back(Ball{{x,game::ball_radius,z}, {0,0,0},k}); 
                }
            }
            std::swap(active_balls[8].number,active_balls[5].number);
        }
};

class Scene
{
    private:
        fcg::Shaders* shaders = nullptr;
        std::string current_shader = "";
    public:
        Camera camera;
        Lights lights;
        GPUMesh sphere;
        GPUMesh cube;
        Physics physics;
        static constexpr glm::vec3 diffuse_balls[7] = {
            {0.92f, 0.72f, 0.08f},
            {0.08f, 0.22f, 0.62f},
            {0.85f, 0.15f, 0.15f},
            {0.32f, 0.13f, 0.42f},
            {0.88f, 0.42f, 0.04f},
            {0.08f, 0.42f, 0.18f},
            {0.42f, 0.09f, 0.13f}
        };

        Material table_mat = {{0.10f, 0.35f, 0.18f}, {0.03f, 0.09f, 0.05f}, {0.02f, 0.02f, 0.02f}, 4.0f};
        Material rail_mat = {{0.16f, 0.48f, 0.26f}, {0.05f, 0.14f, 0.08f}, {0.08f, 0.08f, 0.08f}, 12.0f};
        Material wood = {{0.30f, 0.18f, 0.10f}, {0.09f, 0.05f, 0.03f}, {0.15f, 0.15f, 0.15f}, 20.0f};

    public:
        Scene(fcg::Shaders& shaders) : 
            shaders(&shaders),
            camera(shaders),
            lights(shaders),
            sphere ("../Risorse/sphere.off"),
            cube ("../Risorse/cube.off")
        {
            camera.set_initial_view(30.0f, 90.0f);
            lights.send_parameters();
        }

        void draw ()
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            current_shader = "";

            draw_table(fcg::identity());
            glm::mat4 ball_scale = fcg::scaling(2.0f *game::ball_radius);
            for(const Ball&b : physics.active_balls){
                glm::mat4 ball_mm = fcg::translation(b.pos) * ball_scale;
                Material mat = get_material(b.number);
                draw_mesh(sphere, ball_mm, mat, "phong");
            }
        }

        void use_shader(const std::string& name)
        {
            if(name==current_shader) return;

            shaders->use(name);
            current_shader = name;

            shaders->set("vp", camera.vp);
            camera.send_position();
            lights.send_parameters();
        }

    private:
        void send_material(const Material& m)
        {
            shaders->set("material.diffuse", m.diffuse);
            shaders->set("material.ambient", m.ambient);
            shaders->set("material.specular", m.specular);
            shaders->set("material.shininess", m.shininess);
        }

        Material get_material(int ball_number)
        {
            glm::vec3 diffuse;
            if(ball_number==0){
                diffuse = {0.92f, 0.90f, 0.85f};
            }
            else if(ball_number==8){
                diffuse = {0.05f, 0.05f, 0.05f};
            }
            else{
                int index = (ball_number % 8) - 1;
                diffuse = diffuse_balls[index];
            }
            return {diffuse, diffuse * 0.25f, {1.0f,1.0f,1.0f}, 80.0f};
        }

        void draw_mesh(GPUMesh& mesh, const glm::mat4& parent_mm, const Material& m, const std::string& shader_name)
        {
            use_shader(shader_name);

            glm::mat4 mm = parent_mm * mesh.to_unit_extent;
            glm::mat3 ti_mm = glm::transpose(glm::inverse(glm::mat3 (mm)));
            shaders->set("model", mm);
            shaders->set("tr_inv_model", ti_mm);
            send_material(m);

            mesh.draw();
        }

        void draw_table(glm::mat4 parent_mm)
        {
            glm::mat4 scale, translate, mm;

            scale = fcg::scaling(game::table_length, game::cloth_height, game::table_width);
            translate = fcg::translation(0.0f, -game::cloth_height * 0.5f, 0.0f);
            mm = parent_mm * translate * scale;
            draw_mesh(cube, mm, table_mat,"flat");

            scale = fcg::scaling(game::table_length + 2.0f * game::rail_width,game::rail_height + game::cloth_height,game::rail_width);
            translate = fcg::translation(0.0f, (game::rail_height - game::cloth_height) * 0.5f, -(game::table_width + game::rail_width)*0.5f);
            mm = parent_mm * translate * scale;
            draw_mesh(cube, mm, rail_mat,"flat");

            translate = fcg::translation(0.0f, (game::rail_height - game::cloth_height) * 0.5f, (game::table_width + game::rail_width)*0.5f);
            mm = parent_mm * translate * scale;
            draw_mesh(cube, mm, rail_mat,"flat");

            scale = fcg::scaling(game::rail_width, game::rail_height + game::cloth_height, game::table_width);
            translate = fcg::translation(-(game::table_length + game::rail_width) * 0.5f, (game::rail_height - game::cloth_height) * 0.5f, 0.0f);
            mm = parent_mm * translate * scale;
            draw_mesh(cube, mm, rail_mat,"flat");

            translate = fcg::translation((game::table_length + game::rail_width) * 0.5f, (game::rail_height - game::cloth_height) * 0.5f, 0.0f);
            mm = parent_mm * translate * scale;
            draw_mesh(cube, mm, rail_mat,"flat");

            scale = fcg::scaling(game::table_length + game::rail_width * 2.0f, game::table_height, game::table_width + game::rail_width * 2.0f);
            translate = fcg::translation(0.0f, -game::cloth_height - game:: table_height * 0.5f, 0.0f);
            mm = parent_mm * translate * scale;
            draw_mesh(cube, mm, wood, "flat");

            scale = fcg::scaling(game::leg_width, game::leg_height, game::leg_width);
            float leg_x = game::table_length * 0.5f - game::leg_width;
            float leg_y = -(game::cloth_height + game::table_height + game::leg_height * 0.5f);
            float leg_z = game::table_width * 0.5f - game::leg_width;
            for(float sign_x : {-1.0f, 1.0f}){
                for(float sign_z : {-1.0f, 1.0f}){
                    translate = fcg::translation(sign_x * leg_x, leg_y, sign_z * leg_z);
                    mm = parent_mm * translate * scale;
                    draw_mesh(cube, mm, wood, "flat");
                }
            }

        }
};





//////////////////
//SFML Callbacks//
//////////////////

void handle(const sf::Event::Resized& resized, Camera& camera)
{
    glViewport(0,0,resized.size.x,resized.size.y);
    camera.set_window_size (resized.size.x,resized.size.y);
}

void handle(const sf::Event::KeyPressed& key_pressed)
{
    switch (key_pressed.scancode){
    case sf::Keyboard::Scancode::Escape:
        exit (0);
    default:
        return;
    }
}

void handle (const sf::Event::MouseButtonPressed& mouse_pressed, Camera& camera)
{
    if (mouse_pressed.button == sf::Mouse::Button::Right) {
        float x = mouse_pressed.position.x;
        float y = mouse_pressed.position.y;
        camera.start_rotate (x, y);
    }
}

void handle (const sf::Event::MouseButtonReleased& mouse_released, Camera& camera)
{
    if (mouse_released.button == sf::Mouse::Button::Right)
        camera.stop_rotate ();
}

void handle (const sf::Event::MouseMoved& mouse_moved, Scene& scene)
{
    float x = mouse_moved.position.x;
    float y = mouse_moved.position.y;

    static float prev_y = 0;
    float dy = y - prev_y; 
    prev_y = y;

    if (scene.camera.rotate (x, y))
        return;
    else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)) {
        scene.camera.zoom (dy);
    }
    else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LAlt)) {
        scene.camera.distance (dy);
    }
}

////////
//Main//
////////

int main(int argc, char* argv[])
{
    Setup setup;
    sf::Window& window = *setup.window;

    fcg::Shaders shaders;
    shaders.add("phong","../Tappa04/vertex_shader.vert","../Tappa05/fragment_phong.frag");
    shaders.add("flat","../Tappa04/vertex_shader.vert","../Tappa05/fragment_flat.frag");
    shaders.use("flat");
    Scene scene (shaders);

    glEnable (GL_CULL_FACE);
    glCullFace (GL_BACK);
    glEnable (GL_DEPTH_TEST);

    bool running = true;
    sf::Clock clock;
    while (running)
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>()) {
                running = false;
            }
            else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
                handle(*resized,scene.camera);
            }
            else if (const auto* key_pressed = event->getIf<sf::Event::KeyPressed> ()){
                handle (*key_pressed);
            }
            else if(const auto* mouse_pressed = event->getIf<sf::Event::MouseButtonPressed> ()){
                handle (*mouse_pressed, scene.camera);
            }
            else if(const auto* mouse_released = event->getIf<sf::Event::MouseButtonReleased> ()){
                handle (*mouse_released, scene.camera);
            }
            else if(const auto* mouse_moved = event->getIf<sf::Event::MouseMoved> ()){
                handle (*mouse_moved, scene);
            }
        }

        scene.physics.update(clock.restart().asSeconds());

        scene.draw();
        window.display();
    }

    return 0;
}