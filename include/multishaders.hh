#ifndef HOT_SHADERS_HH
#define HOT_SHADERS_HH

#include <string>
#include <fstream>
#include <filesystem>
#include <map>



namespace fcg
{

    // returns a C++ string loaded with the contents of a whole file
    inline std::string read_file (const std::string filename)
    {
        // open file
        std::ifstream file (filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error. Failed to open file: " << filename << std::endl;
            exit (1);
        }

        // Reserve space according to size
        std::string s;
        s.reserve (std::filesystem::file_size (filename));

        // Read file content
        s.assign (std::istreambuf_iterator<char> (file),
                  std::istreambuf_iterator<char> ());

        return s;
    }



    // get OpenGL log errors when compiling or linking shaders
    using glGetIv_func = void (*) (GLuint, GLenum, GLint*);
    using glGetInfoLog_func = void (*) (GLuint, GLsizei, GLsizei*, GLchar*);
    inline std::string getInfoLog(GLuint object, 
                                  glGetIv_func get_iv,
                                  glGetInfoLog_func get_infolog)
    {
        // get length
        GLint loglen = 0;
        get_iv (object, GL_INFO_LOG_LENGTH, &loglen);

        if (loglen == 0)
            return std::string ();

        // reserve size and retrieve
        std::string info_log;
        info_log.resize (loglen);
        get_infolog (object, loglen, nullptr, info_log.data());

        // Remove null terminator
        if (!info_log.empty() && info_log.back() == '\0') {
            info_log.pop_back ();
        }

        return info_log;
    }




    class Shaders
    {
    private:
        
        struct Program{
            GLuint id;
            std::map<std::string, GLint> locations;
        };

        std::map<std::string, Program> programs;
        Program* current = nullptr;

    public:

        ~Shaders () {
            clean ();
        }    

        void add(const std::string& name, const std::string& vertex_file, const std::string& fragment_file)
        {
            GLuint id = load(vertex_file,fragment_file);
            if(id==0){
                std::cerr << "Error compiling shaders" << std::endl;
                exit(1);
            }
            Program p;
            p.id = id;
            fetch_locs(p);
            programs[name] = p;
        }

        void set(const std::string& name, const glm::mat4& m)
        {
            GLint l = loc(name);
            if(l!=-1) glUniformMatrix4fv (l,1,GL_FALSE,&m[0][0]);
        }

        void set(const std::string& name, const glm::mat3& m)
        {
            GLint l = loc(name);
            if(l!=-1) glUniformMatrix3fv (l,1,GL_FALSE,&m[0][0]);
        }

        void set(const std::string& name, const glm::vec3& v)
        {
            GLint l = loc(name);
            if(l!=-1) glUniform3fv (l,1,&v[0]);
        }

        void set(const std::string& name, float f)
        {
            GLint l = loc(name);
            if(l!=-1) glUniform1f (l,f);
        }


        GLuint load (const std::string vertex_file, const std::string fragment_file)
        {
            const std::string vertex_string = read_file (vertex_file);
            const std::string fragment_string = read_file (fragment_file);
            const char* vertex_source = vertex_string.c_str ();
            const char* fragment_source = fragment_string.c_str ();

            GLuint id;
            if (!compile_attach_link (&vertex_source, &fragment_source, id)) {
                return 0;
            }

            return id;
        }


        void clean ()
        {
            for(auto& p : programs)
                glDeleteProgram (p.second.id);
            programs.clear();
            current = nullptr;
        }

        bool compile_attach_link (const char** vertex_source_ptr,
                                  const char** fragment_source_ptr,
                                  GLuint& new_id)
        {
            int params = false;

            // copmile vertex shader
            GLuint vertex = glCreateShader (GL_VERTEX_SHADER);
            glShaderSource (vertex, 1, vertex_source_ptr, NULL);
            glCompileShader (vertex);
            // check for errors
            params = -1;
            glGetShaderiv (vertex, GL_COMPILE_STATUS, &params);
            if (!params) {
                std::cerr << "Error compiling vertex shader: "
                          << getInfoLog (vertex, glGetShaderiv, glGetShaderInfoLog)
                          << std::endl;
                return false;
            }

            // compile fragment shader
            GLuint fragment = glCreateShader (GL_FRAGMENT_SHADER);
            glShaderSource (fragment, 1, fragment_source_ptr, NULL);
            glCompileShader (fragment);
            // check for errors
            glGetShaderiv (fragment, GL_COMPILE_STATUS, &params);
            if (!params) {
                std::cerr << "Error compiling fragment shader: "
                          << getInfoLog (fragment, glGetShaderiv, glGetShaderInfoLog)
                          << std::endl;
                return false;
            }

            // attach & link
            new_id = glCreateProgram ();
            glAttachShader (new_id, fragment);
            glAttachShader (new_id, vertex);
            glLinkProgram (new_id);
            // check for errors
            glGetProgramiv (new_id, GL_LINK_STATUS, &params);
            if (!params) {
                std::cerr << "Error linking shaders: "
                          << getInfoLog (new_id, glGetProgramiv, glGetProgramInfoLog)
                          << std::endl;
                return false;
            }

            // once shaders are attached, they can be deleted to free up memory
            glDeleteShader (vertex);
            glDeleteShader (fragment);

            return true;
        }

        void use (const std::string& name)
        {
            auto it = programs.find(name);
            if(it == programs.end()){
                std::cerr << "Errore shader program non trovato" << name << std::endl;
            }
            current = &it->second;
            glUseProgram(current->id);
        }

        // if the need to stop the program arises, this is how to do it
        // void stop ()
        // {
        //     glUseProgram (0);
        // }
    private:

        GLint loc(const std::string& name)
        {
            if(!current) return -1;
            auto it = current->locations.find(name);
            if(it == current->locations.end()) return -1;
            return  it->second;
        }


        void fetch_locs(Program& p)
        {
            static const char* names[] = {
                "model", "vp", "tr_inv_model", "camera_pos",
                "light.direct_pos", "light.direct_val", "light.ambient_val",
                "material.diffuse", "material.ambient", "material.specular", "material.shininess"
            };

            for (auto n : names)
                p.locations[n] =glGetUniformLocation(p.id, n);
        }
    };
    

}

#endif
