#version 410 core

uniform vec3 camera_pos;

struct Light{
    vec3 direct_pos;
    vec3 direct_val;
    vec3 ambient_val;
};
uniform Light light;

struct Material{
    vec3 diffuse;
    vec3 ambient;
    vec3 specular;
    float shininess;
};
uniform Material material;

uniform bool cut_pocket;

in vec3 interpolated_pos;
in vec3 interpolated_normal;
in vec3 obj_pos;
uniform bool isStriped;

out vec4 frag_colour;

void main (void) {
    vec3 pos = interpolated_pos;
    vec3 normal = normalize(interpolated_normal);
    if(!gl_FrontFacing) normal = -normal;

    if(cut_pocket && interpolated_pos.y > 0){
        if(abs(interpolated_pos.x) < 0.5 && abs(interpolated_pos.z) < 0.25){
            discard;
        }
    }

    float stripeHeight = abs(normalize(obj_pos).y);
    bool striped = isStriped && stripeHeight > 0.70;
    vec3 diffuse_aux = striped ? vec3(1.0) : material.diffuse;
    vec3 ambient_aux = striped ? vec3(0.25) : material.ambient;

    vec3 ambient = ambient_aux * light.ambient_val;

    vec3 light_dir = normalize (light.direct_pos - pos);
    float diff = max (dot (normal,light_dir), 0.0);
    vec3 diffuse = diffuse_aux * diff * light.direct_val;

    vec3 view_dir = normalize (camera_pos - pos);
    vec3 reflect_dir = reflect (-light_dir, normal);
    float spec = pow (max (dot (view_dir, reflect_dir), 0.0), material.shininess);
    vec3 specular = material.specular * spec * light.direct_val;

    frag_colour = vec4 (ambient + diffuse + specular, 1.0);
}