// very simple default post effect to remove the dependency on HW gamma
// ramps. This allows us to overbrighten the scene even when windowed.

uniform sampler2DRect backBufferTex;

// the amount of gamma correction to apply to the scene ( 1.0f / gamma )
uniform float p_gammaRecip;
// number of times to multiply gamma corrected color by '2'
//(emulates the games overbright bits)
uniform float p_overbright;
// 0.0 = solid grey, 1.0 = no change, > 1.0 = more contrast
uniform float p_contrast;

const vec3 avgLuminance = vec3(0.5, 0.5, 0.5);

void main()
{
    vec4 backBuffer = texture2DRect( backBufferTex, gl_TexCoord[0].xy );
    vec3 gammaRecipVec = vec3(p_gammaRecip, p_gammaRecip, p_gammaRecip);
    vec3 gammaColor = p_overbright * pow( backBuffer.rgb, gammaRecipVec );

    vec3 contrastColor = mix( avgLuminance, gammaColor, p_contrast );

    gl_FragColor = vec4( contrastColor, 1.0 );
}
