uniform sampler2DRect backBufferTex;

void main()
{
	//vec4 color = texture2DRect(backBufferTex, gl_TexCoord[0].xy * 2.0 );
	vec4 color = texture2DRect(backBufferTex, gl_TexCoord[1].xy );
	gl_FragColor = color;
}
