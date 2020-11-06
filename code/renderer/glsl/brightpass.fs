uniform sampler2DRect backBufferTex;
uniform float p_brightthreshold;

void main(void)
{
	vec3 sample = texture2DRect( backBufferTex, gl_TexCoord[0].xy ).rgb;
	gl_FragColor.rgb = clamp((sample - p_brightthreshold) / (1.0 - p_brightthreshold), 0.0, 1.0);
}
