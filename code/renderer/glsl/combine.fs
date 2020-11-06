uniform float p_bloomsaturation;
uniform float p_scenesaturation;
uniform float p_bloomintensity;
uniform float p_sceneintensity;

uniform sampler2DRect backBufferTex;
uniform sampler2DRect bloomTex;

const vec3 greyscale = vec3(0.3, 0.59, 0.11);

void main()
{
    vec4 backBuffer = texture2DRect( backBufferTex, gl_TexCoord[0].xy );
    vec4 bloom = texture2DRect( bloomTex, gl_TexCoord[1].xy);

    bloom = mix( vec4(dot(bloom.rgb, greyscale)), bloom, p_bloomsaturation);
    bloom *= p_bloomintensity;
    backBuffer *= (1.0 - clamp(bloom, 0.0, 1.0));
    backBuffer = mix( vec4(dot(backBuffer.rgb, greyscale)) , backBuffer, p_scenesaturation);
    gl_FragColor = (backBuffer * p_sceneintensity) + bloom;
}
