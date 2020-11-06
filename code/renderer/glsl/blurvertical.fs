uniform sampler2DRect backBufferTex;

void main()
{
	vec4 result = vec4(0.0);
	vec2 texCoord = gl_TexCoord[0].xy + vec2( 0.0, -5.0);
	result += 0.042557 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.056743 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.075657 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.100876 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.134501 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.179335 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.134501 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.100876 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.075657 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.056743 * texture2DRect(backBufferTex, texCoord);

	texCoord.y += 1.0;
	result += 0.042557 * texture2DRect(backBufferTex, texCoord);

	result.a = 1.0;
	gl_FragColor = result;

}
