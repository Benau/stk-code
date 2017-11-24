uniform sampler2D tex;

uniform float density;
uniform vec3 col;

out vec4 FragColor;


#stk_include "utils/getPosFromUVDepth.frag"

void main()
{
    vec2 uv = gl_FragCoord.xy / u_screen;
    float z = texture(tex, uv).x;
    vec4 xpos = getPosFromUVDepth(vec3(uv, z), u_inverse_projection_matrix);

    float dist = length(xpos.xyz);
    float factor = (1. - exp(- density * dist));
    vec3 fog = col * factor;

    // fog is scattering component, factor is the beer lambert absorption
    FragColor = vec4(fog, factor);
}
