uniform float4x4 ViewProj;
uniform texture2d image;

uniform float4x4 yuv_mat = { 0.182586,  0.614231,  0.062007, 0.062745,
                            -0.100644, -0.338572,  0.439216, 0.501961,
                             0.439216, -0.398942, -0.040274, 0.501961,
                             0.000000,  0.000000,  0.000000, 1.000000};

uniform float4 color;
uniform float contrast;
uniform float brightness;
uniform float gamma;

uniform float2 chroma_key;
uniform float2 pixel_size;
uniform float similarity;
uniform float smoothness;
uniform float spill;

sampler_state textureSampler {
	Filter    = Linear;
	AddressU  = Clamp;
	AddressV  = Clamp;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv  : TEXCOORD0;
};

VertData VSDefault(VertData v_in)
{
	VertData vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv  = v_in.uv;
	return vert_out;
}

float4 CalcColor(float4 rgba)
{
	return float4(pow(rgba.rgb, float3(gamma, gamma, gamma)) * contrast + brightness, rgba.a);
}

float GetChromaDist(float3 rgb)
{
	float4 yuvx = mul(float4(rgb.rgb, 1.0), yuv_mat);
	return distance(chroma_key, yuvx.yz);
}

float4 SampleTexture(float2 uv)
{
	return image.Sample(textureSampler, uv);
}

float GetBoxFilteredChromaDist(float3 rgb, float2 texCoord)
{
	float2 h_pixel_size = pixel_size / 2.0;
	float2 point_0 = float2(pixel_size.x, h_pixel_size.y);
	float2 point_1 = float2(h_pixel_size.x, -pixel_size.y);
	float distVal = GetChromaDist(SampleTexture(texCoord-point_0).rgb);
	distVal += GetChromaDist(SampleTexture(texCoord+point_0).rgb);
	distVal += GetChromaDist(SampleTexture(texCoord-point_1).rgb);
	distVal += GetChromaDist(SampleTexture(texCoord+point_1).rgb);
	distVal *= 2.0;
	distVal += GetChromaDist(rgb);
	return distVal / 9.0;
}

float4 ProcessChromaKey(float4 rgba, VertData v_in)
{
	float chromaDist = GetBoxFilteredChromaDist(rgba.rgb, v_in.uv);
	float baseMask = chromaDist - similarity;
	float fullMask = pow(saturate(baseMask / smoothness), 1.5);
	float spillVal = pow(saturate(baseMask / spill), 1.5);

	rgba.rgba *= color;
	rgba.a *= fullMask;

	float desat = (rgba.r * 0.2126 + rgba.g * 0.7152 + rgba.b * 0.0722);
	rgba.rgb = saturate(float3(desat, desat, desat)) * (1.0 - spillVal) + rgba.rgb * spillVal;

	return CalcColor(rgba);
}

float4 PSChromaKeyRGBA(VertData v_in) : TARGET
{
	float4 rgba = image.Sample(textureSampler, v_in.uv);
	return ProcessChromaKey(rgba, v_in);
}

technique Draw
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader  = PSChromaKeyRGBA(v_in);
	}
}
