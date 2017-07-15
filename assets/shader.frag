uniform sampler2D diffuseTex;

varying vec3 normal;
varying vec2 texcoord;

void main(void)
{
    gl_FragColor = texture2D(diffuseTex, texcoord);
    //gl_FragColor = vec4(0.5 * normalize(normal) + 0.5, 1.0);
}
